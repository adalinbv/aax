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
#include <QtGui/QScrollBar>
#include <QtGui/QStatusBar>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Player
{
public:
    QWidget *centralwidget;
    QPushButton *buttonPlay;
    QPushButton *buttonPause;
    QPushButton *buttonStop;
    QLabel *labelTotal;
    QLabel *labelRemaining;
    QLabel *timeRemaining;
    QLabel *timeTotal;
    QScrollBar *VUright;
    QScrollBar *VUleft;
    QLabel *timeCurrent;
    QProgressBar *pctPlaying;
    QMenuBar *menubar;
    QMenu *menuFil;
    QStatusBar *statusbar;

    void setupUi(QDialog *AudioPlayer)
    {
        if (AudioPlayer->objectName().isEmpty())
            AudioPlayer->setObjectName(QString::fromUtf8("AudioPlayer"));
        AudioPlayer->resize(380, 160);
        AudioPlayer->setMinimumSize(QSize(380, 160));
        AudioPlayer->setMaximumSize(QSize(380, 160));
        centralwidget = new QWidget(AudioPlayer);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        buttonPlay = new QPushButton(centralwidget);
        buttonPlay->setObjectName(QString::fromUtf8("buttonPlay"));
        buttonPlay->setGeometry(QRect(25, 80, 51, 25));
        buttonPause = new QPushButton(centralwidget);
        buttonPause->setObjectName(QString::fromUtf8("buttonPause"));
        buttonPause->setGeometry(QRect(80, 80, 51, 25));
        buttonStop = new QPushButton(centralwidget);
        buttonStop->setObjectName(QString::fromUtf8("buttonStop"));
        buttonStop->setGeometry(QRect(135, 80, 51, 25));
        labelTotal = new QLabel(centralwidget);
        labelTotal->setObjectName(QString::fromUtf8("labelTotal"));
        labelTotal->setGeometry(QRect(30, 30, 81, 20));
        labelRemaining = new QLabel(centralwidget);
        labelRemaining->setObjectName(QString::fromUtf8("labelRemaining"));
        labelRemaining->setGeometry(QRect(30, 10, 81, 17));
        timeRemaining = new QLabel(centralwidget);
        timeRemaining->setObjectName(QString::fromUtf8("timeRemaining"));
        timeRemaining->setGeometry(QRect(120, 10, 66, 17));
        timeTotal = new QLabel(centralwidget);
        timeTotal->setObjectName(QString::fromUtf8("timeTotal"));
        timeTotal->setGeometry(QRect(120, 30, 66, 17));
        VUright = new QScrollBar(centralwidget);
        VUright->setObjectName(QString::fromUtf8("VUright"));
        VUright->setGeometry(QRect(245, 30, 111, 16));
        VUright->setOrientation(Qt::Horizontal);
        VUleft = new QScrollBar(centralwidget);
        VUleft->setObjectName(QString::fromUtf8("VUleft"));
        VUleft->setGeometry(QRect(245, 10, 111, 16));
        VUleft->setOrientation(Qt::Horizontal);
        timeCurrent = new QLabel(centralwidget);
        timeCurrent->setObjectName(QString::fromUtf8("timeCurrent"));
        timeCurrent->setGeometry(QRect(290, 80, 66, 17));
        timeCurrent->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        pctPlaying = new QProgressBar(centralwidget);
        pctPlaying->setObjectName(QString::fromUtf8("pctPlaying"));
        pctPlaying->setGeometry(QRect(30, 60, 325, 12));
        pctPlaying->setValue(24);
        menubar = new QMenuBar(AudioPlayer);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 380, 25));
        menuFil = new QMenu(menubar);
        menuFil->setObjectName(QString::fromUtf8("menuFil"));
        statusbar = new QStatusBar(AudioPlayer);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));

        menubar->addAction(menuFil->menuAction());

        retranslateUi(AudioPlayer);

        QMetaObject::connectSlotsByName(AudioPlayer);
    } // setupUi

    void retranslateUi(QDialog *AudioPlayer)
    {
        AudioPlayer->setWindowTitle(QApplication::translate("Player", "AeonWave Audio Player", 0, QApplication::UnicodeUTF8));
        buttonPlay->setText(QApplication::translate("Player", ">", 0, QApplication::UnicodeUTF8));
        buttonPause->setText(QApplication::translate("Player", "||", 0, QApplication::UnicodeUTF8));
        buttonStop->setText(QApplication::translate("Player", "[  ]", 0, QApplication::UnicodeUTF8));
        labelTotal->setText(QApplication::translate("Player", "Total:", 0, QApplication::UnicodeUTF8));
        labelRemaining->setText(QApplication::translate("Player", "Remaining:", 0, QApplication::UnicodeUTF8));
        timeRemaining->setText(QApplication::translate("Player", "00:00:00", 0, QApplication::UnicodeUTF8));
        timeTotal->setText(QApplication::translate("Player", "00:00:00", 0, QApplication::UnicodeUTF8));
        timeCurrent->setText(QApplication::translate("Player", "00:00:00", 0, QApplication::UnicodeUTF8));
        menuFil->setTitle(QApplication::translate("Player", "File", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class Player: public Ui_Player {};
} // namespace Ui

QT_END_NAMESPACE

#endif // AAXPLAYER_UI_H
