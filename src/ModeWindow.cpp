#include "ModeWindow.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFont>

#include "Logging.h"

ModeWindow::ModeWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle("Modes - bosectl");
    buildUi();
    setMinimumSize(420, 380);
}

void ModeWindow::setModes(const QList<ModeInfo>& modes, uint8_t activeIdx) {
    updatingUi_ = true;
    modes_ = modes;

    int previousRow = modeList_->currentRow();
    modeList_->clear();

    for (const auto& m : modes_) {
        QString label = m.name;
        if (m.active) label += "  \u25c0";  // ◀ active indicator
        if (!m.editable) label += "  [built-in]";
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, m.idx);
        if (m.active) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
        modeList_->addItem(item);
    }

    // Restore selection
    if (previousRow >= 0 && previousRow < modeList_->count())
        modeList_->setCurrentRow(previousRow);
    else if (modeList_->count() > 0)
        modeList_->setCurrentRow(0);

    updatingUi_ = false;

    if (modeList_->currentRow() >= 0)
        onModeSelected(modeList_->currentRow());
}

void ModeWindow::onModeSelected(int row) {
    if (updatingUi_ || row < 0 || row >= modes_.size()) return;
    selectedRow_ = row;
    const auto& mode = modes_[row];
    updateEditor(mode);
    setEditorEnabled(mode.editable);
    deleteBtn_->setEnabled(mode.editable);
    activateBtn_->setEnabled(!mode.active);
}

void ModeWindow::onActivateClicked() {
    if (selectedRow_ < 0 || selectedRow_ >= modes_.size()) return;
    qCInfo(lcUi) << "Mode: activate clicked;" << modes_[selectedRow_].name;
    emit activateMode(modes_[selectedRow_].name);
}

void ModeWindow::onSaveClicked() {
    if (selectedRow_ < 0 || selectedRow_ >= modes_.size()) return;
    const auto& mode = modes_[selectedRow_];
    if (!mode.editable) return;

    QString name = nameEdit_->text().trimmed();
    if (name.isEmpty()) {
        qCWarning(lcUi) << "Mode: save rejected; empty name";
        QMessageBox::warning(this, "bosectl", "Mode name cannot be empty.");
        return;
    }

    uint8_t spatial = spatialCombo_->currentIndex();
    qCInfo(lcUi) << "Mode: save clicked; idx=" << mode.idx << "name=" << name
                 << "cnc=" << cncSlider_->value() << "spatial=" << spatial
                 << "wind=" << windBlockCheck_->isChecked()
                 << "anc=" << ancToggleCheck_->isChecked();
    emit saveMode(mode.idx, name,
                  static_cast<uint8_t>(cncSlider_->value()),
                  spatial,
                  windBlockCheck_->isChecked(),
                  ancToggleCheck_->isChecked());
}

void ModeWindow::onDeleteClicked() {
    if (selectedRow_ < 0 || selectedRow_ >= modes_.size()) return;
    const auto& mode = modes_[selectedRow_];
    if (!mode.editable) return;

    auto reply = QMessageBox::question(this, "bosectl",
        QString("Delete mode \"%1\"?").arg(mode.name),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        qCInfo(lcUi) << "Mode: delete confirmed;" << mode.name;
        emit deleteMode(mode.name);
    } else {
        qCInfo(lcUi) << "Mode: delete cancelled;" << mode.name;
    }
}

void ModeWindow::onNewClicked() {
    bool ok;
    QString name = QInputDialog::getText(this, "New Mode", "Mode name:",
                                          QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) {
        qCInfo(lcUi) << "Mode: new cancelled or empty name";
        return;
    }

    uint8_t spatial = spatialCombo_->currentIndex();
    qCInfo(lcUi) << "Mode: new clicked; name=" << name.trimmed()
                 << "cnc=" << cncSlider_->value() << "spatial=" << spatial
                 << "wind=" << windBlockCheck_->isChecked()
                 << "anc=" << ancToggleCheck_->isChecked();
    emit createMode(name.trimmed(),
                    static_cast<uint8_t>(cncSlider_->value()),
                    spatial,
                    windBlockCheck_->isChecked(),
                    ancToggleCheck_->isChecked());
}

void ModeWindow::updateEditor(const ModeInfo& mode) {
    updatingUi_ = true;

    nameEdit_->setText(mode.name);

    cncSlider_->setValue(mode.cncLevel);
    cncValueLabel_->setText(QString::number(mode.cncLevel));

    spatialCombo_->setCurrentIndex(mode.spatial);

    windBlockCheck_->setChecked(mode.windBlock);
    ancToggleCheck_->setChecked(mode.ancToggle);

    updatingUi_ = false;
}

void ModeWindow::setEditorEnabled(bool editable) {
    nameEdit_->setReadOnly(!editable);
    cncSlider_->setEnabled(editable);
    spatialCombo_->setEnabled(editable);
    windBlockCheck_->setEnabled(editable);
    ancToggleCheck_->setEnabled(editable);
    saveBtn_->setEnabled(editable);
}

void ModeWindow::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    // ── Left: Mode list ─────────────────────────────────────────────────────
    auto* leftCol = new QVBoxLayout;

    auto* listLabel = new QLabel("Modes");
    QFont boldFont = listLabel->font();
    boldFont.setBold(true);
    listLabel->setFont(boldFont);
    leftCol->addWidget(listLabel);

    modeList_ = new QListWidget;
    modeList_->setMinimumWidth(160);
    leftCol->addWidget(modeList_, 1);

    auto* listButtons = new QHBoxLayout;
    newBtn_ = new QPushButton("New");
    deleteBtn_ = new QPushButton("Delete");
    deleteBtn_->setEnabled(false);
    listButtons->addWidget(newBtn_);
    listButtons->addWidget(deleteBtn_);
    listButtons->addStretch();
    leftCol->addLayout(listButtons);

    root->addLayout(leftCol);

    // ── Right: Editor ───────────────────────────────────────────────────────
    auto* rightCol = new QVBoxLayout;

    auto* editorGroup = new QGroupBox("Mode Settings");
    auto* editorLayout = new QVBoxLayout(editorGroup);
    editorLayout->setSpacing(8);

    // Name
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel("Name:"));
    nameEdit_ = new QLineEdit;
    nameRow->addWidget(nameEdit_);
    editorLayout->addLayout(nameRow);

    // CNC
    auto* cncRow = new QHBoxLayout;
    cncRow->addWidget(new QLabel("Noise Cancel:"));
    cncSlider_ = new QSlider(Qt::Horizontal);
    cncSlider_->setRange(0, 10);
    cncSlider_->setTickPosition(QSlider::TicksBelow);
    cncSlider_->setTickInterval(1);
    cncValueLabel_ = new QLabel("0");
    cncValueLabel_->setMinimumWidth(20);
    cncRow->addWidget(cncSlider_, 1);
    cncRow->addWidget(cncValueLabel_);
    editorLayout->addLayout(cncRow);

    connect(cncSlider_, &QSlider::valueChanged, cncValueLabel_, [this](int v) {
        cncValueLabel_->setText(QString::number(v));
    });

    // Spatial
    auto* spatialRow = new QHBoxLayout;
    spatialRow->addWidget(new QLabel("Spatial:"));
    spatialCombo_ = new QComboBox;
    spatialCombo_->addItem("Off");
    spatialCombo_->addItem("Room");
    spatialCombo_->addItem("Head Tracking");
    spatialRow->addWidget(spatialCombo_);
    editorLayout->addLayout(spatialRow);

    // Toggles
    windBlockCheck_ = new QCheckBox("Wind Block");
    ancToggleCheck_ = new QCheckBox("ANC Toggle");
    editorLayout->addWidget(windBlockCheck_);
    editorLayout->addWidget(ancToggleCheck_);

    rightCol->addWidget(editorGroup);

    statusLabel_ = new QLabel;
    statusLabel_->setAlignment(Qt::AlignCenter);
    rightCol->addWidget(statusLabel_);

    // Action buttons
    auto* actionRow = new QHBoxLayout;
    activateBtn_ = new QPushButton("Activate");
    saveBtn_ = new QPushButton("Save");
    saveBtn_->setEnabled(false);
    auto* closeBtn = new QPushButton("Close");
    actionRow->addWidget(activateBtn_);
    actionRow->addWidget(saveBtn_);
    actionRow->addStretch();
    actionRow->addWidget(closeBtn);
    rightCol->addLayout(actionRow);

    root->addLayout(rightCol, 1);

    // Connections
    connect(modeList_, &QListWidget::currentRowChanged, this, &ModeWindow::onModeSelected);
    connect(activateBtn_, &QPushButton::clicked, this, &ModeWindow::onActivateClicked);
    connect(saveBtn_, &QPushButton::clicked, this, &ModeWindow::onSaveClicked);
    connect(deleteBtn_, &QPushButton::clicked, this, &ModeWindow::onDeleteClicked);
    connect(newBtn_, &QPushButton::clicked, this, &ModeWindow::onNewClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
}
