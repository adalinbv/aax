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

class Ui_Player;

class AeonWavePlayer : public QDialog
{
    O_OBJECT

public:
    AeonWavePlayer(QWidget *parent = 0);
    ~AeonWavePlayer();

    Ui_Player *ui;
};

extern AeonWavePlayer *_mw;

#endif /* !AAXPLAYER_H */
