#include <QApplication>
#include <QSystemTrayIcon>
#include <QMessageBox>

#include "BmapWorker.h"
#include "TrayIcon.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("bosectl-qt");
    app.setOrganizationName("bosectl");
    app.setQuitOnLastWindowClosed(false);

    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<int8_t>("int8_t");
    qRegisterMetaType<EqState>("EqState");
    qRegisterMetaType<DeviceState>("DeviceState");
    qRegisterMetaType<QStringList>("QStringList");
    qRegisterMetaType<ModeInfo>("ModeInfo");
    qRegisterMetaType<QList<ModeInfo>>("QList<ModeInfo>");

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "bosectl-qt",
                              "System tray not available on this system.");
        return 1;
    }

    TrayIcon tray;
    tray.show();

    return app.exec();
}
