#include <QApplication>
#include <QCommandLineParser>
#include <QDBusMetaType>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QSystemTrayIcon>

#include "BluezBatteryProvider.h"
#include "BmapWorker.h"
#include "Logging.h"
#include "TrayIcon.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("bosectl-qt");
    app.setApplicationVersion("0.3.0");
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

    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<int8_t>("int8_t");
    qRegisterMetaType<EqState>("EqState");
    qRegisterMetaType<DeviceState>("DeviceState");
    qRegisterMetaType<QStringList>("QStringList");
    qRegisterMetaType<ModeInfo>("ModeInfo");
    qRegisterMetaType<QList<ModeInfo>>("QList<ModeInfo>");

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

    TrayIcon tray;
    tray.show();

    return app.exec();
}
