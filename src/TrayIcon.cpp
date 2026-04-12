#include "TrayIcon.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QMessageBox>

TrayIcon::TrayIcon(QObject* parent)
    : QSystemTrayIcon(parent)
    , menu_(new QMenu)
    , ncWindow_(new NcWindow)
    , modeWindow_(new ModeWindow)
    , eqWindow_(new EqWindow)
    , worker_(new BmapWorker)
    , pollTimer_(new QTimer(this))
{
    setIcon(QIcon(":/bosectl-qt.svg"));
    setToolTip("bosectl - Disconnected");

    buildMenu();
    setContextMenu(menu_);

    // Move worker to its own thread
    worker_->moveToThread(&workerThread_);
    connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);

    // Worker signals
    connect(worker_, &BmapWorker::busy, this, [this](bool working) {
        if (working) {
            setToolTip(toolTip().split(" [")[0] + " [working...]");
        } else {
            setToolTip(toolTip().split(" [")[0]);
        }
        ncWindow_->setBusy(working);
        eqWindow_->setBusy(working);
        modeWindow_->setBusy(working);
    });
    connect(worker_, &BmapWorker::statusReady, this, &TrayIcon::onStatusReady);
    connect(worker_, &BmapWorker::modesReady, this, &TrayIcon::onModesReady);
    connect(worker_, &BmapWorker::modeDetailsReady, this, &TrayIcon::onModeDetailsReady);
    connect(worker_, &BmapWorker::error, this, &TrayIcon::onError);
    connect(worker_, &BmapWorker::disconnected, this, &TrayIcon::onDisconnected);

    // EQ window signals
    connect(eqWindow_, &EqWindow::eqTry, this, [this](int8_t b, int8_t m, int8_t t) {
        QMetaObject::invokeMethod(worker_, "setEq", Qt::QueuedConnection,
                                  Q_ARG(int8_t, b), Q_ARG(int8_t, m), Q_ARG(int8_t, t));
    });
    connect(eqWindow_, &EqWindow::eqSave, this, [this](int8_t b, int8_t m, int8_t t) {
        settings_.setEqBass(b);
        settings_.setEqMid(m);
        settings_.setEqTreble(t);
        QMetaObject::invokeMethod(worker_, "setEq", Qt::QueuedConnection,
                                  Q_ARG(int8_t, b), Q_ARG(int8_t, m), Q_ARG(int8_t, t));
    });

    // NC window signal
    connect(ncWindow_, &NcWindow::cncChanged, this, [this](int val) {
        settings_.setLastCnc(val);
        QMetaObject::invokeMethod(worker_, "setCnc", Qt::QueuedConnection,
                                  Q_ARG(uint8_t, static_cast<uint8_t>(val)));
    });

    // Mode window signals
    connect(modeWindow_, &ModeWindow::activateMode, this, [this](const QString& name) {
        QMetaObject::invokeMethod(worker_, "activateMode", Qt::QueuedConnection,
                                  Q_ARG(QString, name));
    });
    connect(modeWindow_, &ModeWindow::saveMode, this,
            [this](uint8_t idx, const QString& name, uint8_t cnc,
                   uint8_t spatial, bool wind, bool anc) {
        QMetaObject::invokeMethod(worker_, "saveMode", Qt::QueuedConnection,
                                  Q_ARG(uint8_t, idx), Q_ARG(QString, name),
                                  Q_ARG(uint8_t, cnc), Q_ARG(uint8_t, spatial),
                                  Q_ARG(bool, wind), Q_ARG(bool, anc));
    });
    connect(modeWindow_, &ModeWindow::createMode, this,
            [this](const QString& name, uint8_t cnc,
                   uint8_t spatial, bool wind, bool anc) {
        // idx=255 signals "create new"
        QMetaObject::invokeMethod(worker_, "saveMode", Qt::QueuedConnection,
                                  Q_ARG(uint8_t, uint8_t(255)), Q_ARG(QString, name),
                                  Q_ARG(uint8_t, cnc), Q_ARG(uint8_t, spatial),
                                  Q_ARG(bool, wind), Q_ARG(bool, anc));
    });
    connect(modeWindow_, &ModeWindow::deleteMode, this, [this](const QString& name) {
        QMetaObject::invokeMethod(worker_, "deleteMode", Qt::QueuedConnection,
                                  Q_ARG(QString, name));
    });

    workerThread_.start();

    // Poll status
    connect(pollTimer_, &QTimer::timeout, this, [this] {
        if (lastState_.connected)
            QMetaObject::invokeMethod(worker_, "refresh", Qt::QueuedConnection);
    });
    pollTimer_->start(settings_.pollInterval() * 1000);

    // Auto-connect on startup
    if (settings_.autoConnect()) {
        QMetaObject::invokeMethod(worker_, "connectDevice", Qt::QueuedConnection,
                                  Q_ARG(QString, settings_.lastMac()),
                                  Q_ARG(QString, settings_.lastDeviceType()));
    }
}

TrayIcon::~TrayIcon() {
    workerThread_.quit();
    workerThread_.wait();
    delete menu_;
    delete ncWindow_;
    delete eqWindow_;
    delete modeWindow_;
}

void TrayIcon::buildMenu() {
    // ── Header ──────────────────────────────────────────────────────────────
    headerAction_ = menu_->addAction("Bose Headphones");
    QFont boldFont = headerAction_->font();
    boldFont.setBold(true);
    headerAction_->setFont(boldFont);

    batteryAction_ = menu_->addAction("Battery: --");

    // About submenu
    aboutMenu_ = menu_->addMenu("About");
    firmwareAction_ = aboutMenu_->addAction("Firmware: --");
    firmwareAction_->setEnabled(false);
    macAction_ = aboutMenu_->addAction("MAC: --");
    macAction_->setEnabled(false);
    aboutMenu_->addSeparator();
    aboutMenu_->addAction("About bosectl-qt...", this, [this] {
        QMessageBox about(QMessageBox::NoIcon, "About bosectl-qt",
            QString("<h3>bosectl-qt</h3>"
                    "<p>A Qt6 system tray application for controlling "
                    "Bose headphones over Bluetooth via the BMAP protocol.</p>"
                    "<p>Version 0.1.0</p>"
                    "<p>Built on the <a href=\"https://github.com/aaronsb/bosectl\">bosectl</a> "
                    "reverse-engineered BMAP library.</p>"
                    "<p>By Aaron Bockelie &lt;<a href=\"mailto:aaronsb@gmail.com\">"
                    "aaronsb@gmail.com</a>&gt;</p>"
                    "<p><a href=\"https://github.com/aaronsb/bosectl-qt\">"
                    "github.com/aaronsb/bosectl-qt</a></p>"),
            QMessageBox::Ok);
        about.setIconPixmap(QIcon(":/bosectl-qt.svg").pixmap(64, 64));
        about.setTextFormat(Qt::RichText);
        about.setTextInteractionFlags(Qt::TextBrowserInteraction);
        about.exec();
    });

    menu_->addSeparator();

    // ── Noise Cancellation (opens window) ──────────────────────────────────
    menu_->addAction("Noise Cancellation...", this, [this] {
        ncWindow_->show();
        ncWindow_->raise();
        ncWindow_->activateWindow();
    });

    // ── Mode (opens window) ─────────────────────────────────────────────────
    menu_->addAction("Modes...", this, [this] {
        // Fetch fresh mode data when opening
        QMetaObject::invokeMethod(worker_, "fetchModeDetails", Qt::QueuedConnection);
        modeWindow_->show();
        modeWindow_->raise();
        modeWindow_->activateWindow();
    });

    // ── Equalizer (opens window) ────────────────────────────────────────────
    menu_->addAction("Equalizer...", this, [this] {
        eqWindow_->show();
        eqWindow_->raise();
        eqWindow_->activateWindow();
    });

    // ── Spatial audio submenu ───────────────────────────────────────────────
    spatialMenu_ = menu_->addMenu("Spatial Audio");
    spatialGroup_ = new QActionGroup(this);
    spatialGroup_->setExclusive(true);
    for (const auto& [label, value] : std::vector<std::pair<QString, QString>>{
             {"Off", "off"}, {"Room", "room"}, {"Head Tracking", "head"}}) {
        auto* a = spatialMenu_->addAction(label);
        a->setCheckable(true);
        a->setData(value);
        spatialGroup_->addAction(a);
    }
    connect(spatialGroup_, &QActionGroup::triggered, this, &TrayIcon::onSpatialSelected);

    // ── Sidetone submenu ────────────────────────────────────────────────────
    sidetoneMenu_ = menu_->addMenu("Sidetone");
    sidetoneGroup_ = new QActionGroup(this);
    sidetoneGroup_->setExclusive(true);
    for (const auto& [label, value] : std::vector<std::pair<QString, QString>>{
             {"Off", "off"}, {"Low", "low"}, {"Medium", "medium"}, {"High", "high"}}) {
        auto* a = sidetoneMenu_->addAction(label);
        a->setCheckable(true);
        a->setData(value);
        sidetoneGroup_->addAction(a);
    }
    connect(sidetoneGroup_, &QActionGroup::triggered, this, &TrayIcon::onSidetoneSelected);

    menu_->addSeparator();

    // ── Toggles ─────────────────────────────────────────────────────────────
    ancAction_ = menu_->addAction("Noise Cancellation (ANC)");
    ancAction_->setCheckable(true);
    ancAction_->setToolTip("ANC must be on for the NC slider to take effect");
    connect(ancAction_, &QAction::toggled, this, &TrayIcon::onAncToggled);

    windAction_ = menu_->addAction("Wind Block");
    windAction_->setCheckable(true);
    windAction_->setToolTip("Wind Block overrides the NC slider");
    connect(windAction_, &QAction::toggled, this, &TrayIcon::onWindToggled);

    multipointAction_ = menu_->addAction("Multipoint");
    multipointAction_->setCheckable(true);
    connect(multipointAction_, &QAction::toggled, this, &TrayIcon::onMultipointToggled);

    autoPauseAction_ = menu_->addAction("Auto-Pause");
    autoPauseAction_->setCheckable(true);
    connect(autoPauseAction_, &QAction::toggled, this, &TrayIcon::onAutoPauseToggled);

    menu_->addSeparator();

    // ── Connection ──────────────────────────────────────────────────────────
    connectAction_ = menu_->addAction("Connect", this, &TrayIcon::onConnectClicked);
    powerOffAction_ = menu_->addAction("Power Off", this, &TrayIcon::onPowerOffClicked);
    powerOffAction_->setEnabled(false);
    menu_->addAction("Quit", qApp, &QApplication::quit);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void TrayIcon::onStatusReady(DeviceState state) {
    lastState_ = state;

    if (!state.connected) {
        setToolTip("bosectl - Disconnected");
        headerAction_->setText("Bose Headphones (disconnected)");
        batteryAction_->setText("Battery: --");
        firmwareAction_->setText("Firmware: --");
        macAction_->setText("MAC: --");
        connectAction_->setText("Connect");
        powerOffAction_->setEnabled(false);
        return;
    }

    // Save settings
    settings_.setLastMac(state.mac);
    settings_.setLastDeviceType(state.deviceType);
    settings_.setLastCnc(state.cncLevel);
    settings_.setLastMode(state.mode);
    settings_.setEqBass(state.eq.bass);
    settings_.setEqMid(state.eq.mid);
    settings_.setEqTreble(state.eq.treble);
    settings_.setLastSidetone(state.sidetone);
    settings_.setLastSpatial(state.spatial);

    setToolTip(QString("%1 - %2% - %3").arg(state.deviceName)
               .arg(state.battery).arg(state.mode));
    headerAction_->setText(state.deviceName);

    if (state.battery <= 15)
        batteryAction_->setText(QString("Battery: %1% \u26a0").arg(state.battery));
    else
        batteryAction_->setText(QString("Battery: %1%").arg(state.battery));

    firmwareAction_->setText(QString("Firmware: %1").arg(state.firmware));
    macAction_->setText(QString("MAC: %1").arg(state.mac));

    // NC window (only when not visible, to avoid fighting the user)
    if (!ncWindow_->isVisible()) {
        ncWindow_->setMaximum(state.cncMax);
        ncWindow_->setValue(state.cncLevel);
    }

    // Sidetone
    for (auto* a : sidetoneGroup_->actions())
        a->setChecked(a->data().toString() == state.sidetone);

    // Spatial
    for (auto* a : spatialGroup_->actions())
        a->setChecked(a->data().toString() == state.spatial);

    // Toggles
    ancAction_->blockSignals(true);
    ancAction_->setChecked(state.ancEnabled);
    ancAction_->blockSignals(false);

    windAction_->blockSignals(true);
    windAction_->setChecked(state.windBlock);
    windAction_->blockSignals(false);

    multipointAction_->blockSignals(true);
    multipointAction_->setChecked(state.multipoint);
    multipointAction_->blockSignals(false);

    autoPauseAction_->blockSignals(true);
    autoPauseAction_->setChecked(state.autoPause);
    autoPauseAction_->blockSignals(false);

    connectAction_->setText("Reconnect");
    powerOffAction_->setEnabled(true);

    // EQ window (only when not actively editing)
    if (!eqWindow_->isVisible()) {
        eqWindow_->setCurrentEq(state.eq.bass, state.eq.mid, state.eq.treble);
        eqWindow_->setSavedEq(state.eq.bass, state.eq.mid, state.eq.treble);
    }
}

void TrayIcon::onModesReady(QStringList) {
    // No longer used for the simple list; mode window uses modeDetailsReady
}

void TrayIcon::onModeDetailsReady(QList<ModeInfo> modes, uint8_t activeIdx) {
    modeWindow_->setModes(modes, activeIdx);
}

void TrayIcon::onError(QString message) {
    showMessage("bosectl", message, QSystemTrayIcon::Warning, 3000);
}

void TrayIcon::onDisconnected() {
    lastState_ = DeviceState{};
    onStatusReady(lastState_);
}

void TrayIcon::onSidetoneSelected(QAction* action) {
    auto val = action->data().toString();
    settings_.setLastSidetone(val);
    QMetaObject::invokeMethod(worker_, "setSidetone", Qt::QueuedConnection,
                              Q_ARG(QString, val));
}

void TrayIcon::onSpatialSelected(QAction* action) {
    auto val = action->data().toString();
    settings_.setLastSpatial(val);
    QMetaObject::invokeMethod(worker_, "setSpatial", Qt::QueuedConnection,
                              Q_ARG(QString, val));
}

void TrayIcon::onAncToggled(bool checked) {
    QMetaObject::invokeMethod(worker_, "setAnc", Qt::QueuedConnection,
                              Q_ARG(bool, checked));
}

void TrayIcon::onWindToggled(bool checked) {
    QMetaObject::invokeMethod(worker_, "setWind", Qt::QueuedConnection,
                              Q_ARG(bool, checked));
}

void TrayIcon::onMultipointToggled(bool checked) {
    QMetaObject::invokeMethod(worker_, "setMultipoint", Qt::QueuedConnection,
                              Q_ARG(bool, checked));
}

void TrayIcon::onAutoPauseToggled(bool checked) {
    QMetaObject::invokeMethod(worker_, "setAutoPause", Qt::QueuedConnection,
                              Q_ARG(bool, checked));
}

void TrayIcon::onConnectClicked() {
    QMetaObject::invokeMethod(worker_, "connectDevice", Qt::QueuedConnection,
                              Q_ARG(QString, settings_.lastMac()),
                              Q_ARG(QString, settings_.lastDeviceType()));
}

void TrayIcon::onPowerOffClicked() {
    QMetaObject::invokeMethod(worker_, "powerOff", Qt::QueuedConnection);
}
