#pragma once

#include <QOpenGLWidget>
#include <QString>
#include <QTimer>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include "controlbar.h"


class MpvWidget : public QOpenGLWidget
{
    Q_OBJECT
    QSize sizeHint() const override { return QSize(800, 600); }
    bool isPaused = false;


public:
    explicit MpvWidget(QWidget *parent = nullptr);
    ~MpvWidget();

    void play(const QString &url);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

public slots:
    void playNext();
    void playPrev();
    void processMpvEvents();


private slots:
    void setupControlConnections();

signals:
    void durationChanged(double seconds);
    void positionChanged(double seconds);

private:
    void repositionControls();
    bool isControlsHovered() const;
    QStringList playlist;
    int currentIndex = -1;
    QString pendingPlayUrl;


    mpv_handle *mpv = nullptr;
    mpv_render_context *mpv_gl = nullptr;
    ControlBar *controls;
    QTimer *cursorHideTimer;
    bool isSeekingManually = false;

    friend void on_mpv_events(void *ctx);
};
