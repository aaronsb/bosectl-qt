#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>

#include "BmapWorker.h"

// Persistent EQ editor window. Opened from the tray menu.
// Has Bass/Mid/Treble sliders with Try/Save/Reset/Close buttons.
class EqWindow : public QWidget {
    Q_OBJECT

public:
    explicit EqWindow(QWidget* parent = nullptr);

    void setCurrentEq(int8_t bass, int8_t mid, int8_t treble);
    void setSavedEq(int8_t bass, int8_t mid, int8_t treble);

signals:
    void eqTry(int8_t bass, int8_t mid, int8_t treble);
    void eqSave(int8_t bass, int8_t mid, int8_t treble);

private slots:
    void onTry();
    void onSave();
    void onReset();

private:
    void buildUi();

    QSlider* bassSlider_;
    QSlider* midSlider_;
    QSlider* trebleSlider_;
    QLabel* bassLabel_;
    QLabel* midLabel_;
    QLabel* trebleLabel_;

    QLabel* statusLabel_;
    int8_t savedBass_ = 0, savedMid_ = 0, savedTreble_ = 0;

public:
    void setBusy(bool busy) {
        statusLabel_->setText(busy ? "Working..." : "");
    }
};
