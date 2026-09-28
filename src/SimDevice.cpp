#include "SimDevice.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <thread>

using bmap::Operator;

namespace {

std::vector<uint8_t> frame(uint8_t fblock, uint8_t func, Operator op,
                           const std::vector<uint8_t>& payload) {
    return bmap::bmap_packet(fblock, func, op, payload);
}

void append(std::vector<uint8_t>& out, const std::vector<uint8_t>& more) {
    out.insert(out.end(), more.begin(), more.end());
}

constexpr uint8_t kFuncNotSupp = 4;
constexpr uint8_t kInvalidData = 6;
constexpr uint8_t kCncMax = 10;

}  // namespace

SimDevice::SimDevice() {
    // Presets: fixed names, read-only. Their settings are what switching to
    // them applies.
    slots_[0] = {"Quiet", 10, 0, true, true, false, true};
    slots_[1] = {"Aware", 0, 0, true, false, false, true};
    slots_[2] = {"Immersion", 10, 2, true, true, false, true};
    slots_[3] = {"Cinema", 10, 1, true, true, false, true};
    // Custom profiles: two in use, the rest free.
    slots_[4] = {"Commute", 7, 0, true, true, true, true};
    slots_[5] = {"Focus", 10, 0, false, true, true, true};
    for (size_t i = 6; i < slots_.size(); ++i) slots_[i] = {"None", 0, 0, true, true, true, false};
}

std::unique_ptr<bmap::Transport> SimDevice::connect() {
    std::lock_guard lock(mutex_);
    if (!reachable_)
        throw std::runtime_error(std::string("Failed to connect to ") + kMac + ": Host is down");
    return std::make_unique<SimTransport>(shared_from_this());
}

void SimDevice::setBattery(uint8_t pct) {
    std::lock_guard lock(mutex_);
    battery_ = pct > 100 ? 100 : pct;
}

void SimDevice::setReachable(bool on) {
    std::lock_guard lock(mutex_);
    reachable_ = on;
}

bool SimDevice::reachable() const {
    std::lock_guard lock(mutex_);
    return reachable_;
}

void SimDevice::setLatency(int ms) {
    std::lock_guard lock(mutex_);
    latencyMs_ = ms < 0 ? 0 : ms;
}

int SimDevice::latency() const {
    std::lock_guard lock(mutex_);
    return latencyMs_;
}

std::string SimDevice::describe() const {
    std::lock_guard lock(mutex_);
    std::ostringstream s;
    s << "reachable=" << (reachable_ ? "true" : "false")
      << " battery=" << int(battery_)
      << " name=" << name_
      << " mode=" << int(currentMode_)
      << " cnc=" << int(audio_[0])
      << " spatial=" << int(audio_[2])
      << " wind=" << int(audio_[3])
      << " anc=" << int(audio_[4])
      << " eq=" << int(eq_[0]) << "," << int(eq_[1]) << "," << int(eq_[2])
      << " sidetone=" << int(sidetone_)
      << " multipoint=" << (multipoint_ ? 1 : 0)
      << " autopause=" << (autoPause_ ? 1 : 0)
      << " latency=" << latencyMs_;
    return s.str();
}

std::vector<uint8_t> SimDevice::handle(const std::vector<uint8_t>& packet) {
    int delay = latency();
    if (delay > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay));

    auto req = bmap::parse_response(packet);
    if (!req) return {};
    std::lock_guard lock(mutex_);
    // A link that dropped while connected answers nothing, the way a
    // headset switched off mid-session does.
    if (!reachable_) throw std::runtime_error("Connection reset by peer");
    return handleLocked(req->fblock, req->func, req->op, req->payload);
}

// 48-byte QC Ultra 2 ModeConfig STATUS: idx, two prompt bytes, editable,
// configured, a reserved byte, 32 name bytes, four reserved, then cnc,
// auto_cnc, spatial, wind, a reserved byte, anc.
std::vector<uint8_t> SimDevice::slotStatus(uint8_t idx) const {
    const Slot& s = slots_[idx];
    std::vector<uint8_t> p = {idx, 0, 0, uint8_t(s.editable), uint8_t(s.configured), 0};
    auto name = bmap::encode_mode_name(s.name);
    p.insert(p.end(), name.begin(), name.end());
    p.insert(p.end(), {0, 0, 0, 0, s.cnc, 0, s.spatial, uint8_t(s.wind), 0, uint8_t(s.anc)});
    return frame(31, 6, Operator::Status, p);
}

void SimDevice::applyMode(uint8_t idx) {
    currentMode_ = idx;
    const Slot& s = slots_[idx];
    audio_ = {s.cnc, 0, s.spatial, uint8_t(s.wind), uint8_t(s.anc)};
}

std::vector<uint8_t> SimDevice::handleLocked(uint8_t fb, uint8_t fn, Operator op,
                                             const std::vector<uint8_t>& p) {
    auto status = [&](const std::vector<uint8_t>& payload) {
        return frame(fb, fn, Operator::Status, payload);
    };
    auto error = [&](uint8_t code) { return frame(fb, fn, Operator::Error, {code}); };
    const bool get = op == Operator::Get;
    const bool setget = op == Operator::SetGet;
    const bool start = op == Operator::Start;

    auto nameReply = [&] {
        std::vector<uint8_t> out = {0};
        out.insert(out.end(), name_.begin(), name_.end());
        return status(out);
    };
    auto eqReply = [&] {
        std::vector<uint8_t> out;
        for (uint8_t band = 0; band < 3; ++band)
            append(out, {uint8_t(int8_t(-10)), 10, uint8_t(eq_[band]), band});
        return status(out);
    };
    auto audioReply = [&] { return status({audio_.begin(), audio_.end()}); };

    switch ((fb << 8) | fn) {
    case (0 << 8) | 5:  // firmware
        if (get) return status({firmware_.begin(), firmware_.end()});
        break;
    case (1 << 8) | 2:  // product name
        if (get) return nameReply();
        if (setget) {
            name_.assign(p.begin(), p.end());
            return nameReply();
        }
        break;
    case (1 << 8) | 3:  // voice prompts
        if (get) return status({prompts_});
        break;
    case (1 << 8) | 5:  // cnc: max+1, current, reserved
        if (get) return status({kCncMax + 1, audio_[0], 0});
        break;
    case (1 << 8) | 7:  // eq: {min, max, current, band} per band
        if (get) return eqReply();
        if (setget && p.size() >= 2 && p[1] < 3) {
            eq_[p[1]] = static_cast<int8_t>(p[0]);
            return eqReply();
        }
        if (setget) return error(kInvalidData);
        break;
    case (1 << 8) | 10:  // multipoint, bit 1
        if (get) return status({uint8_t(multipoint_ ? 0x02 : 0)});
        if (setget && !p.empty()) {
            multipoint_ = p[0] != 0;
            return status({uint8_t(multipoint_ ? 0x02 : 0)});
        }
        break;
    case (1 << 8) | 11:  // sidetone: {1, level}
        if (get) return status({1, sidetone_});
        if (setget && p.size() >= 2) {
            sidetone_ = p[1];
            return status({1, sidetone_});
        }
        break;
    case (1 << 8) | 24:  // auto-pause
        if (get) return status({uint8_t(autoPause_)});
        if (setget && !p.empty()) {
            autoPause_ = p[0] != 0;
            return status({uint8_t(autoPause_)});
        }
        break;
    case (2 << 8) | 2:  // battery
        if (get) return status({battery_});
        break;
    case (7 << 8) | 4:  // power off: the link goes with it
        if (start) {
            reachable_ = false;
            return frame(fb, fn, Operator::Result, {});
        }
        break;
    case (31 << 8) | 1:  // all mode configs, one STATUS [31.6] each
        if (start) {
            std::vector<uint8_t> out;
            for (uint8_t i = 0; i < slots_.size(); ++i) append(out, slotStatus(i));
            append(out, frame(fb, fn, Operator::Result, {}));
            return out;
        }
        break;
    case (31 << 8) | 3:  // current mode
        if (get) return status({currentMode_});
        if (start && !p.empty()) {
            if (p[0] >= slots_.size() || !slots_[p[0]].configured) return error(kInvalidData);
            applyMode(p[0]);
            return frame(fb, fn, Operator::Result, {currentMode_});
        }
        break;
    case (31 << 8) | 6:  // write one mode config (40-byte layout)
        if (setget && p.size() >= 40) {
            uint8_t idx = p[0];
            if (idx >= slots_.size() || !slots_[idx].editable) return error(kInvalidData);
            Slot& s = slots_[idx];
            auto end = std::find(p.begin() + 3, p.begin() + 35, uint8_t(0));
            s.name.assign(p.begin() + 3, end);
            s.cnc = p[35];
            s.spatial = p[37];
            s.wind = p[38] != 0;
            s.anc = p[39] != 0;
            s.configured = true;
            if (idx == currentMode_) applyMode(idx);
            return slotStatus(idx);
        }
        break;
    case (31 << 8) | 10:  // audio settings
        if (get) return audioReply();
        if (setget && p.size() >= 5) {
            std::copy(p.begin(), p.begin() + 5, audio_.begin());
            // An editable mode remembers what was changed while it was on.
            Slot& s = slots_[currentMode_];
            if (s.editable) {
                s.cnc = audio_[0];
                s.spatial = audio_[2];
                s.wind = audio_[3] != 0;
                s.anc = audio_[4] != 0;
            }
            return audioReply();
        }
        break;
    default:
        break;
    }
    return error(kFuncNotSupp);
}
