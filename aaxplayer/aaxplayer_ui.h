/********************************************************************************
** Form generated from reading UI file 'aaxplayer.ui'
**
** Created by: Qt User Interface Compiler version 4.8.6
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef AAXPLAYER_UI_H
#define AAXPLAYER_UI_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDialog>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QProgressBar>
#include <QtGui/QPushButton>
#include <QtGui/QSlider>
#include <QtGui/QStatusBar>

QT_BEGIN_NAMESPACE

class Ui_AudioPlayer
{
public:
    QAction *actionOpen;
    QAction *actionSave;
    QAction *actionExit;
    QAction *actionHardware;
    QMenuBar *menubar;
    QMenu *menuFile;
    QMenu *menuSetup;
    QStatusBar *statusbar;
    QPushButton *stopPlay;
    QLabel *timeCurrent;
    QLabel *timeRemaining;
    QProgressBar *pctPlaying;
    QPushButton *startPlay;
    QLabel *timeTotal;
    QPushButton *pausePlay;
    QPushButton *startRecord;
    QLabel *timeRecord;
    QProgressBar *VUleft;
    QProgressBar *VUright;
    QSlider *volumeSlider;

    void setupUi(QDialog *AudioPlayer)
    {
        if (AudioPlayer->objectName().isEmpty())
            AudioPlayer->setObjectName(QString::fromUtf8("AudioPlayer"));
        AudioPlayer->resize(400, 160);
        AudioPlayer->setMinimumSize(QSize(400, 160));
        AudioPlayer->setMaximumSize(QSize(400, 160));
        actionOpen = new QAction(AudioPlayer);
        actionOpen->setObjectName(QString::fromUtf8("actionOpen"));
        actionSave = new QAction(AudioPlayer);
        actionSave->setObjectName(QString::fromUtf8("actionSave"));
        actionExit = new QAction(AudioPlayer);
        actionExit->setObjectName(QString::fromUtf8("actionExit"));
        actionHardware = new QAction(AudioPlayer);
        actionHardware->setObjectName(QString::fromUtf8("actionHardware"));
        menubar = new QMenuBar(AudioPlayer);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 400, 24));
        menuFile = new QMenu(menubar);
        menuFile->setObjectName(QString::fromUtf8("menuFile"));
        menuSetup = new QMenu(menubar);
        menuSetup->setObjectName(QString::fromUtf8("menuSetup"));
        statusbar = new QStatusBar(AudioPlayer);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        statusbar->setGeometry(QRect(0, 0, 3, 22));
        stopPlay = new QPushButton(AudioPlayer);
        stopPlay->setObjectName(QString::fromUtf8("stopPlay"));
        stopPlay->setGeometry(QRect(125, 113, 51, 25));
        timeCurrent = new QLabel(AudioPlayer);
        timeCurrent->setObjectName(QString::fromUtf8("timeCurrent"));
        timeCurrent->setGeometry(QRect(15, 35, 71, 20));
        QFont font;
        font.setFamily(QString::fromUtf8("Liberation Sans Narrow"));
        font.setPointSize(10);
        font.setBold(true);
        font.setWeight(75);
        timeCurrent->setFont(font);
        timeCurrent->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        timeRemaining = new QLabel(AudioPlayer);
        timeRemaining->setObjectName(QString::fromUtf8("timeRemaining"));
        timeRemaining->setGeometry(QRect(185, 35, 71, 16));
        timeRemaining->setFont(font);
        timeRemaining->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        pctPlaying = new QProgressBar(AudioPlayer);
        pctPlaying->setObjectName(QString::fromUtf8("pctPlaying"));
        pctPlaying->setGeometry(QRect(15, 57, 242, 12));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(true);
        font1.setWeight(75);
        pctPlaying->setFont(font1);
        pctPlaying->setValue(24);
        startPlay = new QPushButton(AudioPlayer);
        startPlay->setObjectName(QString::fromUtf8("startPlay"));
        startPlay->setGeometry(QRect(15, 113, 51, 25));
        timeTotal = new QLabel(AudioPlayer);
        timeTotal->setObjectName(QString::fromUtf8("timeTotal"));
        timeTotal->setGeometry(QRect(185, 70, 71, 20));
        timeTotal->setFont(font);
        timeTotal->setLayoutDirection(Qt::LeftToRight);
        timeTotal->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        pausePlay = new QPushButton(AudioPlayer);
        pausePlay->setObjectName(QString::fromUtf8("pausePlay"));
        pausePlay->setGeometry(QRect(70, 113, 51, 25));
        startRecord = new QPushButton(AudioPlayer);
        startRecord->setObjectName(QString::fromUtf8("startRecord"));
        startRecord->setGeometry(QRect(330, 113, 51, 25));
        timeRecord = new QLabel(AudioPlayer);
        timeRecord->setObjectName(QString::fromUtf8("timeRecord"));
        timeRecord->setGeometry(QRect(315, 95, 61, 17));
        timeRecord->setFont(font);
        timeRecord->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        VUleft = new QProgressBar(AudioPlayer);
        VUleft->setObjectName(QString::fromUtf8("VUleft"));
        VUleft->setGeometry(QRect(310, 33, 81, 16));
        VUleft->setFont(font1);
        VUleft->setValue(24);
        VUleft->setOrientation(Qt::Horizontal);
        VUright = new QProgressBar(AudioPlayer);
        VUright->setObjectName(QString::fromUtf8("VUright"));
        VUright->setGeometry(QRect(310, 53, 81, 16));
        VUright->setFont(font1);
        VUright->setValue(24);
        VUright->setOrientation(Qt::Horizontal);
        volumeSlider = new QSlider(AudioPlayer);
        volumeSlider->setObjectName(QString::fromUtf8("volumeSlider"));
        volumeSlider->setGeometry(QRect(270, 30, 29, 105));
        volumeSlider->setOrientation(Qt::Vertical);
        QWidget::setTabOrder(startPlay, pausePlay);
        QWidget::setTabOrder(pausePlay, stopPlay);
        QWidget::setTabOrder(stopPlay, volumeSlider);
        QWidget::setTabOrder(volumeSlider, startRecord);

        menubar->addAction(menuFile->menuAction());
        menubar->addAction(menuSetup->menuAction());
        menuFile->addAction(actionOpen);
        menuFile->addAction(actionSave);
        menuFile->addAction(actionExit);
        menuSetup->addAction(actionHardware);

        retranslateUi(AudioPlayer);

        QMetaObject::connectSlotsByName(AudioPlayer);
    } // setupUi

    void retranslateUi(QDialog *AudioPlayer)
    {
        AudioPlayer->setWindowTitle(QApplication::translate("AudioPlayer", "AeonWave Audio Player", 0, QApplication::UnicodeUTF8));
        actionOpen->setText(QApplication::translate("AudioPlayer", "Open", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        actionOpen->setToolTip(QApplication::translate("AudioPlayer", "Open Input File", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        actionOpen->setShortcut(QApplication::translate("AudioPlayer", "Ctrl+O", 0, QApplication::UnicodeUTF8));
        actionSave->setText(QApplication::translate("AudioPlayer", "Save To", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        actionSave->setToolTip(QApplication::translate("AudioPlayer", "Select Save To file", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        actionSave->setShortcut(QApplication::translate("AudioPlayer", "Ctrl+S", 0, QApplication::UnicodeUTF8));
        actionExit->setText(QApplication::translate("AudioPlayer", "Exit", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        actionExit->setToolTip(QApplication::translate("AudioPlayer", "Close the application", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        actionExit->setShortcut(QApplication::translate("AudioPlayer", "Ctrl+X", 0, QApplication::UnicodeUTF8));
        actionHardware->setText(QApplication::translate("AudioPlayer", "Configuration", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        actionHardware->setToolTip(QApplication::translate("AudioPlayer", "Hardware Configuration", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        actionHardware->setShortcut(QApplication::translate("AudioPlayer", "Ctrl+H", 0, QApplication::UnicodeUTF8));
        menuFile->setTitle(QApplication::translate("AudioPlayer", "File", 0, QApplication::UnicodeUTF8));
        menuSetup->setTitle(QApplication::translate("AudioPlayer", "Setup", 0, QApplication::UnicodeUTF8));
        stopPlay->setText(QApplication::translate("AudioPlayer", "STOP", 0, QApplication::UnicodeUTF8));
        timeCurrent->setText(QApplication::translate("AudioPlayer", "00:00:00", 0, QApplication::UnicodeUTF8));
        timeRemaining->setText(QApplication::translate("AudioPlayer", "00:00:00", 0, QApplication::UnicodeUTF8));
        startPlay->setText(QApplication::translate("AudioPlayer", "PLAY", 0, QApplication::UnicodeUTF8));
        timeTotal->setText(QApplication::translate("AudioPlayer", "00:00:00", 0, QApplication::UnicodeUTF8));
        pausePlay->setText(QApplication::translate("AudioPlayer", "PAUSE", 0, QApplication::UnicodeUTF8));
        startRecord->setText(QApplication::translate("AudioPlayer", "REC", 0, QApplication::UnicodeUTF8));
        timeRecord->setText(QApplication::translate("AudioPlayer", "00:00:00", 0, QApplication::UnicodeUTF8));
        VUleft->setFormat(QString());
        VUright->setFormat(QString());
    } // retranslateUi

};

namespace Ui {
    class AudioPlayer: public Ui_AudioPlayer {};
} // namespace Ui

QT_END_NAMESPACE

#endif // AAXPLAYER_UI_H
