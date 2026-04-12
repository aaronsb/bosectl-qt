#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

class NcWindow : public QWidget {
    Q_OBJECT

public:
    explicit NcWindow(QWidget* parent = nullptr);

    void setValue(int val);
    void setMaximum(int max);
    void setBusy(bool busy);

signals:
    void cncChanged(int value);

private:
    QSlider* slider_;
    QLabel* valueLabel_;
    QLabel* statusLabel_;
};
