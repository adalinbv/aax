/*
 * Copyright 2014 by Erik Hofman.
 * Copyright 2014 by Adalin B.V.
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

#include "rendertype.h"

_aaxRenderer*
_aaxSoftwareInitRenderer(float dt)
{
   static _aaxRenderer *rv = NULL;

   if (!rv)
   {
      _aaxRendererDetect* rtype;
      int i = -1, found = -1;

      /* first find the last available renderer */
      while ((rtype = _aaxRenderTypes[++i]) != NULL)
      {
         _aaxRenderer* type = rtype();
         if (type && type->detect()) {
            found = i;
         }
         free(type);
      }

      if (found >= 0)
      {
         _aaxRendererDetect* rtype = _aaxRenderTypes[found];
         _aaxRenderer* type = rtype();
         if (type && type->detect())
         {
            type->id = type->setup(floorf(1000.0f * dt));
            type->open(type->id);
            rv = type;
         }
      }
   }

   return rv;
}

/* -------------------------------------------------------------------------- */

_aaxRendererDetect* _aaxRenderTypes[] =
{
   _aaxDetectCPURenderer,
   _aaxDetectPoolRenderer,

   0            /* Must be last */
};

