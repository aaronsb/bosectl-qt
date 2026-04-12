#include "NcWindow.h"

NcWindow::NcWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle("Noise Cancellation - bosectl");
    setFixedWidth(300);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(12);

    auto* title = new QLabel("Noise Cancellation");
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* hint = new QLabel("Requires ANC on and Wind Block off");
    hint->setAlignment(Qt::AlignCenter);
    QFont hintFont = hint->font();
    hintFont.setPointSize(hintFont.pointSize() - 1);
    hint->setFont(hintFont);
    hint->setStyleSheet("color: gray;");
    root->addWidget(hint);

    // Endpoint labels above the slider
    auto* endpointRow = new QHBoxLayout;
    auto* minLabel = new QLabel("Max NC");
    minLabel->setAlignment(Qt::AlignLeft);
    auto* maxLabel = new QLabel("Ambient");
    maxLabel->setAlignment(Qt::AlignRight);
    endpointRow->addWidget(minLabel);
    endpointRow->addStretch();
    endpointRow->addWidget(maxLabel);
    root->addLayout(endpointRow);

    // Slider + big value label
    auto* sliderRow = new QHBoxLayout;
    slider_ = new QSlider(Qt::Horizontal);
    slider_->setRange(0, 10);
    slider_->setTickPosition(QSlider::TicksBelow);
    slider_->setTickInterval(1);

    valueLabel_ = new QLabel("0");
    valueLabel_->setMinimumWidth(32);
    valueLabel_->setAlignment(Qt::AlignCenter);
    QFont valFont = valueLabel_->font();
    valFont.setBold(true);
    valFont.setPointSize(valFont.pointSize() + 2);
    valueLabel_->setFont(valFont);

    sliderRow->addWidget(slider_, 1);
    sliderRow->addWidget(valueLabel_);
    root->addLayout(sliderRow);

    statusLabel_ = new QLabel;
    statusLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(statusLabel_);

    auto* btnRow = new QHBoxLayout;
    auto* applyBtn = new QPushButton("Apply");
    applyBtn->setDefault(true);
    btnRow->addWidget(applyBtn);
    btnRow->addStretch();
    auto* closeBtn = new QPushButton("Close");
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    // Only update the visible label while dragging — never fire the network call
    connect(slider_, &QSlider::valueChanged, valueLabel_, [this](int v) {
        valueLabel_->setText(QString::number(v));
    });

    // Apply button sends the current slider value
    connect(applyBtn, &QPushButton::clicked, this, [this] {
        emit cncChanged(slider_->value());
    });

    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
}

void NcWindow::setValue(int val) {
    slider_->blockSignals(true);
    slider_->setValue(val);
    slider_->blockSignals(false);
    valueLabel_->setText(QString::number(val));
}

void NcWindow::setMaximum(int max) {
    slider_->setMaximum(max);
}

void NcWindow::setBusy(bool busy) {
    statusLabel_->setText(busy ? "Working..." : "");
}
