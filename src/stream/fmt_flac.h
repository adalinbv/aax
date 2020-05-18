/*
 * Copyright 2016-2020 by Erik Hofman.
 * Copyright 2016-2020. by Adalin B.V.
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

#ifndef __FILE_FMT_FLAC_H
#define __FILE_FMT_FLAC_H 1

/*
 * Public domain flac decoder:
 *
 * http://mackron.github.io/dr_flac.html
 */
#define DR_FLAC_IMPLEMENTATION	1
#define DR_FLAC_NO_STDIO	1
#define DR_FLAC_NO_CRC		1
#define DR_FLAC_NO_OGG		1
#include "3rdparty/dr_flac.h"

#define FRAME_SIZE	960
#define MAX_FRAME_SIZE	(6*960)
#define MAX_PACKET_SIZE	(3*1276)
#define MAX_FLACBUFSIZE	(2*DR_FLAC_BUFFER_SIZE)
#define MAX_PCMBUFSIZE	8291

#endif /* __FILE_FMT_FLAC_H */

