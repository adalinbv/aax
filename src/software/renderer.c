/*
 * Copyright 2014-2017 by Erik Hofman.
 * Copyright 2014-2017 by Adalin B.V.
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
#endif

#include "renderer.h"

_aaxRenderer*
_aaxSoftwareInitRenderer(float dt, enum aaxRenderMode mode, int registered)
{
   _aaxRenderer *rv = NULL;

   if (registered || mode == AAX_MODE_READ)
   {
      _aaxRendererDetect* rtype = _aaxRenderTypes[0];
      return  rtype();
   }
   else if (!rv)
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
         rv = rtype();
      }
   }

   if (rv)
   {
      _aaxRenderer* type = rv;
      if (type->detect())
      {
         type->id = type->setup(_MIN(floorf(1000.0f * dt), 1));
         if (type->id)
         {
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

