#include "BluezBatteryProvider.h"

#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#include <QStringList>
#include <QVariant>

#include "Logging.h"

// ── BatteryProviderObject ────────────────────────────────────────────────────

BatteryProviderObject::BatteryProviderObject(QDBusObjectPath device,
                                             QString deviceType,
                                             uint8_t percentage,
                                             QObject* parent)
    : QObject(parent)
    , device_(std::move(device))
    , deviceType_(std::move(deviceType))
    , percentage_(percentage)
{}

QVariantMap BatteryProviderObject::propertyMap() const {
    QVariantMap props;
    props[QStringLiteral("Device")] = QVariant::fromValue(device_);
    props[QStringLiteral("Percentage")] = QVariant::fromValue<uchar>(percentage_);
    props[QStringLiteral("Source")] = source();
    return props;
}

// ── ObjectManagerAdaptor ─────────────────────────────────────────────────────

ObjectManagerAdaptor::ObjectManagerAdaptor(BluezBatteryProvider* parent)
    : QDBusAbstractAdaptor(parent)
    , provider_(parent)
{
    // We do NOT call setAutoRelaySignals(true): the provider's Qt signals use
    // lowercase-leading names (interfacesAdded/Removed) while the D-Bus-facing
    // adaptor signals use the capital-leading wire names BlueZ expects
    // (InterfacesAdded/Removed). Auto-relay matches by name+signature and
    // would find nothing, so the explicit QObject::connects below are the
    // actual relay — which means enabling auto-relay also would have been
    // dead code at best and a doubled signal at worst.
    QObject::connect(provider_, &BluezBatteryProvider::interfacesAdded,
                     this, &ObjectManagerAdaptor::InterfacesAdded);
    QObject::connect(provider_, &BluezBatteryProvider::interfacesRemoved,
                     this, &ObjectManagerAdaptor::InterfacesRemoved);
}

ManagedObjectList ObjectManagerAdaptor::GetManagedObjects() {
    return provider_->managedObjects();
}

// ── BluezBatteryProvider ─────────────────────────────────────────────────────

BluezBatteryProvider::BluezBatteryProvider(QObject* parent)
    : QObject(parent)
    , bus_(QDBusConnection::systemBus())
    , objectManagerAdaptor_(new ObjectManagerAdaptor(this))
{
    if (!bus_.isConnected()) {
        qCWarning(lcDbus) << "system bus not available; battery provider disabled";
        return;
    }

    // Export the provider root so bluez can call GetManagedObjects on us
    // after RegisterBatteryProvider. ExportAdaptors picks up the
    // ObjectManagerAdaptor we just attached.
    if (!bus_.registerObject(QString::fromLatin1(kProviderPath), this,
                             QDBusConnection::ExportAdaptors)) {
        qCWarning(lcDbus) << "failed to register provider root at" << kProviderPath
                          << ":" << bus_.lastError().message();
        return;
    }
    qCDebug(lcDbus) << "provider root exported at" << kProviderPath;
}

BluezBatteryProvider::~BluezBatteryProvider() {
    clear();
    if (bus_.isConnected()) {
        bus_.unregisterObject(QString::fromLatin1(kProviderPath));
    }
}

ManagedObjectList BluezBatteryProvider::managedObjects() const {
    ManagedObjectList result;
    if (child_) {
        InterfaceProperties ifaces;
        ifaces[QString::fromLatin1(kBatteryProvider1Iface)] = child_->propertyMap();
        result[QDBusObjectPath(QString::fromLatin1(kChildPath))] = ifaces;
    }
    return result;
}

void BluezBatteryProvider::publish(const QString& mac, const QString& deviceType,
                                   uint8_t percentage) {
    if (!bus_.isConnected()) return;

    // Normalize MAC to uppercase for storage and comparison. BlueZ returns
    // Device1.Address in uppercase, and callers may pass either case from
    // BMAP's connection state — keeping them in one canonical form prevents
    // the fast-path compare from false-negativing on case differences and
    // forcing a re-registration cycle every poll.
    const QString normalizedMac = mac.toUpper();

    // If this is a new device (or our first publish), resolve paths, export
    // the child, and register with bluez. Otherwise just update percentage
    // and notify via PropertiesChanged.
    if (normalizedMac == currentMac_ && child_ && !registeredAdapter_.path().isEmpty()) {
        if (child_->percentage() == percentage) return;  // no-op
        child_->setPercentage(percentage);
        emitPercentageChanged(percentage);
        qCDebug(lcDbus) << "battery update:" << percentage << "% for" << normalizedMac;
        return;
    }

    // Device changed (or first publish) — tear down any stale state first.
    clear();

    QDBusObjectPath adapterPath;
    QDBusObjectPath devicePath;
    if (!resolveBluezPaths(normalizedMac, adapterPath, devicePath)) {
        qCWarning(lcDbus) << "could not resolve bluez paths for" << normalizedMac
                          << "; battery will not be published system-wide";
        return;
    }

    exportChild(devicePath, deviceType, percentage);
    if (!registerWithBluez(adapterPath)) {
        unexportChild();
        return;
    }
    // Only commit state after every step succeeds so a mid-publish failure
    // leaves us fully idle (no half-registered registration, no stale
    // currentMac_ that would block a retry on the next poll).
    currentMac_ = normalizedMac;
    registeredAdapter_ = adapterPath;
    qCInfo(lcDbus) << "registered battery provider for" << normalizedMac
                   << "(" << deviceType << ")"
                   << "on adapter" << adapterPath.path()
                   << "at" << percentage << "%";
}

void BluezBatteryProvider::clear() {
    if (!bus_.isConnected()) return;
    if (!registeredAdapter_.path().isEmpty()) {
        unregisterFromBluez();
    }
    unexportChild();
    currentMac_.clear();
    registeredAdapter_ = QDBusObjectPath();
}

bool BluezBatteryProvider::resolveBluezPaths(const QString& mac,
                                             QDBusObjectPath& adapterPath,
                                             QDBusObjectPath& devicePath) {
    // Walk org.freedesktop.DBus.ObjectManager on bluez's root to find the
    // Device1 whose Address matches our MAC, then read its Adapter property.
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kBluezService),
        QStringLiteral("/"),
        QString::fromLatin1(kObjectManagerIface),
        QStringLiteral("GetManagedObjects"));
    QDBusMessage reply = bus_.call(call, QDBus::Block, kDbusCallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcDbus) << "GetManagedObjects on bluez failed:" << reply.errorMessage();
        return false;
    }

    // Demarshal the reply into our ManagedObjectList type.
    const QVariantList args = reply.arguments();
    if (args.isEmpty()) return false;
    const QDBusArgument dbusArg = args.first().value<QDBusArgument>();
    ManagedObjectList managed;
    dbusArg >> managed;

    const QString upperMac = mac.toUpper();
    for (auto it = managed.constBegin(); it != managed.constEnd(); ++it) {
        const InterfaceProperties& ifaces = it.value();
        const auto devIter = ifaces.find(QString::fromLatin1(kDevice1Iface));
        if (devIter == ifaces.end()) continue;
        const QVariantMap& props = devIter.value();
        const QString addr = props.value(QStringLiteral("Address")).toString().toUpper();
        if (addr != upperMac) continue;
        devicePath = it.key();
        const QVariant adapter = props.value(QStringLiteral("Adapter"));
        adapterPath = adapter.value<QDBusObjectPath>();
        qCDebug(lcDbus) << "resolved" << mac
                        << "→ device" << devicePath.path()
                        << "adapter" << adapterPath.path();
        return !adapterPath.path().isEmpty();
    }
    return false;
}

void BluezBatteryProvider::exportChild(const QDBusObjectPath& devicePath,
                                       const QString& deviceType,
                                       uint8_t percentage) {
    child_ = new BatteryProviderObject(devicePath, deviceType, percentage, this);
    // ExportAllProperties alone is sufficient: org.bluez.BatteryProvider1 is
    // a properties-only interface, so the child has no slots to export. The
    // Q_CLASSINFO("D-Bus Interface", ...) on BatteryProviderObject makes the
    // properties appear under that interface name, which `busctl introspect
    // org.bluez /org/bluez/hci0/dev_<MAC>` confirms by showing the
    // re-exported org.bluez.Battery1.Percentage. Dropping ExportAllSlots
    // prevents any future slot additions from leaking onto the bus.
    if (!bus_.registerObject(QString::fromLatin1(kChildPath), child_,
                             QDBusConnection::ExportAllProperties)) {
        qCWarning(lcDbus) << "failed to export battery child at" << kChildPath
                          << ":" << bus_.lastError().message();
        delete child_;
        child_ = nullptr;
        return;
    }
    // Let ObjectManager subscribers know a new object appeared.
    InterfaceProperties ifaces;
    ifaces[QString::fromLatin1(kBatteryProvider1Iface)] = child_->propertyMap();
    emit interfacesAdded(QDBusObjectPath(QString::fromLatin1(kChildPath)), ifaces);
}

void BluezBatteryProvider::unexportChild() {
    if (!child_) return;
    emit interfacesRemoved(QDBusObjectPath(QString::fromLatin1(kChildPath)),
                           QStringList{QString::fromLatin1(kBatteryProvider1Iface)});
    bus_.unregisterObject(QString::fromLatin1(kChildPath));
    delete child_;
    child_ = nullptr;
}

bool BluezBatteryProvider::registerWithBluez(const QDBusObjectPath& adapterPath) {
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kBluezService),
        adapterPath.path(),
        QString::fromLatin1(kBatteryProviderMgr1Iface),
        QStringLiteral("RegisterBatteryProvider"));
    call << QVariant::fromValue(QDBusObjectPath(QString::fromLatin1(kProviderPath)));
    QDBusMessage reply = bus_.call(call, QDBus::Block, kDbusCallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcDbus) << "RegisterBatteryProvider on" << adapterPath.path()
                          << "failed:" << reply.errorMessage();
        return false;
    }
    return true;
}

void BluezBatteryProvider::unregisterFromBluez() {
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kBluezService),
        registeredAdapter_.path(),
        QString::fromLatin1(kBatteryProviderMgr1Iface),
        QStringLiteral("UnregisterBatteryProvider"));
    call << QVariant::fromValue(QDBusObjectPath(QString::fromLatin1(kProviderPath)));
    QDBusMessage reply = bus_.call(call, QDBus::Block, kDbusCallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcDbus) << "UnregisterBatteryProvider failed:" << reply.errorMessage();
    } else {
        qCInfo(lcDbus) << "unregistered battery provider from" << registeredAdapter_.path();
    }
}

void BluezBatteryProvider::emitPercentageChanged(uint8_t percentage) {
    // Emit org.freedesktop.DBus.Properties.PropertiesChanged for the child
    // object so bluez (and UPower behind it) picks up the new reading.
    QDBusMessage sig = QDBusMessage::createSignal(
        QString::fromLatin1(kChildPath),
        QString::fromLatin1(kPropertiesIface),
        QStringLiteral("PropertiesChanged"));
    QVariantMap changed;
    changed[QStringLiteral("Percentage")] = QVariant::fromValue<uchar>(percentage);
    sig << QString::fromLatin1(kBatteryProvider1Iface);
    sig << changed;
    sig << QStringList();  // invalidated
    bus_.send(sig);
}
