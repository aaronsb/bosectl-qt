#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <cstdint>

// D-Bus a{sa{sv}} — interface name to property map.
using InterfaceProperties = QMap<QString, QVariantMap>;
// D-Bus a{oa{sa{sv}}} — object path to interfaces map. Used by ObjectManager.
using ManagedObjectList = QMap<QDBusObjectPath, InterfaceProperties>;

Q_DECLARE_METATYPE(InterfaceProperties)
Q_DECLARE_METATYPE(ManagedObjectList)

// Forward decls for the internal adaptors.
class ObjectManagerAdaptor;
class BatteryProviderObject;

// Publishes the currently connected headphone's battery percentage to BlueZ
// via the org.bluez.BatteryProviderManager1 API so system power indicators
// (UPower → GNOME/KDE/etc.) can surface it. See docs/architecture/0001.
//
// Lifecycle:
//   publish(mac, pct) — called from TrayIcon on every connected status update.
//                       First call triggers registration with bluez; subsequent
//                       calls just update the percentage and emit
//                       PropertiesChanged.
//   clear()           — called on disconnect. Unregisters from bluez.
//
// Failure posture: any step (bluez missing, adapter lookup failed, register
// call rejected) logs a warning on the bosectl.dbus category and leaves the
// provider in an idle state. The app continues to work; the system indicator
// just won't learn about the battery.
class BluezBatteryProvider : public QObject {
    Q_OBJECT

public:
    explicit BluezBatteryProvider(QObject* parent = nullptr);
    ~BluezBatteryProvider() override;

    // Called by the ObjectManager adaptor. Returns the currently-exported
    // battery child, if any, in the shape BlueZ expects.
    ManagedObjectList managedObjects() const;

public slots:
    // Publish a battery reading for the given MAC. First successful call
    // triggers registration with bluez. Idempotent for repeated same values.
    // `deviceType` (e.g. "qc_ultra2") is surfaced on the BatteryProvider1
    // Source property so the real product identifier lives next to the
    // battery in the D-Bus tree regardless of any user rename.
    void publish(const QString& mac, const QString& deviceType, uint8_t percentage);

    // Drop the registration (if any) and forget the current device.
    void clear();

signals:
    // Emitted by the ObjectManager adaptor when a child is added/removed so
    // bluez can track our exported battery provider objects dynamically.
    void interfacesAdded(const QDBusObjectPath& path,
                         const InterfaceProperties& interfaces);
    void interfacesRemoved(const QDBusObjectPath& path,
                           const QStringList& interfaces);

private:
    // Resolve the bluez adapter + device object paths for the given MAC by
    // walking org.freedesktop.DBus.ObjectManager.GetManagedObjects on bluez.
    // Returns true on success and fills adapterPath/devicePath.
    bool resolveBluezPaths(const QString& mac,
                           QDBusObjectPath& adapterPath,
                           QDBusObjectPath& devicePath);

    // Register our provider root with bluez on the given adapter.
    bool registerWithBluez(const QDBusObjectPath& adapterPath);
    void unregisterFromBluez();

    // Export / unexport the child BatteryProvider1 object at childPath_.
    void exportChild(const QDBusObjectPath& devicePath,
                     const QString& deviceType, uint8_t percentage);
    void unexportChild();

    // Emit org.freedesktop.DBus.Properties.PropertiesChanged for the child
    // object so bluez picks up percentage updates.
    void emitPercentageChanged(uint8_t percentage);

    QDBusConnection bus_;
    ObjectManagerAdaptor* objectManagerAdaptor_;  // parented to `this`
    BatteryProviderObject* child_ = nullptr;
    QString currentMac_;
    QDBusObjectPath registeredAdapter_;  // empty if not registered

    static constexpr const char* kProviderPath = "/com/bockelie/bosectl_qt/battery";
    static constexpr const char* kChildPath = "/com/bockelie/bosectl_qt/battery/dev";
    static constexpr const char* kBluezService = "org.bluez";
    static constexpr const char* kBatteryProvider1Iface = "org.bluez.BatteryProvider1";
    static constexpr const char* kBatteryProviderMgr1Iface = "org.bluez.BatteryProviderManager1";
    static constexpr const char* kObjectManagerIface = "org.freedesktop.DBus.ObjectManager";
    static constexpr const char* kPropertiesIface = "org.freedesktop.DBus.Properties";
    static constexpr const char* kDevice1Iface = "org.bluez.Device1";
};

// Adaptor that exposes org.freedesktop.DBus.ObjectManager on the provider
// root. BlueZ calls GetManagedObjects() on registration and subscribes to
// InterfacesAdded/Removed to track the child battery provider object.
class ObjectManagerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.ObjectManager")

public:
    explicit ObjectManagerAdaptor(BluezBatteryProvider* parent);

public slots:
    ManagedObjectList GetManagedObjects();

signals:
    void InterfacesAdded(const QDBusObjectPath& path,
                         const InterfaceProperties& interfaces);
    void InterfacesRemoved(const QDBusObjectPath& path,
                           const QStringList& interfaces);

private:
    BluezBatteryProvider* provider_;
};

// The actual battery provider object, one per currently-connected device.
// Exported at kChildPath with the org.bluez.BatteryProvider1 interface.
class BatteryProviderObject : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.BatteryProvider1")
    Q_PROPERTY(QDBusObjectPath Device READ device)
    Q_PROPERTY(uchar Percentage READ percentage)
    Q_PROPERTY(QString Source READ source)

public:
    BatteryProviderObject(QDBusObjectPath device, QString deviceType,
                          uint8_t percentage, QObject* parent = nullptr);

    QDBusObjectPath device() const { return device_; }
    uchar percentage() const { return percentage_; }
    // Includes the BMAP deviceType (e.g. "qc_ultra2") so the real model
    // identifier is visible to anything introspecting the D-Bus tree,
    // regardless of what the user may have renamed the device to.
    QString source() const {
        return deviceType_.isEmpty()
            ? QStringLiteral("bosectl-qt (BMAP)")
            : QStringLiteral("bosectl-qt (%1)").arg(deviceType_);
    }

    void setPercentage(uint8_t p) { percentage_ = p; }

    // Build the property map bluez expects for this object (used by both
    // managedObjects() and the PropertiesChanged signal).
    QVariantMap propertyMap() const;

private:
    QDBusObjectPath device_;
    QString deviceType_;
    uint8_t percentage_ = 0;
};
