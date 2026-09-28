#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

// Per-user "start on login" via an XDG autostart entry in
// ~/.config/autostart/. KDE Plasma (and GNOME, XFCE, ...) launch every
// .desktop file there at login, and Plasma's System Settings → Autostart
// lists and removes the same file. The file's presence is the state, so a
// change made in System Settings shows up here without any sync.
class Autostart {
public:
    static QString entryPath() {
        return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
               + "/autostart/bosectl-qt.desktop";
    }

    // Earlier READMEs told users to `cp` the installed .desktop straight
    // into ~/.config/autostart/, keeping its installed name. Recognize that
    // file too so those installs still show as enabled and get cleaned up
    // when the box is unchecked.
    static QString legacyEntryPath() {
        return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
               + "/autostart/bosectl-qt-autostart.desktop";
    }

    static bool isEnabled() {
        return entryIsEnabled(entryPath()) || entryIsEnabled(legacyEntryPath());
    }

    // Returns false if the entry could not be written or removed. Rejects an
    // executable containing a newline, which would corrupt the Exec= line.
    static bool setEnabled(bool on, const QString& executable) {
        const QString path = entryPath();
        if (!on) {
            const bool removedLegacy = !QFileInfo::exists(legacyEntryPath())
                                        || QFile::remove(legacyEntryPath());
            const bool removed = !QFileInfo::exists(path) || QFile::remove(path);
            return removed && removedLegacy;
        }
        if (executable.contains('\n')) return false;

        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        f.write(desktopEntry(executable).toUtf8());
        if (!f.commit()) return false;

        // The freshly-written bosectl-qt.desktop supersedes any legacy copy.
        if (QFileInfo::exists(legacyEntryPath())) QFile::remove(legacyEntryPath());
        return true;
    }

    // Picks the Exec value for the autostart entry: the bare command name
    // when a `bosectl-qt` on PATH resolves to this same binary (so the entry
    // survives a reinstall to a different prefix), the AppImage's own path
    // when running from one (APPIMAGE points at the .AppImage file itself,
    // not the squashfs mount that applicationFilePath() would report), and
    // otherwise the running binary's absolute path.
    static QString execFor(const QString& appPath) {
        const QString onPath = QStandardPaths::findExecutable("bosectl-qt");
        if (!onPath.isEmpty()
            && QFileInfo(onPath).canonicalFilePath() == QFileInfo(appPath).canonicalFilePath()) {
            return "bosectl-qt";
        }

        const QString appImage = qEnvironmentVariable("APPIMAGE");
        if (!appImage.isEmpty()) return appImage;

        return appPath;
    }

    static QString desktopEntry(const QString& executable) {
        return QString(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Bose Headphone Control\n"
            "Comment=Start Bose headphone tray control\n"
            "Exec=%1\n"
            "Icon=bosectl-qt\n"
            "Terminal=false\n"
            "X-GNOME-Autostart-enabled=true\n"
            "X-KDE-autostart-after=panel\n").arg(quoteExec(executable));
    }

private:
    // isEnabled() for a single candidate path: present, and not switched off
    // by the entry itself via Hidden or X-GNOME-Autostart-enabled=false.
    static bool entryIsEnabled(const QString& path) {
        if (!QFileInfo::exists(path)) return false;

        QSettings settings(path, QSettings::IniFormat);
        settings.beginGroup("Desktop Entry");
        if (settings.value("Hidden", false).toBool()) return false;
        if (!settings.value("X-GNOME-Autostart-enabled", true).toBool()) return false;
        return true;
    }

    // Desktop Entry Spec: an Exec argument containing reserved characters is
    // double-quoted, with ", `, $ and \ backslash-escaped inside the quotes.
    // That quoted string is then a string-type value, and string values are
    // unescaped once more when the file is read (\\ -> \, etc.), so every
    // backslash the quoting step produced has to be doubled to survive that
    // pass, and every % doubled so it isn't read as a field-code prefix.
    static QString quoteExec(const QString& arg) {
        static const QString reserved = " \t\n\"'\\><~|&;$*?#()`";
        bool needsQuotes = arg.isEmpty();
        for (QChar c : arg) {
            if (reserved.contains(c)) { needsQuotes = true; break; }
        }

        QString quoted;
        if (!needsQuotes) {
            quoted = arg;
        } else {
            quoted = "\"";
            for (QChar c : arg) {
                if (c == '"' || c == '`' || c == '$' || c == '\\') quoted += '\\';
                quoted += c;
            }
            quoted += "\"";
        }

        QString out;
        out.reserve(quoted.size());
        for (QChar c : quoted) {
            if (c == '\\') out += "\\\\";
            else if (c == '%') out += "%%";
            else out += c;
        }
        return out;
    }
};
