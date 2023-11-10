/*
 * SPDX-FileCopyrightText: Copyright © 2016-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2016-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

