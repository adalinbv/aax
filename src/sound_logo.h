/*
 * Copyright 2017-2023 by Erik Hofman.
 * Copyright 2017-2023 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

const char *
__aax_sound_logo =  							\
"<?xml version='1.0'?>"							\
"<aeonwave>"								\
 "<sound frequency='220' db='-21' duration='3.0'>"				\
  "<waveform src='triangle' pitch='0.51' voices='5' spread='0.9'/>"	\
  "<waveform src='square' processing='modulate' ratio='0.8' pitch='8.'/>"\
  "<waveform src='triangle' processing='add' ratio='0.3' pitch='0.94'/>"\
  "<waveform src='pink-noise' processing='add' ratio='0.1' pitch='0.02'/>"\
  "<filter type='frequency' src='48db'>"				\
   "<slot>"								\
    "<param>1650</param>"						\
    "<param>1.0</param>"						\
    "<param>0.1</param>"						\
    "<param>3.0</param>"						\
   "</slot>"								\
  "</filter>"								\
 "</sound>"								\
 "<emitter looping='true'>"						\
  "<filter type='timed-gain'>"						\
   "<slot>"								\
    "<param>0.5</param>"						\
    "<param>0.41</param>"						\
    "<param>1.0</param>"						\
    "<param>2.525</param>"						\
   "</slot>"								\
   "<slot>"								\
    "<param>0.8</param>"						\
    "<param>2.25</param>"						\
   "</slot>"								\
  "</filter>"								\
  "<effect type='phasing' src='inverse-sine'>"				\
   "<slot>"								\
    "<param>0.3</param>"						\
    "<param>0.1</param>"						\
    "<param>0.3</param>"						\
    "<param>0.5</param>"						\
   "</slot>"								\
  "</effect>"								\
 "</emitter>"								\
 "<audioframe>"								\
  "<filter type='frequency' src='sine' stereo='1'>"			\
   "<slot>"								\
    "<param>110</param>"						\
    "<param>0.0</param>"						\
    "<param>1.0</param>"						\
    "<param>10.0</param>"						\
   "</slot>"								\
   "<slot>"								\
    "<param>1100</param>"						\
    "<param n='3'>0.06</param>"						\
   "</slot>"								\
  "</filter>"								\
 "</audioframe>"							\
"</aeonwave>";

