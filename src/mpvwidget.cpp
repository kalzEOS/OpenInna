#include "mpvwidget.h"
#include <QDebug>
#include <clocale>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <QEvent>
#include <QTimer>
#include <QTime>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QMimeData>
#include <QIcon>
#include <QDragMoveEvent>


// MPV redraw callback
static void on_mpv_redraw(void *ctx)
{
    MpvWidget *self = static_cast<MpvWidget*>(ctx);
    self->update();
}

// MPV wakeup callback: handle events (position, duration)
void on_mpv_events(void *ctx)
{
    MpvWidget *self = static_cast<MpvWidget*>(ctx);
    // mpv can invoke this from non-Qt threads; defer processing to Qt main thread
    QMetaObject::invokeMethod(self, "processMpvEvents", Qt::QueuedConnection);
}

MpvWidget::MpvWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    setlocale(LC_NUMERIC, "C");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(100, 100);
    setMouseTracking(true);
    setAcceptDrops(true);


    // Create floating controls
    controls = new ControlBar(this);
    controls->setAttribute(Qt::WA_ShowWithoutActivating);
    controls->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    controls->setMouseTracking(true);

    // Set initial position at bottom
    QTimer::singleShot(0, this, [this]() {
        repositionControls();
    });

    // Connect MPV signals to controls
    QTimer::singleShot(100, this, &MpvWidget::setupControlConnections);

    // Track window movement to keep controls positioned
    installEventFilter(this);

    // Cursor hide timer
    cursorHideTimer = new QTimer(this);
    cursorHideTimer->setInterval(2000);
    cursorHideTimer->setSingleShot(true);
    connect(cursorHideTimer, &QTimer::timeout, this, [this]() {
        if (!isControlsHovered()) {
            setCursor(Qt::BlankCursor);
        }
    });
}

MpvWidget::~MpvWidget()
{
    if (mpv_gl)
        mpv_render_context_free(mpv_gl);

    if (mpv)
        mpv_destroy(mpv);

    if (controls)
        controls->deleteLater();
}

void MpvWidget::repositionControls()
{
    if (!controls) return;

    const int margin = 40;
    const int barHeight = controls->height();

    const int available = qMax(0, width() - (2 * margin));
    const int desired = static_cast<int>(width() * 0.40);  // 40% of window width
    const int minWidth = controls->sizeHint().width();
    int barWidth = qMin(available, qMax(minWidth, desired));
    int x = (width() - barWidth) / 2; // center it

    controls->setGeometry(
        x,
        height() - barHeight - margin,
                          barWidth,
                          barHeight
    );

    controls->raise();
}

bool MpvWidget::isControlsHovered() const
{
    if (!controls) return false;

    QPoint cursorPos = mapFromGlobal(QCursor::pos());
    return controls->geometry().contains(cursorPos);
}

void MpvWidget::setupControlConnections()
{
    if (!controls || !mpv) return;

    // Previous button
    connect(controls->prevButton, &QPushButton::clicked, this, &MpvWidget::playPrev);

    // Play/Pause button
    connect(controls->playButton, &QPushButton::clicked, this, [this]() {
        if (!mpv) return;

        if (isPaused) {
            mpv_set_property_string(mpv, "pause", "no");

            // ⬇⬇ SET PAUSE ICON
            controls->playButton->setIcon(QIcon(":/icons/pause.svg"));

            isPaused = false;
        } else {
            mpv_set_property_string(mpv, "pause", "yes");

            // ⬇⬇ SET PLAY ICON
            controls->playButton->setIcon(QIcon(":/icons/play.svg"));

            isPaused = true;
        }
    });


    // Next button
    connect(controls->nextButton, &QPushButton::clicked, this, &MpvWidget::playNext);




    // Seek slider
    connect(controls->seekSlider, &QSlider::sliderPressed, this, [this]() {
        isSeekingManually = true;
    });

    connect(controls->seekSlider, &QSlider::sliderReleased, this, [this]() {
        isSeekingManually = false;
    });

    connect(controls->seekSlider, &QSlider::sliderMoved, this, [this](int value) {
        if (!mpv) return;

        double duration = 0;
        mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &duration);

        if (duration > 0) {
            double seekTo = duration * value / controls->seekSlider->maximum();
            QString cmd = QString("seek %1 absolute").arg(seekTo, 0, 'f', 3);
            mpv_command_string(mpv, cmd.toUtf8().constData());
        }
    });

    // Volume slider
    connect(controls->volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (!mpv) return;
        QString volume = QString::number(value);
        mpv_set_property_string(mpv, "volume", volume.toUtf8().constData());
    });

    // Position changed signal
    connect(this, &MpvWidget::positionChanged, this, [this](double pos) {
        if (!controls || !mpv || isSeekingManually) return;

        double duration = 0;
        mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &duration);

        if (duration > 0) {
            int sliderPos = static_cast<int>((pos / duration) * controls->seekSlider->maximum());

            controls->seekSlider->blockSignals(true);
            controls->seekSlider->setValue(sliderPos);
            controls->seekSlider->blockSignals(false);

            // Update time label
            QTime currentTime(0, 0, 0);
            QTime totalTime(0, 0, 0);
            currentTime = currentTime.addSecs(static_cast<int>(pos));
            totalTime = totalTime.addSecs(static_cast<int>(duration));

            QString timeText;
            if (duration >= 3600) {
                timeText = QString("%1 / %2")
                .arg(currentTime.toString("hh:mm:ss"))
                .arg(totalTime.toString("hh:mm:ss"));
            } else {
                timeText = QString("%1 / %2")
                .arg(currentTime.toString("mm:ss"))
                .arg(totalTime.toString("mm:ss"));
            }

            controls->timeLabel->setText(timeText);
        }
    });

    // Duration changed signal
    connect(this, &MpvWidget::durationChanged, this, [this](double duration) {
        if (!controls || !mpv) return;

        if (duration > 0) {
            controls->seekSlider->setMaximum(1000);
        }
    });
}

void MpvWidget::initializeGL()
{
    setlocale(LC_NUMERIC, "C");

    mpv = mpv_create();
    if (!mpv)
        qFatal("Could not create MPV instance");

    auto setOpt = [this](const char *name, const char *value) {
        int r = mpv_set_option_string(mpv, name, value);
        if (r < 0) {
            qWarning() << "Failed to set mpv option" << name << ":" << mpv_error_string(r);
        }
    };

    // Use libmpv VO to avoid spawning a separate window; rendered via the OpenGL callback
    setOpt("vo", "libmpv");
    // Use safe hardware decoding; avoid forcing CUDA on systems without it
    setOpt("hwdec", "auto-safe");
    // Standard player behavior: scale to window with letterboxing/pillarboxing
    setOpt("keepaspect", "yes");
    setOpt("keepaspect-window", "yes");
    setOpt("video-unscaled", "no");
    setOpt("panscan", "0");
    int initStatus = mpv_initialize(mpv);
    if (initStatus < 0) {
        qFatal("Could not initialize mpv: %s", mpv_error_string(initStatus));
    }

    mpv_opengl_init_params gl_init = {
        .get_proc_address = [](void *, const char *name) -> void * {
            return reinterpret_cast<void *>(QOpenGLContext::currentContext()->getProcAddress(name));
        },
        .get_proc_address_ctx = nullptr
    };

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    if (mpv_render_context_create(&mpv_gl, mpv, params) < 0)
        qFatal("Failed to create MPV render context");

    mpv_render_context_set_update_callback(mpv_gl, on_mpv_redraw, this);

    mpv_observe_property(mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 2, "duration", MPV_FORMAT_DOUBLE);

    mpv_set_wakeup_callback(mpv, on_mpv_events, this);

    // Set initial volume once on startup
    mpv_set_property_string(mpv, "volume", "50");
    if (controls && controls->volumeSlider) {
        controls->volumeSlider->setValue(50);
    }

    // If a file/URL was dropped before mpv initialized, start it now
    if (!pendingPlayUrl.isEmpty()) {
        play(pendingPlayUrl);
        pendingPlayUrl.clear();
    }
}

void MpvWidget::paintGL()
{
    if (!mpv_gl)
        return;

    int fb_w = static_cast<int>(devicePixelRatio() * width());
    int fb_h = static_cast<int>(devicePixelRatio() * height());

    glViewport(0, 0, fb_w, fb_h);

    mpv_opengl_fbo fbo = {
        .fbo = static_cast<int>(defaultFramebufferObject()),
        .w   = fb_w,
        .h   = fb_h,
        .internal_format = 0,
    };

    int flip_y = 1;
    int size[2] = { fb_w, fb_h };

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_SW_SIZE, size},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    mpv_render_context_render(mpv_gl, params);
}

void MpvWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w)
    Q_UNUSED(h)

    if (mpv_gl)
        mpv_render_context_set_update_callback(mpv_gl, on_mpv_redraw, this);

    update();
}

void MpvWidget::play(const QString &url)
{
    if (!mpv) {
        pendingPlayUrl = url;
        qWarning() << "MPV not initialized yet; queued" << url;
        return;
    }

    // If this is a new file, add to playlist
    if (currentIndex == -1 || playlist.isEmpty() || playlist.value(currentIndex) != url) {
        playlist << url;
        currentIndex = playlist.size() - 1;
    }

    QByteArray ba = url.toUtf8();
    const char *cmd[] = {"loadfile", ba.constData(), nullptr};
    int status = mpv_command(mpv, cmd);
    if (status < 0) {
        qWarning() << "Failed to load" << url << ":" << mpv_error_string(status);
        return;
    }

    // Always start playing (even if previous item was paused)
    mpv_set_property_string(mpv, "pause", "no");
    isPaused = false;
    // show pause icon to reflect playing state
    if (controls && controls->playButton) {
        controls->playButton->setIcon(QIcon(":/icons/pause.svg"));
    }
}

void MpvWidget::playNext()
{
    if (playlist.isEmpty()) return;

    currentIndex++;
    if (currentIndex >= playlist.size()) currentIndex = 0;

    play(playlist[currentIndex]);
}

void MpvWidget::playPrev()
{
    if (playlist.isEmpty()) return;

    currentIndex--;
    if (currentIndex < 0) currentIndex = playlist.size() - 1;

    play(playlist[currentIndex]);
}


void MpvWidget::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    repositionControls();
}

bool MpvWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == this) {
        if (event->type() == QEvent::Move ||
            event->type() == QEvent::Resize ||
            event->type() == QEvent::WindowStateChange) {
            QTimer::singleShot(10, this, &MpvWidget::repositionControls);
            }
    }

    return QOpenGLWidget::eventFilter(obj, event);
}

void MpvWidget::mouseMoveEvent(QMouseEvent *event)
{
    QOpenGLWidget::mouseMoveEvent(event);

    // Show cursor and controls
    setCursor(Qt::ArrowCursor);

    if (controls) {
        controls->fadeIn();
        controls->resetHideTimer();
    }

    // Restart cursor hide timer
    if (cursorHideTimer) {
        cursorHideTimer->stop();
        cursorHideTimer->start();
    }
}

void MpvWidget::enterEvent(QEnterEvent *event)
{
    QOpenGLWidget::enterEvent(event);
    setCursor(Qt::ArrowCursor);

    if (controls) {
        controls->fadeIn();
    }
}

void MpvWidget::leaveEvent(QEvent *event)
{
    QOpenGLWidget::leaveEvent(event);
    if (cursorHideTimer) {
        cursorHideTimer->stop();
    }
    setCursor(Qt::ArrowCursor);
}

void MpvWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QOpenGLWidget::mouseDoubleClickEvent(event);

    if (window()->windowState() & Qt::WindowFullScreen) {
        window()->showNormal();
    } else {
        window()->showFullScreen();
    }
    repositionControls();
}

void MpvWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F) {
        if (window()->windowState() & Qt::WindowFullScreen) {
            window()->showNormal();
        } else {
            window()->showFullScreen();
        }
        repositionControls();
    } else if (event->key() == Qt::Key_Escape) {
        if (window()->windowState() & Qt::WindowFullScreen) {
            window()->showNormal();
            repositionControls();
        }
    } else if (event->key() == Qt::Key_Space) {
        if (controls && controls->playButton) {
            controls->playButton->click();
        }
    }
    QOpenGLWidget::keyPressEvent(event);
}

//add drag and drop functionality
void MpvWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction(); // allow dropping files/urls
    }
}

void MpvWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MpvWidget::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;

    QString filePath = urls.first().toLocalFile();
    if (!filePath.isEmpty()) {
        if (mpv) {
            play(filePath);            // local file
        } else {
            pendingPlayUrl = filePath; // wait for mpv init
        }
        event->acceptProposedAction();
        return;
    }

    // Fallback: try remote URL directly
    const QString url = urls.first().toString();
    if (!url.isEmpty()) {
        if (mpv) {
            play(url);
        } else {
            pendingPlayUrl = url;
        }
        event->acceptProposedAction();
    }
}
void MpvWidget::processMpvEvents()
{
    if (!mpv)
        return;

    while (true) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;

        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            auto *prop = static_cast<mpv_event_property *>(event->data);

            if (strcmp(prop->name, "time-pos") == 0 && prop->format == MPV_FORMAT_DOUBLE && prop->data) {
                double pos = *static_cast<double *>(prop->data);
                emit positionChanged(pos);
            }

            if (strcmp(prop->name, "duration") == 0 && prop->format == MPV_FORMAT_DOUBLE && prop->data) {
                double dur = *static_cast<double *>(prop->data);
                emit durationChanged(dur);
            }
        }

        if (event->event_id == MPV_EVENT_END_FILE) {
            auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (!end)
                continue;

            // Only auto-advance on natural EOF, not on STOP (which fires when we manually replace a file)
            if (end->reason == MPV_END_FILE_REASON_EOF && playlist.size() > 1) {
                playNext();
            }
        }
    }
}
