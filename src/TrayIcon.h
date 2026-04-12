#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QThread>
#include <QTimer>

#include "BmapWorker.h"
#include "Settings.h"
#include "EqWindow.h"
#include "ModeWindow.h"
#include "NcWindow.h"

class TrayIcon : public QSystemTrayIcon {
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

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

private:
    void buildMenu();

    QMenu* menu_;
    QAction* headerAction_;
    QAction* batteryAction_;

    QMenu* aboutMenu_;
    QAction* firmwareAction_;
    QAction* macAction_;

    NcWindow* ncWindow_;
    ModeWindow* modeWindow_;
    EqWindow* eqWindow_;

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

    QThread workerThread_;
    BmapWorker* worker_;
    QTimer* pollTimer_;

    Settings settings_;
    DeviceState lastState_;
};
