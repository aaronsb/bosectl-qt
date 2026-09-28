// Autostart entry lifecycle, written under QStandardPaths test mode so the
// real ~/.config/autostart is never touched.
#include <QtTest>

#include "Autostart.h"

class TestAutostart : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void cleanup()      { QFile::remove(Autostart::entryPath()); }

    void entryLivesUnderTestConfig() {
        QVERIFY(Autostart::entryPath().contains("qttest"));
        QVERIFY(Autostart::entryPath().endsWith("/autostart/bosectl-qt.desktop"));
    }

    void enableWritesEntry() {
        QVERIFY(!Autostart::isEnabled());
        QVERIFY(Autostart::setEnabled(true, "/usr/bin/bosectl-qt"));
        QVERIFY(Autostart::isEnabled());

        QFile f(Autostart::entryPath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString body = QString::fromUtf8(f.readAll());
        QVERIFY(body.startsWith("[Desktop Entry]\n"));
        QVERIFY(body.contains("\nExec=/usr/bin/bosectl-qt\n"));
        QVERIFY(body.contains("\nX-KDE-autostart-after=panel\n"));
    }

    void disableRemovesEntry() {
        QVERIFY(Autostart::setEnabled(true, "bosectl-qt"));
        QVERIFY(Autostart::setEnabled(false, "bosectl-qt"));
        QVERIFY(!Autostart::isEnabled());
    }

    void disableWhenAbsentSucceeds() {
        QVERIFY(Autostart::setEnabled(false, "bosectl-qt"));
    }

    void execIsQuotedPerSpec() {
        QVERIFY(Autostart::desktopEntry("/opt/my apps/bose$1")
                    .contains("\nExec=\"/opt/my apps/bose\\$1\"\n"));
    }
};

QTEST_GUILESS_MAIN(TestAutostart)
#include "test_autostart.moc"
