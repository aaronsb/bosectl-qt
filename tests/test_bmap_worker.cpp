// BmapWorker behaviour over a mock transport — no Bluetooth, no tray.
#include <QtTest>
#include <QSignalSpy>

#include <map>
#include <memory>

#include "BmapWorker.h"
#include "devices.h"

namespace {

// Mirrors the mock in lib/bosectl/cpp/tests/test_connection.cpp.
class MockTransport : public bmap::Transport {
public:
    std::map<std::pair<uint8_t, uint8_t>, std::vector<uint8_t>> responses;
    std::vector<std::vector<uint8_t>> sent;

    void add(uint8_t fblock, uint8_t func, uint8_t op, std::vector<uint8_t> payload) {
        std::vector<uint8_t> resp = {fblock, func, op, static_cast<uint8_t>(payload.size())};
        resp.insert(resp.end(), payload.begin(), payload.end());
        responses[{fblock, func}] = resp;
    }
    std::vector<uint8_t> send_recv(const std::vector<uint8_t>& packet) override {
        sent.push_back(packet);
        auto it = responses.find({packet[0], packet[1]});
        if (it != responses.end()) return it->second;
        return {packet[0], packet[1], 0x04, 1, 4};  // FuncNotSupp
    }
    std::vector<uint8_t> send_recv_drain(const std::vector<uint8_t>& packet) override {
        return send_recv(packet);
    }
};

// 48-byte QC Ultra 2 ModeConfig STATUS for slot 4 "Work", prompts (0x0c,0x01).
std::vector<uint8_t> ultra2Slot4() {
    std::vector<uint8_t> p = {4, 0x0c, 0x01, 1, 1, 0};
    std::string name = "Work";
    for (int i = 0; i < 32; ++i) p.push_back(i < (int)name.size() ? name[i] : 0);
    p.insert(p.end(), {0, 0, 0, 0, 5, 0, 0, 1, 0, 1});  // cnc=5 @42, wind @45, anc @47
    return p;
}

// 47-byte prince ModeConfig STATUS for slot 3 "Music".
std::vector<uint8_t> princeSlot3() {
    std::vector<uint8_t> p = {3, 0, 0, 1, 1, 0};
    std::string name = "Music";
    for (int i = 0; i < 32; ++i) p.push_back(i < (int)name.size() ? name[i] : 0);
    p.insert(p.end(), {0, 0, 0, 0, 5, 0, 0, 0, 0});
    return p;
}

// Last SETGET to [31.6] — emitModeDetails() sends more traffic afterwards.
const std::vector<uint8_t>& lastModeWrite(const MockTransport& t) {
    for (auto it = t.sent.rbegin(); it != t.sent.rend(); ++it) {
        if ((*it)[0] == 31 && (*it)[1] == 6 && (*it)[2] == 0x02) return *it;
    }
    static const std::vector<uint8_t> none;
    return none;
}

struct Rig {
    MockTransport* t;
    BmapWorker worker;
    Rig(bmap::DeviceConfig cfg, const std::vector<uint8_t>& slot) {
        auto mt = std::make_unique<MockTransport>();
        t = mt.get();
        std::vector<uint8_t> getAll = {31, 6, 0x03, static_cast<uint8_t>(slot.size())};
        getAll.insert(getAll.end(), slot.begin(), slot.end());
        t->responses[{31, 1}] = getAll;
        t->add(31, 6, 0x03, slot);
        t->add(31, 3, 0x03, {slot[0]});
        worker.setConnectionForTest(
            std::make_unique<bmap::BmapConnection>(std::move(mt), std::move(cfg)));
    }
};

} // namespace

class TestBmapWorker : public QObject {
    Q_OBJECT

private slots:
    void friendlyConnectError_mapsKnownErrno() {
        QCOMPARE(BmapWorker::friendlyConnectError("Failed to connect: Host is down"),
                 QString("Headphones appear to be off or out of range."));
        QCOMPARE(BmapWorker::friendlyConnectError("[Errno 16] Device or resource busy"),
                 QString("Bluetooth is busy — try again in a moment."));
    }

    void friendlyConnectError_passesUnknownThrough() {
        QCOMPARE(BmapWorker::friendlyConnectError("something else"), QString("something else"));
    }

    void deviceState_equalityCoversEveryField() {
        DeviceState a, b;
        QVERIFY(a == b);
        b.windBlock = !a.windBlock;  QVERIFY(a != b); b = a;
        b.deviceType = "qc45";       QVERIFY(a != b); b = a;
        b.eq.treble = 3;             QVERIFY(a != b);
    }

    void saveMode_usesDeviceBuilder_40bytes() {
        Rig rig(bmap::qc_ultra2(), ultra2Slot4());
        QSignalSpy errors(&rig.worker, &BmapWorker::error);
        rig.worker.saveMode(4, "Work", 7, 0, false, true);
        QCOMPARE(errors.count(), 0);
        const auto& pkt = lastModeWrite(*rig.t);
        QVERIFY(!pkt.empty());
        QCOMPARE(pkt[3], 40);
        QCOMPARE(pkt[4], 4);
        QCOMPARE(pkt[5], 0x0c); QCOMPARE(pkt[6], 0x01);  // prompt bytes carried over
        QCOMPARE(pkt[4 + 35], 7);                       // cnc
        QCOMPARE(pkt[4 + 39], 1);                       // anc toggle present
    }

    void saveMode_usesDeviceBuilder_39bytes() {
        Rig rig(bmap::qc_prince(), princeSlot3());
        QSignalSpy errors(&rig.worker, &BmapWorker::error);
        rig.worker.saveMode(3, "Music", 2, 0, true, false);
        QCOMPARE(errors.count(), 0);
        const auto& pkt = lastModeWrite(*rig.t);
        QVERIFY(!pkt.empty());
        QCOMPARE(pkt[3], 39);
        QCOMPARE(pkt[4], 3);
        QCOMPARE(pkt[4 + 35], 2);   // cnc
        QCOMPARE(pkt[4 + 38], 1);   // wind block is the last byte; no anc byte
    }

    void saveMode_reportsErrorWhenDeviceCannotEdit() {
        Rig rig(bmap::qc_earbuds(), princeSlot3());
        QSignalSpy errors(&rig.worker, &BmapWorker::error);
        rig.worker.saveMode(3, "Music", 2, 0, true, false);
        QCOMPARE(errors.count(), 1);
        QVERIFY(errors.first().first().toString().contains("not supported"));
    }

    void setCnc_withoutConnection_isNoop() {
        BmapWorker w;
        QSignalSpy errors(&w, &BmapWorker::error);
        w.setCnc(3);
        QCOMPARE(errors.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestBmapWorker)
#include "test_bmap_worker.moc"
