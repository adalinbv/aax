/*
 * Copyright 2016-2017 by Erik Hofman.
 * Copyright 2016-2017. by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#ifndef __FILE_FMT_VORBIS_H
#define __FILE_FMT_VORBIS_H 1

#include <aax/aax.h>
#include <driver.h>

/*
 * Public domain vorbis decoder:
 *
 * http://nothings.org/stb_vorbis/
 * https://github.com/nothings/stb
 */

#define STB_VORBIS_NO_PULLDATA_API		1
#define STB_VORBIS_MAX_CHANNELS			_AAX_MAX_SPEAKERS
#ifndef TRUE
# define TRUE					AAX_TRUE
#endif
#ifndef FALSE
# define FALSE					AAX_FALSE
#endif
#ifdef alloca
# undef alloca
#endif

#define STB_VORBIS_HEADER_ONLY
#include <3rdparty/stb_vorbis.c> 

#undef FALSE
#undef TRUE


#endif /* __FILE_FMT_VORBIS_H */

