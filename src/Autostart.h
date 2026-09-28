#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
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

    static bool isEnabled() {
        return QFileInfo::exists(entryPath());
    }

    // Returns false if the entry could not be written or removed.
    static bool setEnabled(bool on, const QString& executable) {
        const QString path = entryPath();
        if (!on) return !QFileInfo::exists(path) || QFile::remove(path);

        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        f.write(desktopEntry(executable).toUtf8());
        return f.commit();
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
    // Desktop Entry Spec: an Exec argument containing reserved characters is
    // double-quoted, with ", `, $ and \ backslash-escaped inside the quotes.
    static QString quoteExec(const QString& arg) {
        static const QString reserved = " \t\n\"'\\><~|&;$*?#()`";
        bool needsQuotes = arg.isEmpty();
        for (QChar c : arg) {
            if (reserved.contains(c)) { needsQuotes = true; break; }
        }
        if (!needsQuotes) return arg;

        QString out = "\"";
        for (QChar c : arg) {
            if (c == '"' || c == '`' || c == '$' || c == '\\') out += '\\';
            out += c;
        }
        return out + "\"";
    }
};
