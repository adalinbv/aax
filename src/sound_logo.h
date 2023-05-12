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
__aax_sound_logo =  							   \
"<?xml version='1.0'?>"							   \
"<aeonwave>"								   \
 "<sound frequency='220' duration='3.0'>"				   \
  "<waveform src='triangle' pitch='0.51' voices='5' spread='0.9'/>"	   \
  "<waveform src='square' processing='modulate' ratio='0.8' pitch='8.'/>"  \
  "<waveform src='triangle' processing='add' ratio='0.3' pitch='0.94'/>"   \
  "<waveform src='pink-noise' processing='add' ratio='0.1' pitch='0.02'/>" \
 "</sound>"								   \
  "<emitter looping='true'>"						   \
  "<filter type='frequency' src='48db'>"				   \
   "<slot>"								   \
    "<param n='0'>1650</param>"						   \
    "<param n='1'>1.0</param>"						   \
    "<param n='2'>0.1</param>"						   \
    "<param n='3'>3.0</param>"						   \
   "</slot>"								   \
  "</filter>"								   \
  "<filter type='timed-gain'>"						   \
   "<slot>"								   \
    "<param n='0'>0.5</param>"						   \
    "<param n='1'>0.41</param>"						   \
    "<param n='2'>1.0</param>"						   \
    "<param n='3'>2.525</param>"					   \
   "</slot>"								   \
   "<slot>"								   \
    "<param n='0'>0.8</param>"						   \
    "<param n='1'>2.25</param>"						   \
   "</slot>"								   \
  "</filter>"								   \
  "<effect type='phasing' src='inverse-sine'>"				   \
   "<slot>"								   \
    "<param n='0'>0.3</param>"						   \
    "<param n='1'>0.1</param>"						   \
    "<param n='2'>0.3</param>"						   \
    "<param n='3'>0.5</param>"						   \
   "</slot>"								   \
  "</effect>"								   \
 "</emitter>"								   \
 "<audioframe>"								   \
  "<filter type='frequency' src='sine' stereo='1'>"			   \
   "<slot>"								   \
    "<param n='0'>110</param>"						   \
    "<param n='1'>0.0</param>"						   \
    "<param n='2'>1.0</param>"						   \
    "<param n='3'>10.0</param>"						   \
   "</slot>"								   \
   "<slot>"								   \
    "<param n='0'>1100</param>"						   \
    "<param n='3'>0.06</param>"						   \
   "</slot>"								   \
  "</filter>"								   \
  "<effect type='reverb'>"						   \
   "<slot>"								   \
    "<param n='0'>571.9</param>"					   \
    "<param n='1'>0.035</param>"					   \
    "<param n='2'>0.504</param>"					   \
    "<param n='3'>0.280</param>"					   \
   "</slot>"								   \
  "</effect>"								   \
 "</audioframe>"							   \
"</aeonwave>";

