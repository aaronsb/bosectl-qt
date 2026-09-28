// Autostart entry lifecycle, written under QStandardPaths test mode so the
// real ~/.config/autostart is never touched.
#include <QtTest>

#include "Autostart.h"

class TestAutostart : public QObject {
    Q_OBJECT

private:
    static void writeEntry(const QString& path, const QString& body) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(body.toUtf8());
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init() {
        QFile::remove(Autostart::entryPath());
        QFile::remove(Autostart::legacyEntryPath());
    }
    void cleanup() {
        QFile::remove(Autostart::entryPath());
        QFile::remove(Autostart::legacyEntryPath());
    }

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

    void setEnabledRejectsNewlineInExecutable() {
        QVERIFY(!Autostart::setEnabled(true, "/usr/bin/bose\nctl"));
        QVERIFY(!Autostart::isEnabled());
    }

    void execIsQuotedPerSpec() {
        // Shell-quoting escapes '$' with a backslash; the Desktop Entry
        // Spec's own string unescaping then requires that backslash be
        // doubled so a reader gets a single backslash back.
        QVERIFY(Autostart::desktopEntry("/opt/my apps/bose$1")
                    .contains("\nExec=\"/opt/my apps/bose\\\\$1\"\n"));
    }

    void percentIsDoubled() {
        // '%' introduces a field code in Exec=, so a literal '%' must be
        // escaped as '%%' even when the argument otherwise needs no quoting.
        QVERIFY(Autostart::desktopEntry("/usr/bin/100%done")
                    .contains("\nExec=/usr/bin/100%%done\n"));
    }

    void legacyEntryIsDetectedAsEnabled() {
        QVERIFY(!Autostart::isEnabled());
        writeEntry(Autostart::legacyEntryPath(),
                   "[Desktop Entry]\n"
                   "Type=Application\n"
                   "Exec=bosectl-qt\n");
        QVERIFY(Autostart::isEnabled());
    }

    void disablingRemovesLegacyEntry() {
        writeEntry(Autostart::legacyEntryPath(),
                   "[Desktop Entry]\n"
                   "Type=Application\n"
                   "Exec=bosectl-qt\n");
        QVERIFY(Autostart::setEnabled(false, "bosectl-qt"));
        QVERIFY(!QFileInfo::exists(Autostart::legacyEntryPath()));
        QVERIFY(!Autostart::isEnabled());
    }

    void enablingReplacesLegacyEntry() {
        writeEntry(Autostart::legacyEntryPath(),
                   "[Desktop Entry]\n"
                   "Type=Application\n"
                   "Exec=bosectl-qt\n");
        QVERIFY(Autostart::setEnabled(true, "bosectl-qt"));
        QVERIFY(!QFileInfo::exists(Autostart::legacyEntryPath()));
        QVERIFY(QFileInfo::exists(Autostart::entryPath()));
        QVERIFY(Autostart::isEnabled());
    }

    void hiddenEntryIsNotEnabled() {
        writeEntry(Autostart::entryPath(),
                   "[Desktop Entry]\n"
                   "Type=Application\n"
                   "Exec=bosectl-qt\n"
                   "Hidden=true\n");
        QVERIFY(!Autostart::isEnabled());
    }

    void gnomeDisabledEntryIsNotEnabled() {
        writeEntry(Autostart::entryPath(),
                   "[Desktop Entry]\n"
                   "Type=Application\n"
                   "Exec=bosectl-qt\n"
                   "X-GNOME-Autostart-enabled=false\n");
        QVERIFY(!Autostart::isEnabled());
    }

    void execForFallsBackToAppPathWhenNoMatch() {
        qunsetenv("APPIMAGE");
        const QString appPath = "/nonexistent/path/to/bosectl-qt";
        QCOMPARE(Autostart::execFor(appPath), appPath);
    }
};

QTEST_GUILESS_MAIN(TestAutostart)
#include "test_autostart.moc"
