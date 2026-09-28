#pragma once

#include <QGroupBox>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

#include "HelpContent.h"

class ExplanationFrame;
class QPushButton;

// A map of the app: the tray menu and the three dialogs laid out side by
// side as static replicas built from ordinary Qt widgets, so they render in
// the desktop theme. Clicking a replica item opens its explanation from
// help.md directly beneath it; clicking it again closes it. Only one
// explanation is open at a time. Submenus expand inline.
//
// Nothing here is wired to the headphones or to Settings: replica sliders,
// lists and checkboxes swallow input, and replica buttons only open help.
// A plain top-level window with no popups, so it behaves the same on Wayland.
class HelpWindow : public QWidget {
    Q_OBJECT

public:
    explicit HelpWindow(QWidget* parent = nullptr);

    // Every help.md id the window shows — frames, items and choices. The
    // tests hold help.md to exactly this set so the two cannot drift.
    static QStringList helpIds();

    // Id of the explanation currently open, or empty.
    QString openId() const { return openId_; }

private:
    QGroupBox* makeFrame(const QString& id, const QString& fallbackTitle,
                         int width, QVBoxLayout*& layout);
    QWidget* buildMenuFrame();
    QWidget* buildNcFrame();
    QWidget* buildModesFrame();
    QWidget* buildEqFrame();

    // Adds a hidden explanation frame to `layout`. Items bound to it open
    // their text there, i.e. beneath the row that precedes it.
    ExplanationFrame* addSlot(QBoxLayout* layout);

    // Makes `w` (and any `also` widgets, e.g. its label) open `id` in `slot`.
    void bind(QWidget* w, const QString& id, ExplanationFrame* slot,
              const QList<QWidget*>& also = {});
    void catchClicks(QWidget* w, std::function<void()> onClick);

    void toggle(QWidget* source, const QString& id, ExplanationFrame* slot);
    void closeExplanation();
    QString markdownFor(const QString& id) const;

    const HelpContent& help_;
    QScrollArea* scroll_;

    QWidget* openSource_ = nullptr;
    ExplanationFrame* openSlot_ = nullptr;
    QString openId_;
};
