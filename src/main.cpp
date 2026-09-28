#include <QApplication>
#include <QCommandLineParser>
#include <QDBusMetaType>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QSystemTrayIcon>

#include "BluezBatteryProvider.h"
#include "BmapWorker.h"
#include "Logging.h"
#include "SimControl.h"
#include "SimDevice.h"
#include "TrayIcon.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("bosectl-qt");
    app.setApplicationVersion("0.6.1");
    app.setOrganizationName("bosectl");
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Qt6 system tray application for Bose headphones (BMAP over Bluetooth).");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption verboseOption("verbose",
        "Enable debug logging (bosectl.*.debug=true). "
        "Fine-grained control is also available via QT_LOGGING_RULES.");
    parser.addOption(verboseOption);
    parser.process(app);

    if (parser.isSet(verboseOption)) {
        QLoggingCategory::setFilterRules("bosectl.*.debug=true");
        qCInfo(lcTray) << "verbose logging enabled";
    }

    // QtDBus needs to know how to marshal these nested container types so
    // ObjectManager.GetManagedObjects() and the BlueZ Battery Provider
    // registration can round-trip a{oa{sa{sv}}} correctly.
    qDBusRegisterMetaType<InterfaceProperties>();
    qDBusRegisterMetaType<ManagedObjectList>();

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "bosectl-qt",
                              "System tray not available on this system.");
        return 1;
    }

    // BOSECTL_QT_SIM=qc_ultra2 runs against a simulated headset and exports
    // org.bosectl.qt /Sim for the test harness (docs/testing.md).
    std::shared_ptr<SimDevice> sim;
    const QString simType = qEnvironmentVariable("BOSECTL_QT_SIM");
    if (!simType.isEmpty()) {
        if (simType != SimDevice::kDeviceType) {
            qCCritical(lcTray) << "BOSECTL_QT_SIM: only" << SimDevice::kDeviceType
                               << "is simulated, not" << simType;
            return 2;
        }
        sim = std::make_shared<SimDevice>();
        qCWarning(lcTray) << "running against a simulated" << simType << "headset";
    }

    TrayIcon tray(sim);
    std::unique_ptr<SimControl> control;
    if (sim) {
        control = std::make_unique<SimControl>(&tray, sim);
        if (!control->registerOnBus())
            qCWarning(lcTray) << "sim: could not register org.bosectl.qt /Sim";
    }
    tray.show();

    return app.exec();
}
