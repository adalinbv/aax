/*
 * Copyright (C) 2014 by Adalin B.V.
 *
 * This file is part of AeonWave-AudioPlayer.
 *
 *  AeonWave-Config is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AeonWave-Config is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with AeonWave-Config.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <assert.h>

#include <QMessageBox>
#include <QFileDialog>

#include <base/types.h>
#include "aaxplayer_ui.h"
#include "aaxplayer.h"

#undef NDEBUG

#ifndef NDEBUG
# define _ATB(a)                \
do {                            \
   int r=(a);                   \
   if (r!=AAX_TRUE) printf("Error at line %i: %s\n",__LINE__,aaxGetErrorString(aaxGetErrorNo())); \
   assert(r==AAX_TRUE); \
} while(0);
#else
# define _ATB(a)                a
#endif

AeonWavePlayer::AeonWavePlayer(QWidget *parent) :
    QDialog(parent),
    outdev(NULL),
    indev(NULL),
    agc_enabled(true),
    playing(false),
    file(NULL),
    bitrate(-1),
    recording(false),
    infiles_path(""),
    outfiles_path(""),
    max_samples(0)
{
    ui = new Ui_AudioPlayer;
    ui->setupUi(this);

    QFile QSSFile(":/aaxplayer.qss");
    QSSFile.open(QFile::ReadOnly);
    QString StyleSheet = QLatin1String(QSSFile.readAll());
    qApp->setStyleSheet(StyleSheet);

    QPixmap PlayPixMap(":/play.png");
    QIcon PlayIcon(PlayPixMap);
    ui->startPlay->setIcon(PlayIcon);
    ui->startPlay->setIconSize(PlayPixMap.rect().size());

    QPixmap PausePixMap(":/pause.png");
    QIcon PauseIcon(PausePixMap);
    ui->pausePlay->setIcon(PauseIcon);
    ui->pausePlay->setIconSize(PausePixMap.rect().size());

    QPixmap StopPixMap(":/stop.png");
    QIcon StopIcon(StopPixMap);
    ui->stopPlay->setIcon(StopIcon);
    ui->stopPlay->setIconSize(StopPixMap.rect().size());

    outdev = aaxDriverOpenDefault(AAX_MODE_WRITE_STEREO);
    if (outdev)
    {
        int res = aaxMixerSetState(outdev, AAX_INITIALIZED);
        if (!res)
        {
            alert(tr("<br>Unable to initialize the output device:</br>"
                     "<p><i><b>%1</b></i></p>"
                  ).arg(aaxGetErrorString(aaxGetErrorNo())));
            _ATB(aaxDriverClose(outdev));
            _ATB(aaxDriverDestroy(outdev));
        }
        aaxMixerSetState(outdev, AAX_PLAYING);
    }

    QObject::connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(exit()));
    QObject::connect(ui->startPlay, SIGNAL(released()), this,  SLOT(togglePlay()));
    QObject::connect(ui->pausePlay, SIGNAL(released()), this,  SLOT(togglePause()));
    QObject::connect(ui->stopPlay, SIGNAL(released()), this,  SLOT(toggleStop()));
    QObject::connect(ui->volume, SIGNAL(valueChanged(int)), this, SLOT(volumeChanged(int)));
    QObject::connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(loadFile()));
//  QObject::connect(ui->startRecord, SIGNAL(released()), this,  SLOT(toggleRecord()));
//  QObject::connect(ui->actionSave, SIGNAL(triggered()), this, SLOT(saveTo()));

    QObject::connect(&timer, SIGNAL(timeout()), SLOT(tick()));
    timer.setSingleShot(false);
    timer.start(100);

    ui->pctPlaying->setValue(0);

    aaxFilter flt = aaxMixerGetFilter(outdev, AAX_VOLUME_FILTER);
    float vol = _MIN(aaxFilterGetParam(flt, AAX_GAIN, AAX_LINEAR), 1.0f);
    ui->volume->setValue(rintf(vol*100));
    aaxFilterDestroy(flt);
}

AeonWavePlayer::~AeonWavePlayer()
{
    if (recording)
    {
        recording = false;
        aaxMixerSetState(file, AAX_STOPPED);
        aaxMixerDeregisterSensor(file, outdev);
        aaxDriverClose(file);
        aaxDriverDestroy(file);
    }

    aaxMixerSetState(indev, AAX_STOPPED);
    aaxMixerDeregisterSensor(indev, outdev);
    aaxDriverClose(indev);
    aaxDriverDestroy(indev);

    aaxMixerSetState(outdev, AAX_STOPPED);
    aaxDriverClose(outdev);
    aaxDriverDestroy(outdev);
    outdev = NULL;

    delete ui;
}

/* ------------------------------------------------------------------------- */

void
AeonWavePlayer::tick()
{
   if (indev && playing) {
      playing = (aaxMixerGetState(indev) == AAX_PLAYING) ? true : false;
   }

   if (indev && playing)
   {
       float freq = (float)aaxMixerGetSetup(indev, AAX_FREQUENCY);
       float hour, minutes, seconds, pos;

       pos = (float)aaxSensorGetOffset(indev, AAX_SAMPLES);

       seconds = floorf(pos/freq);
       hour = floorf(seconds/(60.0f*60.0f));
       seconds -= hour*60.0f*60.0f;
       minutes = floorf(seconds/60.0f);
       seconds -= minutes*60.0f;

       QString current = QString("%1:%2:%3").arg(hour,2,'f',0,'0').arg(minutes,2,'f',0,'0').arg(seconds,2,'f',0,'0');
       ui->timeCurrent->setText(current);

       seconds = _MAX(ceilf((max_samples-pos)/freq), 0);
       hour = floorf(seconds/(60.0f*60.0f));
       seconds -= hour*60.0f*60.0f;
       minutes = floorf(seconds/60.0f);
       seconds -= minutes*60.0f;

       QString remain = QString("%1:%2:%3").arg(hour,2,'f',0,'0').arg(minutes,2,'f',0,'0').arg(seconds,2,'f',0,'0');
       ui->timeRemaining->setText(remain);

       ui->pctPlaying->setValue(100*pos/max_samples);

       static const double MAX = 8388608;
       static const double MAXDIV = 1.0/MAX;
       static const double REFERENCE = 256;
       static const double MIN_DB = 10*log10(1.0/REFERENCE);
       static const double MAX_DB = 0;
       float dB, vu[2];
       for (int track=0; track<2; track++)
       {
           enum aaxSetupType e1 = aaxSetupType(AAX_AVERAGE_VALUE+track);
           int ival;

           ival = aaxMixerGetSetup(outdev, e1);
           dB = (ival > 0) ? 10*log10(ival*MAXDIV) : -1000000.0;
           vu[track] = _MINMAX(100*(dB-MIN_DB)/(MAX_DB-MIN_DB), 0, 99);
       }
       ui->VUleft->setValue(vu[0]);
       ui->VUright->setValue(vu[1]);

       QApplication::processEvents();
   }
}

void
AeonWavePlayer::exit()
{
    qApp->exit();
}

void
AeonWavePlayer::togglePlay()
{
    if (!playing && indev)
    {
        _ATB(aaxSensorSetState(indev, AAX_CAPTURING));

        float freq = (float)aaxMixerGetSetup(indev, AAX_FREQUENCY);
        float hour, minutes, seconds;

        max_samples = (float)aaxMixerGetSetup(indev, AAX_SAMPLES_MAX);
        seconds = max_samples/freq;
        hour = floorf(seconds/(60.0f*60.0f));
        seconds -= hour*60.0f*60.0f;
        minutes = floorf(seconds/60.0f);
        seconds -= minutes*60.0f;

        QString total = QString("%1:%2:%3").arg(hour,2,'f',0,'0').arg(minutes,2,'f',0,'0').arg(seconds,2,'f',0,'0');
        ui->timeTotal->setText(total);
        
        playing = true;
    }
}

void
AeonWavePlayer::togglePause()
{
    if (playing && indev)
    {
        playing = false;
        _ATB(aaxSensorSetState(indev, AAX_SUSPENDED));
    }
}

void
AeonWavePlayer::toggleStop()
{
    playing = false;
    _ATB(aaxSensorSetState(indev, AAX_STOPPED));
    ui->timeTotal->setText("00:00:00");
    max_samples = 0;
}

void
AeonWavePlayer::toggleRecord()
{
    if (recording)
    {
        recording = false;
        aaxMixerSetState(file, AAX_STOPPED);
    }
    else
    {
        if (!file) {
            saveTo();
        }
        if (file)
        {
            aaxMixerSetState(file, AAX_CAPTURING);
            recording = true;
        }
    }
}

void
AeonWavePlayer::volumeChanged(int val)
{
    aaxFilter flt = aaxMixerGetFilter(outdev, AAX_VOLUME_FILTER);
    aaxFilterSetParam(flt, AAX_GAIN, AAX_LINEAR, (float)val/100.0f);
    aaxMixerSetFilter(outdev, flt);
    aaxFilterDestroy(flt);
}

void
AeonWavePlayer::loadFile()
{
    QString filter = "*.wav";
    aaxConfig cfgi;
 
    cfgi = aaxDriverGetByName("AeonWave on Audio Files", AAX_MODE_READ);
    if (cfgi)
    {
        const char *d, *f;

        d = aaxDriverGetDeviceNameByPos(cfgi, 0, AAX_MODE_WRITE_STEREO);
        f = aaxDriverGetInterfaceNameByPos(cfgi, d, 0, AAX_MODE_WRITE_STEREO);
        filter = f;
        aaxDriverDestroy(cfgi);
    }

    QString fileName = QFileDialog::getOpenFileName(this,
                                    tr("Open Audio Input File"),
                                    infiles_path, filter);
    if (!fileName.isNull())
    {
        QString path = fileName;
        size_t fpos = path.lastIndexOf('/');
        infiles_path = path.mid(0, fpos);

        QString idevname_str = "AeonWave on Audio Files: "+path;
        std::string d = std::string(idevname_str.toUtf8().constData());
        const char *dev = d.empty() ? NULL : d.c_str();

        if (indev)
        {
            aaxMixerSetState(indev, AAX_STOPPED);
            aaxMixerDeregisterSensor(outdev, indev);
            aaxDriverClose(indev);
            aaxDriverDestroy(indev);
        }

        indev = aaxDriverOpenByName(dev, AAX_MODE_READ);
        if (indev)
        {
            _ATB(aaxMixerRegisterSensor(outdev, indev));
            _ATB(aaxMixerSetState(indev, AAX_INITIALIZED));

            ui->volume->setValue(80);
        }
    }
}

void
AeonWavePlayer::saveTo()
{
    QString filter = "*.wav";
    aaxConfig cfgo;

    cfgo = aaxDriverGetByName("AeonWave on Audio Files", AAX_MODE_READ);
    if (cfgo)
    {
        const char *d, *f;

        d = aaxDriverGetDeviceNameByPos(cfgo, 0, AAX_MODE_WRITE_STEREO);
        f = aaxDriverGetInterfaceNameByPos(cfgo, d, 0, AAX_MODE_WRITE_STEREO);
        filter = f;
        aaxDriverDestroy(cfgo);
    }

    QString fileName = QFileDialog::getSaveFileName(this,
                                          tr("Open Audio Output File"),
                                          outfiles_path, filter);
    if (!fileName.isNull())
    {
        QFileInfo f(fileName);

        if (f.suffix().isEmpty()) {
           fileName += ".wav";
        }
        outfiles_path = f.absolutePath();

        QString odevname_str = "AeonWave on Audio Files: "+fileName;
        std::string d = std::string(odevname_str.toUtf8().constData());
        const char *dev = d.empty() ? NULL : d.c_str();

        file = aaxDriverOpenByName(dev, AAX_MODE_READ);
        if (file)
        {
            aaxMixerSetState(file, AAX_INITIALIZED);
        }
    }
}

void
AeonWavePlayer::alert(QString msg)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("AeonWave Audio Player"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setTextFormat(Qt::RichText);
    if (!msg.isEmpty())
    {
        msgBox.setText(msg);
        msgBox.exec();
   }
}

