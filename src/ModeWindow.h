#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "BmapWorker.h"

// Mode manager window. Shows all modes in a list, lets the user
// select/activate modes, edit custom modes, create new ones, and delete.
class ModeWindow : public QWidget {
    Q_OBJECT

public:
    explicit ModeWindow(QWidget* parent = nullptr);

    void setModes(const QList<ModeInfo>& modes, uint8_t activeIdx);

signals:
    void activateMode(const QString& name);
    void saveMode(uint8_t idx, const QString& name, uint8_t cnc,
                  uint8_t spatial, bool windBlock, bool ancToggle);
    void deleteMode(const QString& name);
    void createMode(const QString& name, uint8_t cnc,
                    uint8_t spatial, bool windBlock, bool ancToggle);
    void refreshRequested();

private slots:
    void onModeSelected(int row);
    void onActivateClicked();
    void onSaveClicked();
    void onDeleteClicked();
    void onNewClicked();

private:
    void buildUi();
    void updateEditor(const ModeInfo& mode);
    void setEditorEnabled(bool editable);

    QListWidget* modeList_;

    // Editor panel
    QLineEdit* nameEdit_;
    QSlider* cncSlider_;
    QLabel* cncValueLabel_;
    QComboBox* spatialCombo_;
    QCheckBox* windBlockCheck_;
    QCheckBox* ancToggleCheck_;

    // Buttons
    QPushButton* activateBtn_;
    QPushButton* saveBtn_;
    QPushButton* deleteBtn_;
    QPushButton* newBtn_;

    QLabel* statusLabel_;
    QList<ModeInfo> modes_;
    int selectedRow_ = -1;
    bool updatingUi_ = false;

public:
    void setBusy(bool busy) {
        statusLabel_->setText(busy ? "Working..." : "");
    }
};
