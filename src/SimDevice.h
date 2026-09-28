#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "bmap.h"

// A simulated QC Ultra 2 for screenshots and UI tests. It answers the BMAP
// packets BmapConnection sends with the replies a real headset gives, and
// keeps state: a SETGET changes what the next GET returns, a mode switch
// applies that mode's settings, a profile write fills a slot.
//
// Enabled in the app by BOSECTL_QT_SIM=qc_ultra2 (see main.cpp). One
// SimDevice outlives any number of connections to it, the way a headset
// outlives a Bluetooth link, so settings survive Reconnect.
//
// Thread-safety: the worker thread calls handle() through the transport
// while the GUI thread (SimControl) changes battery, reachability and
// latency. Every member function takes the mutex.
class SimDevice : public std::enable_shared_from_this<SimDevice> {
public:
    static constexpr const char* kDeviceType = "qc_ultra2";
    // Locally administered, so it can never be a real device's address.
    static constexpr const char* kMac = "02:00:00:B0:5E:01";

    struct Slot {
        std::string name;
        uint8_t cnc = 0;
        uint8_t spatial = 0;   // 0 off, 1 room, 2 head
        bool wind = true;
        bool anc = true;
        bool editable = false;
        bool configured = false;
    };

    // Construct through std::make_shared: connect() hands the transport a
    // shared_ptr to this device.
    SimDevice();

    bmap::DeviceConfig config() const { return bmap::qc_ultra2(); }

    // A transport onto this device. Throws the error a powered-off headset
    // gives when the device is unreachable.
    std::unique_ptr<bmap::Transport> connect();

    // One request, one reply buffer (possibly several frames for a drain).
    std::vector<uint8_t> handle(const std::vector<uint8_t>& packet);

    // Back to the state a new SimDevice starts in. Reachability is kept.
    void reset();

    void setBattery(uint8_t pct);
    void setReachable(bool on);
    bool reachable() const;
    // Delay every reply by this many milliseconds, to hold the app in its
    // busy state long enough to see it.
    void setLatency(int ms);
    int latency() const;

    // One line of the device's state, for the harness.
    std::string describe() const;

private:
    std::vector<uint8_t> handleLocked(uint8_t fblock, uint8_t func, bmap::Operator op,
                                      const std::vector<uint8_t>& payload);
    std::vector<uint8_t> slotStatus(uint8_t idx) const;
    void applyMode(uint8_t idx);
    void seed();   // the starting state; mutex_ held or not yet shared

    mutable std::mutex mutex_;
    bool reachable_ = true;
    int latencyMs_ = 0;

    uint8_t battery_ = 0;
    std::string name_;
    std::string firmware_;
    uint8_t prompts_ = 0;
    std::array<int8_t, 3> eq_{};
    uint8_t sidetone_ = 0;
    bool multipoint_ = false;
    bool autoPause_ = false;
    uint8_t currentMode_ = 0;
    // [31.10]: cnc, auto_cnc, spatial, wind, anc
    std::array<uint8_t, 5> audio_{};
    std::array<Slot, 11> slots_;
};

// The transport a connection to the SimDevice uses.
class SimTransport : public bmap::Transport {
public:
    explicit SimTransport(std::shared_ptr<SimDevice> device) : device_(std::move(device)) {}
    std::vector<uint8_t> send_recv(const std::vector<uint8_t>& packet) override {
        return device_->handle(packet);
    }
    std::vector<uint8_t> send_recv_drain(const std::vector<uint8_t>& packet) override {
        return device_->handle(packet);
    }

private:
    std::shared_ptr<SimDevice> device_;
};
