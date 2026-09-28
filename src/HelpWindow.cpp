#include "HelpWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QSlider>
#include <QTimer>

// ── Replica building blocks ─────────────────────────────────────────────────

// The box an explanation appears in: help.md Markdown in a word-wrapped
// label, set off from the replica by the theme's highlight colour.
class ExplanationFrame : public QFrame {
public:
    ExplanationFrame() {
        setObjectName("helpExplanation");
        setStyleSheet("QFrame#helpExplanation {"
                      " background: palette(base);"
                      " border: 1px solid palette(mid);"
                      " border-left: 3px solid palette(highlight);"
                      " border-radius: 3px; }");
        auto* l = new QVBoxLayout(this);
        l->setContentsMargins(10, 8, 10, 8);
        label_ = new QLabel;
        label_->setTextFormat(Qt::MarkdownText);
        label_->setWordWrap(true);
        label_->setOpenExternalLinks(true);
        label_->setTextInteractionFlags(Qt::TextBrowserInteraction);
        l->addWidget(label_);
        hide();
    }

    void setMarkdown(const QString& md) { label_->setText(md); }

private:
    QLabel* label_;
};

namespace {

// One row of the tray-menu replica: a flat button holding an indicator
// column (checkbox for toggles, radio for choices, blank otherwise, like a
// real QMenu's check column), the label, and a ▸ for submenus. Checkable so
// the row stays highlighted while its explanation is open.
class MenuRow : public QPushButton {
public:
    enum Indicator { None, Check, Radio };

    MenuRow(const QString& text, Indicator indicator, bool submenu) {
        setFlat(true);
        setCheckable(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(6, 3, 8, 3);
        row->setSpacing(6);

        QWidget* mark = indicator == Radio ? static_cast<QWidget*>(new QRadioButton)
                                           : static_cast<QWidget*>(new QCheckBox);
        mark->setAttribute(Qt::WA_TransparentForMouseEvents);
        mark->setFocusPolicy(Qt::NoFocus);
        QSizePolicy keep = mark->sizePolicy();
        keep.setRetainSizeWhenHidden(true);
        mark->setSizePolicy(keep);
        mark->setVisible(indicator != None);
        row->addWidget(mark);

        label_ = new QLabel(text);
        label_->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->addWidget(label_, 1);

        if (submenu) {
            arrow_ = new QLabel(QStringLiteral("▸"));   // ▸
            arrow_->setAttribute(Qt::WA_TransparentForMouseEvents);
            row->addWidget(arrow_);
        }
    }

    void setBold() {
        QFont f = label_->font();
        f.setBold(true);
        label_->setFont(f);
    }

    void setExpanded(bool expanded) {
        if (arrow_) arrow_->setText(expanded ? QStringLiteral("▾")    // ▾
                                             : QStringLiteral("▸"));  // ▸
    }

    QSize sizeHint() const override { return layout()->sizeHint(); }
    QSize minimumSizeHint() const override { return layout()->minimumSize(); }

private:
    QLabel* label_;
    QLabel* arrow_ = nullptr;
};

// Makes a real slider/list/combo/checkbox inert and turns a click (or
// Space/Enter when focused) on it into a help request. Everything that
// would change its value or open a popup is swallowed; Tab still moves on.
class ClickCatcher : public QObject {
public:
    ClickCatcher(QObject* parent, std::function<void()> onClick)
        : QObject(parent), onClick_(std::move(onClick)) {}

protected:
    bool eventFilter(QObject* obj, QEvent* e) override {
        switch (e->type()) {
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(e);
            auto* w = static_cast<QWidget*>(obj);
            if (me->button() == Qt::LeftButton && w->rect().contains(me->position().toPoint()))
                onClick_();
            return true;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::ContextMenu:
            return true;
        case QEvent::KeyPress:
        case QEvent::KeyRelease: {
            const int key = static_cast<QKeyEvent*>(e)->key();
            if (key == Qt::Key_Tab || key == Qt::Key_Backtab) return false;
            const bool activate = key == Qt::Key_Space || key == Qt::Key_Return
                                  || key == Qt::Key_Enter;
            if (activate && e->type() == QEvent::KeyPress
                && !static_cast<QKeyEvent*>(e)->isAutoRepeat())
                onClick_();
            return true;
        }
        default:
            return false;
        }
    }

private:
    std::function<void()> onClick_;
};

QLabel* mutedLabel(const QString& text) {
    auto* l = new QLabel(text);
    QFont f = l->font();
    f.setPointSize(f.pointSize() - 1);
    l->setFont(f);
    l->setStyleSheet("color: gray;");
    return l;
}

QFrame* separator() {
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

// ── The tray menu, top to bottom ────────────────────────────────────────────
// Mirrors TrayIcon::buildMenu(). A null label takes the help.md heading.
// `child` entries belong to the nearest Submenu above them.

enum class Kind { Action, Toggle, Submenu, Choice, Separator };

struct MenuSpec {
    const char* id;
    const char* label;
    Kind kind;
    bool child;
};

const MenuSpec kMenu[] = {
    {"menu.device",              "Bose Headphones",          Kind::Submenu,   false},
    {"menu.rename",              "Rename...",                Kind::Action,    true},
    {"menu.battery",             "Battery: 70%",             Kind::Action,    false},
    {"menu.about",               "About",                    Kind::Submenu,   false},
    {"menu.firmware",            nullptr,                    Kind::Action,    true},
    {"menu.mac",                 nullptr,                    Kind::Action,    true},
    {nullptr,                    nullptr,                    Kind::Separator, true},
    // The About dialog has no entry of its own; it explains the submenu.
    {"menu.about",               "About bosectl-qt...",      Kind::Action,    true},
    {"menu.help",                "Help...",                  Kind::Action,    false},
    {nullptr,                    nullptr,                    Kind::Separator, false},
    {"menu.noise-cancellation",  "Noise Cancellation...",    Kind::Action,    false},
    {"menu.modes",               "Modes...",                 Kind::Action,    false},
    {"menu.equalizer",           "Equalizer...",             Kind::Action,    false},
    {"menu.spatial",             "Spatial Audio",            Kind::Submenu,   false},
    {"menu.spatial.off",         "Off",                      Kind::Choice,    true},
    {"menu.spatial.room",        "Room",                     Kind::Choice,    true},
    {"menu.spatial.head",        "Head Tracking",            Kind::Choice,    true},
    {"menu.sidetone",            "Sidetone",                 Kind::Submenu,   false},
    {"menu.sidetone.off",        "Off",                      Kind::Choice,    true},
    {"menu.sidetone.low",        "Low",                      Kind::Choice,    true},
    {"menu.sidetone.medium",     "Medium",                   Kind::Choice,    true},
    {"menu.sidetone.high",       "High",                     Kind::Choice,    true},
    {nullptr,                    nullptr,                    Kind::Separator, false},
    {"menu.anc",                 "Noise Cancellation (ANC)", Kind::Toggle,    false},
    {"menu.wind-block",          "Wind Block",               Kind::Toggle,    false},
    {"menu.multipoint",          "Multipoint",               Kind::Toggle,    false},
    {"menu.auto-pause",          "Auto-Pause",               Kind::Toggle,    false},
    {nullptr,                    nullptr,                    Kind::Separator, false},
    {"menu.connect",             "Connect",                  Kind::Action,    false},
    {"menu.power-off",           "Power Off",                Kind::Action,    false},
    {nullptr,                    nullptr,                    Kind::Separator, false},
    {"menu.start-on-login",      "Start on Login",           Kind::Toggle,    false},
    {"menu.quit",                "Quit",                     Kind::Action,    false},
};

// Dialog controls, in the order the builders below bind them.
const char* const kDialogIds[] = {
    "nc.slider", "nc.apply",
    "modes.list", "modes.new", "modes.delete", "modes.name", "modes.cnc",
    "modes.spatial", "modes.wind-block", "modes.anc-toggle",
    "modes.activate", "modes.save",
    "eq.bass", "eq.mid", "eq.treble", "eq.try", "eq.save", "eq.reset",
};

const char* const kFrameIds[] = {"menu", "nc", "modes", "eq"};

// Column widths. The middle column holds both Noise Cancellation and Modes.
constexpr int kMenuWidth = 300;
constexpr int kMiddleWidth = 470;
constexpr int kEqWidth = 280;

}  // namespace

// ── Window ──────────────────────────────────────────────────────────────────

QStringList HelpWindow::helpIds() {
    QStringList ids;
    for (const char* id : kFrameIds) ids << id;
    for (const auto& m : kMenu)
        if (m.id && !ids.contains(m.id)) ids << m.id;
    for (const char* id : kDialogIds) ids << id;
    return ids;
}

HelpWindow::HelpWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
    , help_(HelpContent::shared())
{
    setWindowTitle("Help - bosectl");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto* canvas = new QWidget;
    auto* canvasLayout = new QVBoxLayout(canvas);
    canvasLayout->setSpacing(12);

    if (!help_.intro().isEmpty()) {
        auto* intro = new QLabel(help_.intro());
        intro->setTextFormat(Qt::MarkdownText);
        intro->setWordWrap(true);
        intro->setOpenExternalLinks(true);
        canvasLayout->addWidget(intro);
    }

    // Three columns, laid out like the mockup: the menu on the left, the
    // two slider dialogs stacked in the middle, the equalizer on the right.
    auto* columns = new QHBoxLayout;
    columns->setSpacing(12);

    auto* left = new QVBoxLayout;
    left->addWidget(buildMenuFrame());
    left->addStretch();

    auto* middle = new QVBoxLayout;
    middle->setSpacing(12);
    middle->addWidget(buildNcFrame());
    middle->addWidget(buildModesFrame());
    middle->addStretch();

    auto* right = new QVBoxLayout;
    right->addWidget(buildEqFrame());
    right->addStretch();

    columns->addLayout(left);
    columns->addLayout(middle);
    columns->addLayout(right);
    columns->addStretch();
    canvasLayout->addLayout(columns);
    canvasLayout->addStretch();

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidget(canvas);
    root->addWidget(scroll_, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* closeBtn = new QPushButton("Close");
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

    // Wide enough for all three columns side by side, but never larger than
    // the screen; the scroll area takes up whatever doesn't fit.
    QSize size(kMenuWidth + kMiddleWidth + kEqWidth + 110, 780);
    if (const QScreen* screen = QGuiApplication::primaryScreen())
        size = size.boundedTo(screen->availableSize() * 0.9);
    resize(size);
}

QGroupBox* HelpWindow::makeFrame(const QString& id, const QString& fallbackTitle,
                                 int width, QVBoxLayout*& layout) {
    const QString title = help_.title(id);
    auto* box = new QGroupBox(title.isEmpty() ? fallbackTitle : title);
    box->setFixedWidth(width);
    layout = new QVBoxLayout(box);
    layout->setSpacing(6);

    // A frame's own help.md body, if any, is a caption under its title.
    if (!help_.body(id).isEmpty()) {
        auto* caption = mutedLabel(help_.body(id));
        caption->setTextFormat(Qt::MarkdownText);
        caption->setWordWrap(true);
        caption->setOpenExternalLinks(true);
        layout->addWidget(caption);
    }
    return box;
}

ExplanationFrame* HelpWindow::addSlot(QBoxLayout* layout) {
    auto* slot = new ExplanationFrame;
    layout->addWidget(slot);
    return slot;
}

void HelpWindow::catchClicks(QWidget* w, std::function<void()> onClick) {
    auto* catcher = new ClickCatcher(w, std::move(onClick));
    w->installEventFilter(catcher);
    // Lists take their mouse input on the viewport, not the widget.
    if (auto* area = qobject_cast<QAbstractScrollArea*>(w))
        area->viewport()->installEventFilter(catcher);
    w->setCursor(Qt::PointingHandCursor);
}

void HelpWindow::bind(QWidget* w, const QString& id, ExplanationFrame* slot,
                      const QList<QWidget*>& also) {
    w->setProperty("helpId", id);
    auto open = [this, w, id, slot] { toggle(w, id, slot); };
    if (auto* button = qobject_cast<QPushButton*>(w)) {
        connect(button, &QPushButton::clicked, this, open);
        button->setCursor(Qt::PointingHandCursor);
    } else {
        catchClicks(w, open);
        w->setFocusPolicy(Qt::TabFocus);
    }
    for (QWidget* extra : also) catchClicks(extra, open);
}

QString HelpWindow::markdownFor(const QString& id) const {
    const QString body = help_.body(id);
    if (!body.isEmpty()) return body;
    return QString("*No help written for `%1` yet.*").arg(id);
}

void HelpWindow::toggle(QWidget* source, const QString& id, ExplanationFrame* slot) {
    const bool wasOpen = openSource_ == source;
    closeExplanation();
    if (wasOpen) return;

    slot->setMarkdown(markdownFor(id));
    slot->show();
    openSource_ = source;
    openSlot_ = slot;
    openId_ = id;
    if (auto* row = dynamic_cast<MenuRow*>(source)) row->setChecked(true);

    // Scroll once the layout has made room for the frame.
    QTimer::singleShot(0, this, [this, slot] {
        if (openSlot_ == slot) scroll_->ensureWidgetVisible(slot, 16, 16);
    });
}

void HelpWindow::closeExplanation() {
    if (openSlot_) openSlot_->hide();
    if (auto* row = dynamic_cast<MenuRow*>(openSource_)) row->setChecked(false);
    openSource_ = nullptr;
    openSlot_ = nullptr;
    openId_.clear();
}

// ── Tray menu ───────────────────────────────────────────────────────────────

QWidget* HelpWindow::buildMenuFrame() {
    QVBoxLayout* frame;
    auto* box = makeFrame("menu", "Tray menu", kMenuWidth, frame);
    frame->setSpacing(0);

    QVBoxLayout* target = frame;   // current submenu's layout while in one
    for (const auto& spec : kMenu) {
        QVBoxLayout* into = spec.child ? target : frame;
        if (spec.kind == Kind::Separator) {
            into->addSpacing(3);
            into->addWidget(separator());
            into->addSpacing(3);
            continue;
        }

        const QString id = spec.id;
        const QString label = spec.label ? QString(spec.label) : help_.title(id);
        const auto indicator = spec.kind == Kind::Toggle ? MenuRow::Check
                             : spec.kind == Kind::Choice ? MenuRow::Radio
                                                         : MenuRow::None;
        auto* row = new MenuRow(label.isEmpty() ? id : label, indicator,
                                spec.kind == Kind::Submenu);
        into->addWidget(row);
        ExplanationFrame* slot = addSlot(into);

        if (spec.kind != Kind::Submenu) {
            bind(row, id, slot);
            continue;
        }

        if (id == "menu.device") row->setBold();

        // Submenu: its choices sit in an indented, initially hidden block.
        // Opening the submenu also opens its explanation; closing it closes
        // whatever explanation was open inside it.
        auto* children = new QWidget;
        target = new QVBoxLayout(children);
        target->setContentsMargins(18, 0, 0, 0);
        target->setSpacing(0);
        children->hide();
        frame->addWidget(children);

        row->setProperty("helpId", id);
        row->setCursor(Qt::PointingHandCursor);
        connect(row, &QPushButton::clicked, this, [this, row, id, slot, children] {
            const bool expand = children->isHidden();
            children->setVisible(expand);
            row->setExpanded(expand);
            if (expand) {
                if (openSource_ != row) toggle(row, id, slot);
                else row->setChecked(true);
            } else {
                if (openSource_ == row || (openSlot_ && children->isAncestorOf(openSlot_)))
                    closeExplanation();
                row->setChecked(false);
            }
        });
    }
    return box;
}

// ── Noise Cancellation dialog ───────────────────────────────────────────────

QWidget* HelpWindow::buildNcFrame() {
    QVBoxLayout* frame;
    auto* box = makeFrame("nc", "Noise Cancellation", kMiddleWidth, frame);

    auto* title = new QLabel("Noise Cancellation");
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    title->setAlignment(Qt::AlignCenter);
    frame->addWidget(title);

    auto* hint = mutedLabel("Requires ANC on and Wind Block off");
    hint->setAlignment(Qt::AlignCenter);
    frame->addWidget(hint);

    auto* endpointRow = new QHBoxLayout;
    auto* minLabel = new QLabel("Max NC");
    auto* maxLabel = new QLabel("Ambient");
    endpointRow->addWidget(minLabel);
    endpointRow->addStretch();
    endpointRow->addWidget(maxLabel);
    frame->addLayout(endpointRow);

    auto* sliderRow = new QHBoxLayout;
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 10);
    slider->setValue(5);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(1);
    auto* value = new QLabel("5");
    value->setMinimumWidth(32);
    value->setAlignment(Qt::AlignCenter);
    QFont valFont = value->font();
    valFont.setBold(true);
    valFont.setPointSize(valFont.pointSize() + 2);
    value->setFont(valFont);
    sliderRow->addWidget(slider, 1);
    sliderRow->addWidget(value);
    frame->addLayout(sliderRow);
    bind(slider, "nc.slider", addSlot(frame), {minLabel, maxLabel, value});

    auto* btnRow = new QHBoxLayout;
    auto* apply = new QPushButton("Apply");
    btnRow->addWidget(apply);
    btnRow->addStretch();
    auto* close = new QPushButton("Close");   // the replica's; does nothing
    close->setEnabled(false);             // shown for layout only
    close->setFocusPolicy(Qt::NoFocus);
    btnRow->addWidget(close);
    frame->addLayout(btnRow);
    bind(apply, "nc.apply", addSlot(frame));

    return box;
}

// ── Modes dialog ────────────────────────────────────────────────────────────

QWidget* HelpWindow::buildModesFrame() {
    QVBoxLayout* frame;
    auto* box = makeFrame("modes", "Modes", kMiddleWidth, frame);

    auto* cols = new QHBoxLayout;
    cols->setSpacing(12);

    // Left: list and its buttons
    auto* leftCol = new QVBoxLayout;
    auto* listLabel = new QLabel("Modes");
    QFont boldFont = listLabel->font();
    boldFont.setBold(true);
    listLabel->setFont(boldFont);
    leftCol->addWidget(listLabel);

    // Sample modes, labelled the way ModeWindow::setModes() labels them.
    auto* list = new QListWidget;
    for (const QString& name : {QString("Quiet  ◀  [built-in]"),
                                QString("Aware  [built-in]"),
                                QString("Immersion  [built-in]"),
                                QString("Commute")}) {
        auto* item = new QListWidgetItem(name, list);
        if (name.startsWith("Quiet")) item->setFont(boldFont);
    }
    list->setCurrentRow(3);
    list->setFixedWidth(170);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setFixedHeight(list->sizeHintForRow(0) * list->count() + 2 * list->frameWidth() + 4);
    leftCol->addWidget(list);
    bind(list, "modes.list", addSlot(leftCol), {listLabel});

    auto* listButtons = new QHBoxLayout;
    auto* newBtn = new QPushButton("New");
    auto* deleteBtn = new QPushButton("Delete");
    listButtons->addWidget(newBtn);
    listButtons->addWidget(deleteBtn);
    listButtons->addStretch();
    leftCol->addLayout(listButtons);
    ExplanationFrame* listButtonSlot = addSlot(leftCol);
    bind(newBtn, "modes.new", listButtonSlot);
    bind(deleteBtn, "modes.delete", listButtonSlot);
    leftCol->addStretch();

    // Right: the editor and the action buttons
    auto* rightCol = new QVBoxLayout;
    auto* editor = new QGroupBox("Mode Settings");
    auto* editorLayout = new QVBoxLayout(editor);
    editorLayout->setSpacing(6);

    auto* nameRow = new QHBoxLayout;
    auto* nameLabel = new QLabel("Name:");
    auto* nameEdit = new QLineEdit("Commute");
    nameEdit->setReadOnly(true);
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(nameEdit);
    editorLayout->addLayout(nameRow);
    bind(nameEdit, "modes.name", addSlot(editorLayout), {nameLabel});

    auto* cncRow = new QHBoxLayout;
    auto* cncLabel = new QLabel("Noise Cancel:");
    auto* cncSlider = new QSlider(Qt::Horizontal);
    cncSlider->setRange(0, 10);
    cncSlider->setValue(7);
    cncSlider->setTickPosition(QSlider::TicksBelow);
    cncSlider->setTickInterval(1);
    auto* cncValue = new QLabel("7");
    cncValue->setMinimumWidth(20);
    cncRow->addWidget(cncLabel);
    cncRow->addWidget(cncSlider, 1);
    cncRow->addWidget(cncValue);
    editorLayout->addLayout(cncRow);
    bind(cncSlider, "modes.cnc", addSlot(editorLayout), {cncLabel, cncValue});

    auto* spatialRow = new QHBoxLayout;
    auto* spatialLabel = new QLabel("Spatial:");
    auto* spatialCombo = new QComboBox;
    spatialCombo->addItems({"Off", "Room", "Head Tracking"});
    spatialRow->addWidget(spatialLabel);
    spatialRow->addWidget(spatialCombo, 1);
    editorLayout->addLayout(spatialRow);
    bind(spatialCombo, "modes.spatial", addSlot(editorLayout), {spatialLabel});

    auto* wind = new QCheckBox("Wind Block");
    editorLayout->addWidget(wind);
    bind(wind, "modes.wind-block", addSlot(editorLayout));

    auto* anc = new QCheckBox("ANC Toggle");
    anc->setChecked(true);
    editorLayout->addWidget(anc);
    bind(anc, "modes.anc-toggle", addSlot(editorLayout));

    rightCol->addWidget(editor);

    auto* actionRow = new QHBoxLayout;
    auto* activate = new QPushButton("Activate");
    auto* save = new QPushButton("Save");
    auto* close = new QPushButton("Close");   // the replica's; does nothing
    close->setEnabled(false);             // shown for layout only
    close->setFocusPolicy(Qt::NoFocus);
    actionRow->addWidget(activate);
    actionRow->addWidget(save);
    actionRow->addStretch();
    actionRow->addWidget(close);
    rightCol->addLayout(actionRow);
    ExplanationFrame* actionSlot = addSlot(rightCol);
    bind(activate, "modes.activate", actionSlot);
    bind(save, "modes.save", actionSlot);
    rightCol->addStretch();

    cols->addLayout(leftCol);
    cols->addLayout(rightCol, 1);
    frame->addLayout(cols);
    return box;
}

// ── Equalizer dialog ────────────────────────────────────────────────────────

QWidget* HelpWindow::buildEqFrame() {
    QVBoxLayout* frame;
    auto* box = makeFrame("eq", "Equalizer", kEqWidth, frame);

    auto* bands = new QGroupBox("EQ Bands");
    auto* bandLayout = new QHBoxLayout(bands);
    bandLayout->setSpacing(20);
    frame->addWidget(bands);
    // The three bands share one row, so they share the frame beneath it.
    ExplanationFrame* bandSlot = addSlot(frame);

    auto makeBand = [&](const QString& name, int v, const char* id) {
        auto* col = new QVBoxLayout;
        auto* value = new QLabel(QString::number(v));
        value->setAlignment(Qt::AlignCenter);
        value->setMinimumWidth(24);
        QFont valFont = value->font();
        valFont.setBold(true);
        value->setFont(valFont);

        auto* slider = new QSlider(Qt::Vertical);
        slider->setRange(-10, 10);
        slider->setValue(v);
        slider->setMinimumHeight(120);
        slider->setTickPosition(QSlider::TicksBothSides);
        slider->setTickInterval(5);

        auto* nameLabel = new QLabel(name);
        nameLabel->setAlignment(Qt::AlignCenter);

        col->addWidget(value, 0, Qt::AlignCenter);
        col->addWidget(slider, 1, Qt::AlignCenter);
        col->addWidget(nameLabel, 0, Qt::AlignCenter);
        bandLayout->addLayout(col);
        bind(slider, id, bandSlot, {value, nameLabel});
    };
    makeBand("Bass", 3, "eq.bass");
    makeBand("Mid", 0, "eq.mid");
    makeBand("Treble", -2, "eq.treble");

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    auto* tryBtn = new QPushButton("Try");
    auto* saveBtn = new QPushButton("Save");
    auto* resetBtn = new QPushButton("Reset");
    auto* close = new QPushButton("Close");   // the replica's; does nothing
    close->setEnabled(false);             // shown for layout only
    close->setFocusPolicy(Qt::NoFocus);
    buttonRow->addWidget(tryBtn);
    buttonRow->addWidget(saveBtn);
    buttonRow->addWidget(resetBtn);
    buttonRow->addStretch();
    buttonRow->addWidget(close);
    frame->addLayout(buttonRow);
    ExplanationFrame* buttonSlot = addSlot(frame);
    bind(tryBtn, "eq.try", buttonSlot);
    bind(saveBtn, "eq.save", buttonSlot);
    bind(resetBtn, "eq.reset", buttonSlot);

    return box;
}
