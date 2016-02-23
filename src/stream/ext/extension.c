/*
 * Copyright 2012-2015 by Erik Hofman.
 * Copyright 2012-2015 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "extension.h"

_aaxExtensionDetect* _aaxFormatTypes[] =
{
   _aaxDetectWavFormat,
   _aaxDetectMP3Format,
/* _aaxDetectAiffFormat, */
/* _aaxDetectFLACFormat, */
/* _aaxDetectVorbisFormat, */

   0		/* Must be last */
};

