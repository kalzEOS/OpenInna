#include "controlbar.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QGraphicsBlurEffect>

ControlBar::ControlBar(QWidget *parent)
: QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    // SIZE + STYLE
    setFixedHeight(120);
    setMinimumWidth(100);

    // Create opacity effect for smooth fading
    opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(1.0);
    setGraphicsEffect(opacityEffect);

    // --- BUTTON SETUP ---
/*
    prevButton = new QPushButton("◀◀");
    playButton = new QPushButton("▶");
    nextButton = new QPushButton("▶▶");
    prevButton->setProperty("class", "media");
    playButton->setProperty("class", "media");
    nextButton->setProperty("class", "media");
*/
prevButton = new QPushButton();
playButton = new QPushButton();
nextButton = new QPushButton();

// assign clean built-in icons from your system theme
prevButton->setIcon(QIcon(":/icons/prev.svg"));
playButton->setIcon(QIcon(":/icons/play.svg"));
nextButton->setIcon(QIcon(":/icons/next.svg"));

// set icon size (this finally works)
prevButton->setIconSize(QSize(18, 18));
playButton->setIconSize(QSize(18, 18));
nextButton->setIconSize(QSize(18, 18));


    // FORCE override ALL system and inherited styles
QString btnStyle = R"(
QPushButton {
    background-color: transparent;
    border: none;
    color: rgba(255,255,255,180);
    font-size: 18px;
    padding: 0;
    margin: 0;
    min-width: 28px;
    min-height: 28px;
    max-width: 28px;
    max-height: 28px;
}
QPushButton:hover {
    color: rgba(255,255,255,230);
}
QPushButton:pressed {
    color: rgba(255,255,255,130);
}
)";


this->setStyleSheet(btnStyle);   // <— THE FIX

for (QPushButton *btn : {prevButton, playButton, nextButton}) {
    btn->setFixedSize(28, 28);
    btn->setFlat(true);
}


    // SEEK SLIDER - styled like INNA
    seekSlider = new QSlider(Qt::Horizontal);
    seekSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "   height: 4px;"
        "   background: rgba(255,255,255,30);"
        "   border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: rgba(255,255,255,80);"
        "   border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: white;"
        "   width: 14px;"
        "   height: 14px;"
        "   margin: -5px 0;"
        "   border-radius: 7px;"
        "   border: 2px solid rgba(0,0,0,30);"
        "}"
        "QSlider::handle:horizontal:hover {"
        "   background: #ffffff;"
        "   width: 16px;"
        "   height: 16px;"
        "   margin: -6px 0;"
        "   border-radius: 8px;"
        "}"
    );

    // TIME LABEL
    timeLabel = new QLabel("00:00 / 00:00");
    timeLabel->setStyleSheet(
        "color: white;"
        "font-weight: 500;"
        "font-size: 13px;"
        "padding: 0 8px;"
    );
    timeLabel->setMinimumWidth(120);

    // VOLUME SLIDER
    volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setFixedWidth(100);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(50);
    volumeSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "   height: 4px;"
        "   background: rgba(255,255,255,30);"
        "   border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: rgba(255,255,255,80);"
        "   border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: white;"
        "   width: 12px;"
        "   height: 12px;"
        "   margin: -4px 0;"
        "   border-radius: 6px;"
        "}"
    );

    // LAYOUT
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 6, 16, 6);
    layout->setSpacing(14);
    layout->setAlignment(Qt::AlignVCenter); // Vertically center all widgets

    layout->addWidget(prevButton);
    layout->addWidget(playButton);
    layout->addWidget(nextButton);
    layout->addWidget(seekSlider, 1);
    layout->addWidget(timeLabel);
    layout->addWidget(volumeSlider);

    // Hide timer - auto-hide after 2 seconds of no mouse
    hideTimer = new QTimer(this);
    hideTimer->setInterval(2000);
    hideTimer->setSingleShot(true);

    connect(hideTimer, &QTimer::timeout, this, [this]() {
        if (!mouseInside) {
            fadeOut();
        }
    });

    // Start visible
    show();
    resetHideTimer();
}

void ControlBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw rounded rectangle with blur-like appearance
    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);

    // Semi-transparent dark background with slight gradient
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(30, 30, 30, 200));
    gradient.setColorAt(1.0, QColor(20, 20, 20, 220));

    painter.fillPath(path, gradient);

    // Add subtle inner glow for depth
    painter.setPen(QPen(QColor(255, 255, 255, 20), 1));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 11, 11);
}

void ControlBar::fadeIn()
{
    if (targetOpacity >= 1.0 && opacityEffect->opacity() >= 0.99) {
        return; // Already visible
    }

    targetOpacity = 1.0;

    QPropertyAnimation *anim = new QPropertyAnimation(opacityEffect, "opacity");
    anim->setDuration(200);
    anim->setStartValue(opacityEffect->opacity());
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    show();
    resetHideTimer();
}

void ControlBar::fadeOut()
{
    if (mouseInside) {
        return; // Don't hide if mouse is over controls
    }

    targetOpacity = 0.0;

    QPropertyAnimation *anim = new QPropertyAnimation(opacityEffect, "opacity");
    anim->setDuration(300);
    anim->setStartValue(opacityEffect->opacity());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    // Hide completely after animation
    QTimer::singleShot(300, this, [this]() {
        if (targetOpacity <= 0.0) {
            hide();
        }
    });
}

void ControlBar::resetHideTimer()
{
    hideTimer->stop();
    hideTimer->start();
}

void ControlBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void ControlBar::enterEvent(QEnterEvent *event)
{
    mouseInside = true;
    hideTimer->stop(); // Don't hide while mouse is over controls
    fadeIn();
    QWidget::enterEvent(event);
}

void ControlBar::leaveEvent(QEvent *event)
{
    mouseInside = false;
    resetHideTimer(); // Start hide timer when mouse leaves
    QWidget::leaveEvent(event);
}
