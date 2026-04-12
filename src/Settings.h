#pragma once

#include <QSettings>
#include <QString>
#include <QStandardPaths>
#include <QDir>

// Persistent settings stored in XDG config dir (~/.config/bosectl-qt/bosectl-qt.conf)
class Settings {
public:
    Settings()
        : settings_(configPath(), QSettings::IniFormat)
    {}

    // Connection
    QString lastMac() const           { return settings_.value("connection/mac").toString(); }
    void setLastMac(const QString& v) { settings_.setValue("connection/mac", v); settings_.sync(); }

    QString lastDeviceType() const           { return settings_.value("connection/device_type").toString(); }
    void setLastDeviceType(const QString& v) { settings_.setValue("connection/device_type", v); settings_.sync(); }

    bool autoConnect() const          { return settings_.value("connection/auto_connect", true).toBool(); }
    void setAutoConnect(bool v)       { settings_.setValue("connection/auto_connect", v); settings_.sync(); }

    // Noise cancellation
    int lastCnc() const               { return settings_.value("audio/cnc", 5).toInt(); }
    void setLastCnc(int v)            { settings_.setValue("audio/cnc", v); settings_.sync(); }

    // EQ
    int eqBass() const                { return settings_.value("audio/eq_bass", 0).toInt(); }
    void setEqBass(int v)             { settings_.setValue("audio/eq_bass", v); settings_.sync(); }

    int eqMid() const                 { return settings_.value("audio/eq_mid", 0).toInt(); }
    void setEqMid(int v)              { settings_.setValue("audio/eq_mid", v); settings_.sync(); }

    int eqTreble() const              { return settings_.value("audio/eq_treble", 0).toInt(); }
    void setEqTreble(int v)           { settings_.setValue("audio/eq_treble", v); settings_.sync(); }

    // Last mode
    QString lastMode() const           { return settings_.value("audio/mode").toString(); }
    void setLastMode(const QString& v) { settings_.setValue("audio/mode", v); settings_.sync(); }

    // Sidetone
    QString lastSidetone() const           { return settings_.value("audio/sidetone", "off").toString(); }
    void setLastSidetone(const QString& v) { settings_.setValue("audio/sidetone", v); settings_.sync(); }

    // Spatial
    QString lastSpatial() const           { return settings_.value("audio/spatial", "off").toString(); }
    void setLastSpatial(const QString& v) { settings_.setValue("audio/spatial", v); settings_.sync(); }

    // Poll interval in seconds
    int pollInterval() const           { return settings_.value("general/poll_interval", 60).toInt(); }
    void setPollInterval(int v)        { settings_.setValue("general/poll_interval", v); settings_.sync(); }

private:
    QSettings settings_;

    static QString configPath() {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                      + "/bosectl-qt";
        QDir().mkpath(dir);
        return dir + "/bosectl-qt.conf";
    }
};
