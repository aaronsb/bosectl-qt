#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QThread>
#include <QTimer>

#include <memory>

#include "BluezBatteryProvider.h"
#include "BmapWorker.h"
#include "Settings.h"
#include "EqWindow.h"
#include "HelpWindow.h"
#include "ModeWindow.h"
#include "NcWindow.h"

class SimDevice;

class TrayIcon : public QSystemTrayIcon {
    Q_OBJECT

public:
    // With a SimDevice, the tray connects to it instead of Bluetooth and
    // publishes nothing to BlueZ.
    explicit TrayIcon(std::shared_ptr<SimDevice> sim = {}, QObject* parent = nullptr);
    ~TrayIcon() override;

    // Poll the device now rather than at the next timer tick.
    void refresh();

private slots:
    void onStatusReady(DeviceState state);
    void onModesReady(QStringList modes);
    void onModeDetailsReady(QList<ModeInfo> modes, uint8_t activeIdx);
    void onError(QString message);
    void onDisconnected();
    void onSidetoneSelected(QAction* action);
    void onSpatialSelected(QAction* action);
    void onMultipointToggled(bool checked);
    void onAutoPauseToggled(bool checked);
    void onAncToggled(bool checked);
    void onWindToggled(bool checked);
    void onConnectClicked();
    void onPowerOffClicked();
    void onStartOnLoginToggled(bool checked);

private:
    void buildMenu();
    void updateTooltip();
    void showStatusNotification();
    QStringList statusLines() const;

    QMenu* menu_;
    QMenu* headerMenu_;
    QAction* renameAction_;
    QAction* batteryAction_;

    QMenu* aboutMenu_;
    QAction* firmwareAction_;
    QAction* macAction_;

    NcWindow* ncWindow_;
    ModeWindow* modeWindow_;
    EqWindow* eqWindow_;
    HelpWindow* helpWindow_ = nullptr;   // built on first open

    // Hidden widget used solely as a parent for transient modal dialogs
    // (e.g. rename). TrayIcon itself is a QObject, not a QWidget, so dialogs
    // spawned from menu actions would otherwise be parentless. Never shown.
    QWidget* dialogAnchor_;

    QMenu* sidetoneMenu_;
    QActionGroup* sidetoneGroup_;

    QMenu* spatialMenu_;
    QActionGroup* spatialGroup_;

    QAction* multipointAction_;
    QAction* autoPauseAction_;
    QAction* ancAction_;
    QAction* windAction_;

    QAction* connectAction_;
    QAction* powerOffAction_;
    QAction* startOnLoginAction_;

    QThread workerThread_;
    BmapWorker* worker_;
    QTimer* pollTimer_;
    BluezBatteryProvider* batteryProvider_ = nullptr;   // none in sim mode

    Settings settings_;
    DeviceState lastState_;
    bool reconnectInFlight_ = false;
    bool workerBusy_ = false;
};
