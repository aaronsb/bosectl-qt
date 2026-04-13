#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>
#include <mutex>

#include "bmap.h"
#include "Logging.h"

struct EqState {
    int8_t bass = 0;
    int8_t mid = 0;
    int8_t treble = 0;
};
Q_DECLARE_METATYPE(EqState)

struct ModeInfo {
    uint8_t idx = 0;
    QString name;
    uint8_t cncLevel = 0;
    uint8_t spatial = 0;
    bool windBlock = true;
    bool ancToggle = true;
    bool editable = false;
    bool configured = false;
    bool active = false;
};
Q_DECLARE_METATYPE(ModeInfo)
Q_DECLARE_METATYPE(QList<ModeInfo>)

struct DeviceState {
    uint8_t battery = 0;
    QString mode;
    uint8_t cncLevel = 0;
    uint8_t cncMax = 10;
    EqState eq;
    QString deviceName;
    QString firmware;
    QString sidetone;
    QString spatial;
    QString mac;
    QString deviceType;
    bool multipoint = false;
    bool autoPause = false;
    bool ancEnabled = true;
    bool windBlock = false;
    bool connected = false;
};
Q_DECLARE_METATYPE(DeviceState)

// Runs all blocking bmap I/O on a dedicated thread.
// The GUI thread queues requests via slots; results come back as signals.
class BmapWorker : public QObject {
    Q_OBJECT

public:
    explicit BmapWorker(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void statusReady(DeviceState state);
    void modesReady(QStringList modes);
    void modeDetailsReady(QList<ModeInfo> modes, uint8_t activeIdx);
    void error(QString message);
    void disconnected();
    void busy(bool working);

public slots:
    void connectDevice(const QString& mac, const QString& deviceType, bool announceError) {
        BusyGuard guard(this);
        qCInfo(lcWorker) << "connectDevice: mac=" << (mac.isEmpty() ? "<auto>" : mac)
                         << "type=" << (deviceType.isEmpty() ? "<auto>" : deviceType)
                         << "announceError=" << announceError;

        std::lock_guard lock(mutex_);

        // Close any existing socket and give bluez time to release the rfcomm
        // channel. Without this settle delay the next bmap::connect() race-
        // loses to the kernel and returns EBUSY.
        if (conn_) {
            qCInfo(lcWorker) << "connectDevice: closing existing connection first";
            conn_.reset();
            QThread::msleep(kReconnectSettleMs);
        }

        // Retry on transient failures. EBUSY and similar post-teardown errors
        // usually clear within a second — worth absorbing silently instead of
        // surfacing a scary disconnect notification.
        std::string lastErr;
        for (int attempt = 1; attempt <= kConnectMaxAttempts; ++attempt) {
            try {
                conn_ = bmap::connect(mac.toStdString(), deviceType.toStdString());
                connMac_ = mac;
                if (connMac_.isEmpty()) {
                    auto detected = bmap::find_bmap_device();
                    if (detected) {
                        connMac_ = QString::fromStdString(detected->first);
                        connDeviceType_ = QString::fromStdString(detected->second);
                    }
                } else {
                    connDeviceType_ = deviceType;
                }
                qCInfo(lcWorker) << "connectDevice: success on attempt" << attempt
                                 << "mac=" << connMac_ << "type=" << connDeviceType_;
                emitStatus();
                return;
            } catch (const std::exception& e) {
                lastErr = e.what();
                qCWarning(lcWorker) << "connectDevice: attempt" << attempt
                                    << "of" << kConnectMaxAttempts << "failed:" << e.what();
                if (attempt < kConnectMaxAttempts) {
                    QThread::msleep(kConnectRetryDelayMs);
                }
            }
        }

        conn_.reset();
        if (announceError) {
            emit error(friendlyConnectError(lastErr));
        }
        emit statusReady(DeviceState{});
    }

    void refresh() {
        std::lock_guard lock(mutex_);
        if (!conn_) {
            qCDebug(lcWorker) << "refresh: no connection, skipping";
            emit statusReady(DeviceState{});
            return;
        }
        try {
            qCDebug(lcWorker) << "refresh: polling status";
            emitStatus();
        } catch (const std::exception& e) {
            qCWarning(lcWorker) << "refresh: failed:" << e.what();
            conn_.reset();
            emit error(QString::fromStdString(e.what()));
            emit disconnected();
        }
    }

    void setCnc(uint8_t level) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_cnc(level);
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setMode(const QString& mode) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_mode(mode.toStdString());
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setEq(int8_t bass, int8_t mid, int8_t treble) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_eq(bass, mid, treble);
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setSidetone(const QString& level) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_sidetone(level.toStdString());
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setSpatial(const QString& mode) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_spatial(mode.toStdString());
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setName(const QString& name) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            qCInfo(lcWorker) << "setName:" << name;
            conn_->set_name(name.toStdString());
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setMultipoint(bool on) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_multipoint(on);
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setAnc(bool on) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_anc(on);
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setWind(bool on) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_wind(on);
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void setAutoPause(bool on) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_auto_pause(on);
            emitStatus();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void fetchModes() {
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            auto modes = conn_->modes();
            QStringList names;
            for (auto& m : modes) {
                if (m.configured && m.name != "None")
                    names << QString::fromStdString(m.name);
            }
            for (auto& [name, _] : conn_->config().preset_modes) {
                auto qname = QString::fromStdString(name);
                if (!names.contains(qname))
                    names.prepend(qname);
            }
            emit modesReady(names);
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void fetchModeDetails() {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            emitModeDetails();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void saveMode(uint8_t idx, const QString& name, uint8_t cnc,
                  uint8_t spatial, bool windBlock, bool ancToggle) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            // If idx == 255, create new profile
            if (idx == 255) {
                conn_->create_profile(name.toStdString(), cnc, spatial, windBlock, ancToggle);
            } else {
                // Write mode config directly using the internal write path
                // We re-create the mode via create_profile style write
                bmap::ModeConfig mc{};
                mc.mode_idx = idx;
                mc.name = name.toStdString();
                mc.cnc_level = cnc;
                mc.spatial = spatial;
                mc.wind_block = windBlock;
                mc.anc_toggle = ancToggle;

                // Use the connection's raw write approach
                auto addr = conn_->config().mode_config;
                if (!addr) throw std::runtime_error("mode_config not supported");
                auto payload = bmap::build_mode_config_40(
                    idx, mc.name, cnc, spatial, windBlock, ancToggle);
                auto pkt = bmap::bmap_packet(addr->fblock, addr->func,
                                              bmap::Operator::SetGet, payload);
                conn_->send_raw(pkt);
            }
            emitModeDetails();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void deleteMode(const QString& name) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->delete_profile(name.toStdString());
            emitModeDetails();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void activateMode(const QString& name) {
        BusyGuard guard(this);
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->set_mode(name.toStdString());
            emitStatus();
            emitModeDetails();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void powerOff() {
        std::lock_guard lock(mutex_);
        if (!conn_) return;
        try {
            conn_->power_off();
            conn_.reset();
            emit disconnected();
        } catch (const std::exception& e) { emit error(QString::fromStdString(e.what())); }
    }

    void disconnect() {
        std::lock_guard lock(mutex_);
        conn_.reset();
        emit disconnected();
    }

private:
    // RAII helper that emits busy(true) on construction, busy(false) on destruction
    struct BusyGuard {
        BmapWorker* w;
        BusyGuard(BmapWorker* w) : w(w) { emit w->busy(true); }
        ~BusyGuard() { emit w->busy(false); }
    };

    static constexpr int kReconnectSettleMs = 300;
    static constexpr int kConnectRetryDelayMs = 500;
    static constexpr int kConnectMaxAttempts = 3;

    // Translate kernel errno strings from libbmap's "Failed to connect ...:"
    // wrapper into messages a user can actually act on.
    static QString friendlyConnectError(const std::string& raw) {
        const QString s = QString::fromStdString(raw);
        struct Pair { const char* needle; const char* msg; };
        static constexpr Pair kMap[] = {
            {"Host is down",              "Headphones appear to be off or out of range."},
            {"No route to host",          "Cannot reach the headphones — are they powered on?"},
            {"Device or resource busy",   "Bluetooth is busy — try again in a moment."},
            {"Operation now in progress", "Connection timed out — headphones not responding."},
            {"Connection timed out",      "Connection timed out — headphones not responding."},
            {"Connection refused",        "Headphones refused the connection."},
        };
        for (const auto& p : kMap) {
            if (s.contains(QLatin1String(p.needle))) return QString::fromLatin1(p.msg);
        }
        return s;
    }

    std::unique_ptr<bmap::BmapConnection> conn_;
    std::mutex mutex_;
    QString connMac_;
    QString connDeviceType_;

    // Must be called with mutex_ held
    void emitModeDetails() {
        auto allModes = conn_->modes();
        auto activeIdx = conn_->mode_idx();
        QList<ModeInfo> result;

        for (auto& [name, preset] : conn_->config().preset_modes) {
            ModeInfo mi;
            mi.idx = preset.idx;
            mi.name = QString::fromStdString(name);
            mi.editable = false;
            mi.configured = true;
            mi.active = (preset.idx == activeIdx);
            for (auto& m : allModes) {
                if (m.mode_idx == preset.idx) {
                    mi.cncLevel = m.cnc_level;
                    mi.spatial = m.spatial;
                    mi.windBlock = m.wind_block;
                    mi.ancToggle = m.anc_toggle;
                    break;
                }
            }
            result.append(mi);
        }

        for (auto& m : allModes) {
            if (!m.editable) continue;
            if (!m.configured || m.name == "None") continue;
            ModeInfo mi;
            mi.idx = m.mode_idx;
            mi.name = QString::fromStdString(m.name);
            mi.cncLevel = m.cnc_level;
            mi.spatial = m.spatial;
            mi.windBlock = m.wind_block;
            mi.ancToggle = m.anc_toggle;
            mi.editable = true;
            mi.configured = true;
            mi.active = (m.mode_idx == activeIdx);
            result.append(mi);
        }

        emit modeDetailsReady(result, activeIdx);
    }

    void emitStatus() {
        auto s = conn_->status();
        DeviceState ds;
        ds.connected = true;
        ds.battery = s.battery;
        ds.mode = QString::fromStdString(s.mode);
        ds.cncLevel = s.cnc_level;
        ds.cncMax = s.cnc_max;
        ds.deviceName = QString::fromStdString(s.name);
        ds.firmware = QString::fromStdString(s.firmware);
        ds.sidetone = QString::fromStdString(s.sidetone);
        ds.multipoint = s.multipoint;
        ds.autoPause = s.auto_pause;
        ds.mac = connMac_;
        ds.deviceType = connDeviceType_;

        for (auto& b : s.eq) {
            if (b.band_id == 0) ds.eq.bass = b.current;
            else if (b.band_id == 1) ds.eq.mid = b.current;
            else if (b.band_id == 2) ds.eq.treble = b.current;
        }

        // Read direct audio settings via [31.10]: {cnc, auto_cnc, spatial, wind, anc}
        try {
            auto as = conn_->audio_settings();
            ds.cncLevel = as[0];
            switch (as[2]) {
                case 0: ds.spatial = "off"; break;
                case 1: ds.spatial = "room"; break;
                case 2: ds.spatial = "head"; break;
                default: ds.spatial = "off"; break;
            }
            ds.windBlock = as[3] != 0;
            ds.ancEnabled = as[4] != 0;
        } catch (...) {
            ds.spatial = "off";
        }

        emit statusReady(ds);
    }
};
