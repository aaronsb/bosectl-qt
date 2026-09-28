// The tray, its menu and its windows against the simulated headset: every
// menu entry's state for a given device state, and what each control does to
// the device. Offscreen, under QStandardPaths test mode, no Bluetooth.
#include <QtTest>

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>

#include <memory>

#include "SimControl.h"
#include "SimDevice.h"
#include "TrayIcon.h"

namespace {

QWidget* window(const QString& title) {
    for (QWidget* w : QApplication::topLevelWidgets())
        if (w->windowTitle() == title) return w;
    return nullptr;
}

QPushButton* button(QWidget* w, const QString& text) {
    for (auto* b : w->findChildren<QPushButton*>())
        if (b->text() == text) return b;
    return nullptr;
}

// Answer the next modal dialog: QInputDialog gets `text` and OK,
// QMessageBox gets Yes.
void answerNextDialog(const QString& text = {}) {
    QTimer::singleShot(50, [text] {
        QWidget* modal = QApplication::activeModalWidget();
        if (auto* in = qobject_cast<QInputDialog*>(modal)) {
            in->setTextValue(text);
            in->accept();
        } else if (auto* box = qobject_cast<QMessageBox*>(modal)) {
            box->button(QMessageBox::Yes) ? box->button(QMessageBox::Yes)->click()
                                          : box->accept();
        } else if (auto* d = qobject_cast<QDialog*>(modal)) {
            d->accept();
        }
    });
}

}  // namespace

class TestTray : public QObject {
    Q_OBJECT

    std::shared_ptr<SimDevice> sim;
    std::unique_ptr<TrayIcon> tray;
    std::unique_ptr<SimControl> control;

    QString menu() const { return control->Menu(); }
    QString device() const { return control->Device(); }
    bool menuHas(const QString& line) const { return menu().split('\n').contains(line); }

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QLoggingCategory::setFilterRules("bosectl.*.info=false\nbosectl.*.warning=false");
    }

    void init() {
        const QString config =
            QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        QFile::remove(config + "/bosectl-qt/bosectl-qt.conf");
        QFile::remove(config + "/autostart/bosectl-qt.desktop");
        sim = std::make_shared<SimDevice>();
        tray = std::make_unique<TrayIcon>(sim);
        control = std::make_unique<SimControl>(tray.get(), sim);
        QTRY_VERIFY(menuHas("Battery: 80%"));
    }

    void cleanup() {
        control->Close({});
        control.reset();
        tray.reset();
        sim.reset();
    }

    // ── Menu state ─────────────────────────────────────────────────────────

    void connectedMenuReflectsDevice() {
        const QString m = menu();
        QVERIFY2(m.startsWith("Bose QC Ultra Headphones\n  Rename..."), qPrintable(m));
        QVERIFY(menuHas("  Firmware: 8.2.20+g34cf029 [disabled]"));
        QVERIFY(menuHas(QString("  MAC: %1 [disabled]").arg(SimDevice::kMac)));
        QVERIFY(menuHas("  Off [checked]"));          // spatial
        QVERIFY(menuHas("  Low [checked]"));          // sidetone
        QVERIFY(menuHas("Noise Cancellation (ANC) [checked]"));
        QVERIFY(menuHas("Wind Block [checked]"));
        QVERIFY(menuHas("Multipoint [checked]"));
        QVERIFY(menuHas("Auto-Pause [checked]"));
        QVERIFY(menuHas("Reconnect"));
        QVERIFY(menuHas("Power Off"));
        QVERIFY(menuHas("Start on Login"));
    }

    void tooltipSummarisesDevice() {
        const QStringList lines = control->Tooltip().split('\n');
        QCOMPARE(lines.value(0), QString("Bose QC Ultra Headphones — 80%"));
        QCOMPARE(lines.value(1), QString("Mode: quiet"));
        QCOMPARE(lines.value(2), QString("ANC: on · NC 10/10 · Wind: on"));
        QCOMPARE(lines.value(4), QString("EQ: B +2 · M 0 · T -1"));
    }

    void lowBatteryIsFlagged() {
        control->SetBattery(12);
        control->Refresh();
        QTRY_VERIFY(menuHas("Battery: 12% ⚠"));
        control->SetBattery(16);
        control->Refresh();
        QTRY_VERIFY(menuHas("Battery: 16%"));
    }

    void droppedLinkShowsDisconnected() {
        control->SetReachable(false);
        control->Refresh();
        QTRY_VERIFY(menuHas("Bose Headphones (disconnected)"));
        QVERIFY(menuHas("  Rename... [disabled]"));
        QVERIFY(menuHas("Battery: --"));
        QVERIFY(menuHas("Connect"));
        QVERIFY(menuHas("Power Off [disabled]"));
        QCOMPARE(control->Tooltip(), QString("Disconnected"));
    }

    void connectAfterReachableAgain() {
        control->SetReachable(false);
        control->Refresh();
        QTRY_VERIFY(menuHas("Connect"));
        control->SetReachable(true);
        QVERIFY(control->Trigger("Connect"));
        QTRY_VERIFY(menuHas("Reconnect"));
        QVERIFY(menuHas("Battery: 80%"));
    }

    void powerOffDisconnects() {
        QVERIFY(control->Trigger("Power Off"));
        QTRY_VERIFY(menuHas("Bose Headphones (disconnected)"));
        QVERIFY(device().contains("reachable=false"));
    }

    // ── Menu actions reach the device ───────────────────────────────────────

    void togglesWriteThrough() {
        QVERIFY(control->Trigger("Multipoint"));
        QTRY_VERIFY(device().contains("multipoint=0"));
        QTRY_VERIFY(menuHas("Multipoint"));
        QVERIFY(control->Trigger("Auto-Pause"));
        QTRY_VERIFY(device().contains("autopause=0"));
        QVERIFY(control->Trigger("Wind Block"));
        QTRY_VERIFY(device().contains("wind=0"));
        QVERIFY(control->Trigger("Noise Cancellation (ANC)"));
        QTRY_VERIFY(device().contains("anc=0"));
        QTRY_VERIFY(control->Tooltip().contains("ANC: off · Wind: off"));
    }

    void submenusWriteThrough() {
        QVERIFY(control->Trigger("Spatial Audio/Head Tracking"));
        QTRY_VERIFY(device().contains("spatial=2"));
        QTRY_VERIFY(menuHas("  Head Tracking [checked]"));
        QVERIFY(control->Trigger("Sidetone/High"));
        QTRY_VERIFY(device().contains("sidetone=1"));
        QTRY_VERIFY(menuHas("  High [checked]"));
    }

    void renameWritesName() {
        answerNextDialog("Desk Cans");
        QVERIFY(control->Trigger("Bose QC Ultra Headphones/Rename..."));
        QTRY_VERIFY(device().contains("name=Desk Cans "));
        QTRY_VERIFY(menu().startsWith("Desk Cans\n"));
    }

    void disabledEntryDoesNotTrigger() {
        QVERIFY(!control->Trigger("About/Firmware: 8.2.20+g34cf029"));
        QVERIFY(!control->Trigger("No Such Entry"));
        QVERIFY(!control->Trigger("Spatial Audio"));   // a submenu, not an action
    }

    void startOnLoginWritesEntry() {
        QVERIFY(control->Trigger("Start on Login"));
        QTRY_VERIFY(menuHas("Start on Login [checked]"));
        QVERIFY(QFile::exists(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                              + "/autostart/bosectl-qt.desktop"));
    }

    // ── Windows ─────────────────────────────────────────────────────────────

    void noiseCancellationApplies() {
        QVERIFY(control->Trigger("Noise Cancellation..."));
        QTRY_VERIFY(control->Windows().contains("Noise Cancellation - bosectl"));
        QWidget* w = window("Noise Cancellation - bosectl");
        auto* slider = w->findChild<QSlider*>();
        QCOMPARE(slider->value(), 10);
        QCOMPARE(slider->maximum(), 10);
        slider->setValue(3);
        QVERIFY(device().contains("cnc=10"));   // dragging alone sends nothing
        button(w, "Apply")->click();
        QTRY_VERIFY(device().contains("cnc=3"));
        button(w, "Close")->click();
        QVERIFY(!w->isVisible());
    }

    void equalizerTryAndReset() {
        QVERIFY(control->Trigger("Equalizer..."));
        QWidget* w = window("Equalizer - bosectl");
        QTRY_VERIFY(w && w->isVisible());
        auto sliders = w->findChildren<QSlider*>();
        QCOMPARE(sliders.size(), 3);
        QCOMPARE(sliders[0]->value(), 2);
        QCOMPARE(sliders[2]->value(), -1);
        sliders[0]->setValue(-6);
        sliders[1]->setValue(4);
        button(w, "Try")->click();
        QTRY_VERIFY(device().contains("eq=-6,4,-1"));
        button(w, "Reset")->click();
        QTRY_VERIFY(device().contains("eq=0,0,0"));
        QCOMPARE(sliders[0]->value(), 0);
    }

    void modesListAndActivate() {
        QVERIFY(control->Trigger("Modes..."));
        QWidget* w = window("Modes - bosectl");
        auto* list = w->findChild<QListWidget*>();
        QTRY_COMPARE(list->count(), 6);
        QCOMPARE(list->item(0)->text(), QString("quiet  ◀  [built-in]"));
        QCOMPARE(list->item(4)->text(), QString("Commute"));
        QCOMPARE(list->item(5)->text(), QString("Focus"));

        // A preset: read-only editor, Delete off, Activate off while active.
        list->setCurrentRow(0);
        QVERIFY(!button(w, "Delete")->isEnabled());
        QVERIFY(!button(w, "Activate")->isEnabled());
        QVERIFY(!button(w, "Save")->isEnabled());

        list->setCurrentRow(4);
        QVERIFY(button(w, "Delete")->isEnabled());
        QVERIFY(button(w, "Activate")->isEnabled());
        QCOMPARE(w->findChild<QLineEdit*>()->text(), QString("Commute"));
        QCOMPARE(w->findChild<QSlider*>()->value(), 7);
        button(w, "Activate")->click();
        QTRY_VERIFY(device().contains("mode=4 cnc=7"));
        QTRY_COMPARE(list->item(4)->text(), QString("Commute  ◀"));
        QTRY_VERIFY(control->Tooltip().contains("Mode: Commute"));
    }

    void modesEditSave() {
        QVERIFY(control->Trigger("Modes..."));
        QWidget* w = window("Modes - bosectl");
        auto* list = w->findChild<QListWidget*>();
        QTRY_COMPARE(list->count(), 6);
        list->setCurrentRow(5);
        w->findChild<QLineEdit*>()->setText("Deep Focus");
        w->findChild<QSlider*>()->setValue(8);
        w->findChild<QComboBox*>()->setCurrentIndex(1);
        button(w, "Save")->click();
        QTRY_COMPARE(list->item(5)->text(), QString("Deep Focus"));
        list->setCurrentRow(5);
        QCOMPARE(w->findChild<QSlider*>()->value(), 8);
        QCOMPARE(w->findChild<QComboBox*>()->currentIndex(), 1);
    }

    void modesCreateAndDelete() {
        QVERIFY(control->Trigger("Modes..."));
        QWidget* w = window("Modes - bosectl");
        auto* list = w->findChild<QListWidget*>();
        QTRY_COMPARE(list->count(), 6);
        list->setCurrentRow(4);
        answerNextDialog("Travel");
        button(w, "New")->click();
        QTRY_COMPARE(list->count(), 7);
        QCOMPARE(list->item(6)->text(), QString("Travel"));

        list->setCurrentRow(6);
        answerNextDialog();
        button(w, "Delete")->click();
        QTRY_COMPARE(list->count(), 6);
    }

    void busyShowsInWindows() {
        sim->setLatency(150);
        QVERIFY(control->Trigger("Noise Cancellation..."));
        QWidget* w = window("Noise Cancellation - bosectl");
        QTRY_VERIFY(w && w->isVisible());
        w->findChild<QSlider*>()->setValue(5);
        button(w, "Apply")->click();
        QTRY_VERIFY(control->Widgets("Noise Cancellation - bosectl").contains("[Working...]"));
        QTRY_VERIFY(control->Tooltip().contains("working…"));
        QTRY_VERIFY(!control->Widgets("Noise Cancellation - bosectl").contains("[Working...]"));
        QVERIFY(device().contains("cnc=5"));
    }

    void helpOpens() {
        QVERIFY(control->Trigger("Help..."));
        QTRY_VERIFY(!control->Windows().filter("Help").isEmpty());
    }

    // ── Sim control surface ────────────────────────────────────────────────

    void widgetsListsWindowContents() {
        QVERIFY(control->Trigger("Equalizer..."));
        QTRY_VERIFY(control->Windows().contains("Equalizer - bosectl"));
        const QString widgets = control->Widgets("Equalizer - bosectl");
        QVERIFY2(widgets.contains(QRegularExpression("QSlider \\d+ \\d+ \\d+ \\d+ value=2")),
                 qPrintable(widgets));
        QVERIFY(widgets.contains(QRegularExpression("QPushButton \\d+ \\d+ \\d+ \\d+ \\[Try\\]")));
        QVERIFY(control->Close("Equalizer - bosectl"));
        QVERIFY(!control->Windows().contains("Equalizer - bosectl"));
    }
};

QTEST_MAIN(TestTray)
#include "test_tray.moc"
