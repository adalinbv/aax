/*
 * Copyright 2016-2017 by Erik Hofman.
 * Copyright 2016-2017. by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
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
#define TRUE					AAX_TRUE
#define FALSE					AAX_FALSE

#define STB_VORBIS_HEADER_ONLY
#include "3rdparty/stb_vorbis.c" 

#undef FALSE
#undef TRUE


#endif /* __FILE_FMT_VORBIS_H */

