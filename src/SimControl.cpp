#include "SimControl.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDBusConnection>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QSpinBox>
#include <QTimer>

#include "Logging.h"
#include "SimDevice.h"
#include "TrayIcon.h"

namespace {

QString plain(QString text) {
    // "&Quit" -> "Quit", "&&" -> "&"
    text.replace("&&", QChar(0x1));
    text.remove('&');
    text.replace(QChar(0x1), "&");
    return text;
}

void describeInto(const QMenu* menu, int depth, QStringList& out) {
    const QString indent(depth * 2, ' ');
    for (QAction* a : menu->actions()) {
        if (!a->isVisible()) continue;
        if (a->isSeparator()) { out << indent + "-"; continue; }
        QString line = indent + plain(a->text());
        if (!a->isEnabled()) line += " [disabled]";
        if (a->isCheckable() && a->isChecked()) line += " [checked]";
        out << line;
        if (a->menu()) describeInto(a->menu(), depth + 1, out);
    }
}

QAction* findAction(const QMenu* menu, const QStringList& parts) {
    if (parts.isEmpty()) return nullptr;
    for (QAction* a : menu->actions()) {
        if (a->isSeparator() || plain(a->text()) != parts.first()) continue;
        if (parts.size() == 1) return a;
        if (a->menu()) return findAction(a->menu(), parts.mid(1));
    }
    return nullptr;
}

QWidget* findWindow(const QString& title) {
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isVisible() && w->windowTitle() == title) return w;
    }
    return nullptr;
}

QString describeWidget(const QWidget* w, const QWidget* window) {
    const QPoint at = w->mapTo(window, QPoint(0, 0));
    QString line = QString("%1 %2 %3 %4 %5")
                       .arg(w->metaObject()->className())
                       .arg(at.x()).arg(at.y()).arg(w->width()).arg(w->height());
    QString text;
    if (auto* b = qobject_cast<const QAbstractButton*>(w)) text = plain(b->text());
    else if (auto* l = qobject_cast<const QLabel*>(w)) text = l->text();
    else if (auto* e = qobject_cast<const QLineEdit*>(w)) text = e->text();
    else if (auto* c = qobject_cast<const QComboBox*>(w)) text = c->currentText();
    else if (auto* lw = qobject_cast<const QListWidget*>(w))
        text = lw->currentItem() ? lw->currentItem()->text() : QString();
    text.replace('\n', ' ');
    if (!text.isEmpty()) line += " [" + text + "]";
    if (auto* s = qobject_cast<const QAbstractSlider*>(w)) line += QString(" value=%1").arg(s->value());
    if (auto* s = qobject_cast<const QSpinBox*>(w)) line += QString(" value=%1").arg(s->value());
    if (auto* b = qobject_cast<const QAbstractButton*>(w); b && b->isCheckable() && b->isChecked())
        line += " [checked]";
    if (!w->isEnabled()) line += " [disabled]";
    return line;
}

}  // namespace

SimControl::SimControl(TrayIcon* tray, std::shared_ptr<SimDevice> sim, QObject* parent)
    : QObject(parent), tray_(tray), sim_(std::move(sim)) {}

bool SimControl::registerOnBus() {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerObject("/Sim", this, QDBusConnection::ExportAllSlots)) return false;
    if (!bus.registerService("org.bosectl.qt")) {
        qCWarning(lcTray) << "sim: org.bosectl.qt is taken on this bus";
        return false;
    }
    return true;
}

QString SimControl::describeMenu(const QMenu* menu) {
    QStringList out;
    describeInto(menu, 0, out);
    return out.join('\n');
}

QString SimControl::Menu() const {
    return describeMenu(tray_->contextMenu());
}

bool SimControl::Trigger(const QString& path) {
    QAction* a = findAction(tray_->contextMenu(), path.split('/'));
    if (!a || !a->isEnabled() || a->menu()) return false;
    // aboutToShow runs the reconnect-on-open and autostart re-read a real
    // click would; a triggered action's menu never shows otherwise.
    emit tray_->contextMenu()->aboutToShow();
    QTimer::singleShot(0, a, &QAction::trigger);
    return true;
}

QStringList SimControl::Windows() const {
    QStringList titles;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isVisible() && !qobject_cast<QMenu*>(w)) titles << w->windowTitle();
    }
    return titles;
}

QString SimControl::Widgets(const QString& title) const {
    QWidget* window = findWindow(title);
    if (!window) return {};
    QStringList out;
    for (QWidget* w : window->findChildren<QWidget*>()) {
        if (!w->isVisibleTo(window)) continue;
        if (qobject_cast<QAbstractButton*>(w) || qobject_cast<QAbstractSlider*>(w)
            || qobject_cast<QLabel*>(w) || qobject_cast<QLineEdit*>(w)
            || qobject_cast<QComboBox*>(w) || qobject_cast<QSpinBox*>(w)
            || qobject_cast<QListWidget*>(w))
            out << describeWidget(w, window);
    }
    return out.join('\n');
}

bool SimControl::Close(const QString& title) {
    bool any = false;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (!w->isVisible() || qobject_cast<QMenu*>(w)) continue;
        if (!title.isEmpty() && w->windowTitle() != title) continue;
        if (auto* d = qobject_cast<QDialog*>(w)) d->reject();
        else w->close();
        any = true;
    }
    return any;
}

bool SimControl::Grab(const QString& title, const QString& path) const {
    QWidget* w = findWindow(title);
    return w && w->grab().save(path, "PNG");
}

QString SimControl::Tooltip() const { return tray_->toolTip(); }

void SimControl::Notify() { emit tray_->activated(QSystemTrayIcon::Trigger); }

void SimControl::Refresh() { tray_->refresh(); }

QString SimControl::Device() const { return QString::fromStdString(sim_->describe()); }

void SimControl::Reset() {
    sim_->reset();
    tray_->refresh();
}

void SimControl::SetBattery(int pct) { sim_->setBattery(static_cast<uint8_t>(qBound(0, pct, 100))); }

void SimControl::SetReachable(bool on) { sim_->setReachable(on); }

void SimControl::SetLatency(int ms) { sim_->setLatency(ms); }

void SimControl::Quit() { QTimer::singleShot(0, qApp, &QApplication::quit); }
