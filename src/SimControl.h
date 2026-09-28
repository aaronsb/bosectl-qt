#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

class QMenu;
class SimDevice;
class TrayIcon;

// The harness's handle on a sim-mode tray: org.bosectl.qt /Sim on the session
// bus (a nest's own bus under dev/nest.sh). Only main.cpp's sim branch
// registers it; a normal run exports nothing.
//
//   qdbus6 org.bosectl.qt /Sim Menu
//   qdbus6 org.bosectl.qt /Sim Trigger "Noise Cancellation..."
//   qdbus6 org.bosectl.qt /Sim Widgets "Noise Cancellation"
//
// Menu paths join entry texts with '/', mnemonics removed:
// "About/About bosectl-qt...", "Spatial Audio/Room".
class SimControl : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bosectl.qt.Sim")

public:
    SimControl(TrayIcon* tray, std::shared_ptr<SimDevice> sim, QObject* parent = nullptr);

    // Register on the session bus as org.bosectl.qt /Sim.
    bool registerOnBus();

    // The tray menu, one line per entry, indented two spaces per level:
    // "text [disabled] [checked]". Separators are "-".
    static QString describeMenu(const QMenu* menu);

public slots:
    QString Menu() const;
    // Trigger the entry at PATH on the next event-loop turn, so an entry that
    // opens a modal dialog does not hold up the reply. False if no such
    // enabled entry exists.
    bool Trigger(const QString& path);
    // Titles of the visible top-level windows.
    QStringList Windows() const;
    // A window's child widgets, one per line:
    // "Class x y w h [text] [value=N] [checked] [disabled]"
    // with geometry relative to the window's client area.
    QString Widgets(const QString& title) const;
    // Close the window with this title; empty closes every visible one.
    bool Close(const QString& title);
    // Render the window with this title to a PNG, without decorations.
    bool Grab(const QString& title, const QString& path) const;
    QString Tooltip() const;
    // What left-clicking the tray icon does: the status notification.
    void Notify();
    void Refresh();

    QString Device() const;
    // The headset's starting state again, then a refresh.
    void Reset();
    void SetBattery(int pct);
    void SetReachable(bool on);
    void SetLatency(int ms);

    void Quit();

private:
    TrayIcon* tray_;
    std::shared_ptr<SimDevice> sim_;
};
