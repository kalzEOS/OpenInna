#include <QApplication>
#include <QMainWindow>
#include <QFileDialog>
#include <QFile>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QInputDialog>
#include <QDir>
#include "mpvwidget.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Q_INIT_RESOURCE(resources);

    QMainWindow mainWindow;
    mainWindow.setWindowTitle("MPV Player - INNA Style");
    mainWindow.resize(1280, 720);

    MpvWidget *mpvWidget = new MpvWidget(&mainWindow);
    mainWindow.setCentralWidget(mpvWidget);

    mainWindow.show();
    mainWindow.raise();
    mainWindow.activateWindow();

    mainWindow.setStyleSheet(R"(
        QMainWindow {
            background-color: #000;
        }
        QMenuBar {
            background-color: #1a1a1a;
            color: white;
        }
        QMenuBar::item:selected {
            background-color: #333;
        }
        QMenu {
            background-color: #1a1a1a;
            color: white;
            border: 1px solid #333;
        }
        QMenu::item:selected {
            background-color: #333;
        }
    )");

    // Create menu bar
    QMenuBar *menuBar = new QMenuBar(&mainWindow);
    mainWindow.setMenuBar(menuBar);

    QMenu *fileMenu = menuBar->addMenu("File");

    // Open file action
    QAction *openAction = new QAction("Open File...", &mainWindow);
    openAction->setShortcut(QKeySequence::Open); // Ctrl+O / Cmd+O
    fileMenu->addAction(openAction);

    QObject::connect(openAction, &QAction::triggered, [&]() {
        QString fileName = QFileDialog::getOpenFileName(
            &mainWindow,
            "Open Video File",
            QDir::homePath(),
            "Video Files (*.mp4 *.mkv *.avi *.mov *.webm *.flv *.wmv *.m4v *.mpg *.mpeg *.ts);;All Files (*)"
        );

        if (!fileName.isEmpty()) {
            mpvWidget->play(fileName);
        }
    });

    // Open URL action
    QAction *openUrlAction = new QAction("Open URL...", &mainWindow);
    openUrlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
    fileMenu->addAction(openUrlAction);

    QObject::connect(openUrlAction, &QAction::triggered, [&]() {
        bool ok;
        QString url = QInputDialog::getText(
            &mainWindow,
            "Open URL",
            "Enter video URL:",
            QLineEdit::Normal,
            "",
            &ok
        );

        if (ok && !url.isEmpty()) {
            mpvWidget->play(url);
        }
    });

    fileMenu->addSeparator();

    // Quit action
    QAction *quitAction = new QAction("Quit", &mainWindow);
    quitAction->setShortcut(QKeySequence::Quit);
    fileMenu->addAction(quitAction);
    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

    mainWindow.show();

    // Only auto-play when a file/URL is provided via CLI; otherwise start idle/black
    if (argc > 1) {
        QString videoPath = QString::fromUtf8(argv[1]);
        mpvWidget->play(videoPath);
    }

    return app.exec();
}
