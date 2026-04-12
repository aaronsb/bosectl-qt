#include "EqWindow.h"

EqWindow::EqWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle("Equalizer - bosectl");
    buildUi();
    setFixedWidth(260);
}

void EqWindow::setCurrentEq(int8_t bass, int8_t mid, int8_t treble) {
    bassSlider_->blockSignals(true);
    bassSlider_->setValue(bass);
    bassSlider_->blockSignals(false);
    bassLabel_->setText(QString::number(bass));

    midSlider_->blockSignals(true);
    midSlider_->setValue(mid);
    midSlider_->blockSignals(false);
    midLabel_->setText(QString::number(mid));

    trebleSlider_->blockSignals(true);
    trebleSlider_->setValue(treble);
    trebleSlider_->blockSignals(false);
    trebleLabel_->setText(QString::number(treble));
}

void EqWindow::setSavedEq(int8_t bass, int8_t mid, int8_t treble) {
    savedBass_ = bass;
    savedMid_ = mid;
    savedTreble_ = treble;
}

void EqWindow::onTry() {
    emit eqTry(bassSlider_->value(), midSlider_->value(), trebleSlider_->value());
}

void EqWindow::onSave() {
    savedBass_ = bassSlider_->value();
    savedMid_ = midSlider_->value();
    savedTreble_ = trebleSlider_->value();
    emit eqSave(savedBass_, savedMid_, savedTreble_);
}

void EqWindow::onReset() {
    setCurrentEq(0, 0, 0);
    emit eqTry(0, 0, 0);
}

void EqWindow::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(12);

    // ── Sliders ─────────────────────────────────────────────────────────────
    auto* sliderGroup = new QGroupBox("EQ Bands");
    auto* sliderLayout = new QHBoxLayout(sliderGroup);
    sliderLayout->setSpacing(20);

    auto makeBand = [&](const QString& name, QSlider*& slider, QLabel*& label) {
        auto* col = new QVBoxLayout;
        col->setAlignment(Qt::AlignHCenter);

        label = new QLabel("0");
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumWidth(24);
        QFont valFont = label->font();
        valFont.setBold(true);
        label->setFont(valFont);

        slider = new QSlider(Qt::Vertical);
        slider->setRange(-10, 10);
        slider->setValue(0);
        slider->setMinimumHeight(120);
        slider->setTickPosition(QSlider::TicksBothSides);
        slider->setTickInterval(5);

        auto* nameLabel = new QLabel(name);
        nameLabel->setAlignment(Qt::AlignCenter);

        col->addWidget(label, 0, Qt::AlignCenter);
        col->addWidget(slider, 1, Qt::AlignCenter);
        col->addWidget(nameLabel, 0, Qt::AlignCenter);

        sliderLayout->addLayout(col);

        connect(slider, &QSlider::valueChanged, label, [label](int v) {
            label->setText(QString::number(v));
        });
    };

    makeBand("Bass", bassSlider_, bassLabel_);
    makeBand("Mid", midSlider_, midLabel_);
    makeBand("Treble", trebleSlider_, trebleLabel_);

    root->addWidget(sliderGroup);

    // ── Buttons ─────────────────────────────────────────────────────────────
    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);

    auto* tryBtn = new QPushButton("Try");
    tryBtn->setToolTip("Apply EQ without saving");
    auto* saveBtn = new QPushButton("Save");
    saveBtn->setToolTip("Apply and save EQ settings");
    auto* resetBtn = new QPushButton("Reset");
    resetBtn->setToolTip("Reset to last saved values");
    auto* closeBtn = new QPushButton("Close");

    buttonRow->addWidget(tryBtn);
    buttonRow->addWidget(saveBtn);
    buttonRow->addWidget(resetBtn);
    buttonRow->addStretch();
    buttonRow->addWidget(closeBtn);

    statusLabel_ = new QLabel;
    statusLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(statusLabel_);

    root->addLayout(buttonRow);

    connect(tryBtn, &QPushButton::clicked, this, &EqWindow::onTry);
    connect(saveBtn, &QPushButton::clicked, this, &EqWindow::onSave);
    connect(resetBtn, &QPushButton::clicked, this, &EqWindow::onReset);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
}
