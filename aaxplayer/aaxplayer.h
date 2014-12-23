/*
 * Copyright (C) 2014 by Adalin B.V.
 *
 * This file is part of AeonWave-AudioPlayer
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

#ifndef AAXPLAYER_H
#define AAXPLAYER_H

#include <aax/aax.h>

#include <QtGui/QDialog>
#include <QTimer>

class Ui_AudioPlayer;

class AeonWavePlayer : public QDialog
{
    Q_OBJECT

public:
    AeonWavePlayer(QWidget *parent = 0);
    ~AeonWavePlayer();

    Ui_AudioPlayer *ui;

    aaxConfig outdev;
    aaxConfig indev;
    bool agc_enabled;
    bool playing;

    aaxConfig file;
    int bitrate;
    bool recording;

    aaxConfig openInputDevice();
    aaxConfig openOutputDevice();
    void closeDevices(bool keep = false);
    void freeDevices();

private:
    QTimer timer;
    QString outfiles_path;
    QString infiles_path;
    float duration;

    void alert(QString msg);

private slots:
    void togglePlay();
    void toggleStop();
    void togglePause();
    void toggleRecord();
    void volumeChanged(int);
    void loadFile();
    void saveTo();
    void exit();

protected slots:
    void tick();
    
};

extern AeonWavePlayer *_mw;

#endif /* !AAXPLAYER_H */
