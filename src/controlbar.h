#pragma once
#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QGraphicsOpacityEffect>

class ControlBar : public QWidget
{
    Q_OBJECT

public:
    explicit ControlBar(QWidget *parent = nullptr);

    void fadeIn();
    void fadeOut();
    void resetHideTimer();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer *hideTimer;
    QGraphicsOpacityEffect *opacityEffect;
    qreal targetOpacity = 1.0;
    bool mouseInside = false;

public:
    QPushButton *playButton;
    QPushButton *nextButton;
    QPushButton *prevButton;
    QSlider *seekSlider;
    QLabel *timeLabel;
    QSlider *volumeSlider;
};
