/*
 * Copyright 2012 by Erik Hofman.
 * Copyright 2012 by Adalin B.V.
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

#include "filetype.h"

_aaxExtensionDetect* _aaxFileTypes[] =
{
   _aaxDetectWavFile,
/* _aaxDetectAiffFile, */
/* _aaxDetectFLACFile, */
/* _aaxDetectMP3File,  */
/* _aaxDetectVorbisFile, */

   0		/* Must be last */
};

