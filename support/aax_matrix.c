/*
 * Copyright 2007-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <aax/aax.h>

#include <base/gmath.h>
#include <base/geometry.h>

#include "api.h"

AAX_API bool AAX_APIENTRY
aaxMatrixCopyMatrix(aaxMtx4f dmtx, aaxMtx4f smtx)
{
   bool rv = false;
   if (dmtx && smtx)
   {
        mtx4fFill(dmtx, smtx);
        rv = true;
   }
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64CopyMatrix64(aaxMtx4d dmtx, aaxMtx4d smtx)
{
   bool rv = false;
   if (dmtx && smtx)
   {
        mtx4dFill(dmtx, smtx);
        rv = true;
   }
   return rv;
}


// AAX_API aaxMtx4f aaxIdentityMatrix;

AAX_API bool AAX_APIENTRY
aaxMatrixSetIdentityMatrix(aaxMtx4f mtx)
{
   bool rv = false;
   if (mtx)
   {
      mtx4fSetIdentity(mtx);
      rv = true;
   }
   else {
      __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
   }
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64SetIdentityMatrix(aaxMtx4d mtx)
{
   bool rv = false;
   if (mtx)
   {
      mtx4dSetIdentity(mtx);
      rv = true;
   }
   else {
      __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
   }
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrixTranslate(aaxMtx4f mtx, float dx, float dy, float dz)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4(mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (is_nan(dx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (is_nan(dy)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else if (is_nan(dz)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 3, __func__);
      } else {
         rv  = true;
      }
   }

   if (rv)
   {
      mtx4f_t m;

      mtx4fFill(m.m4, mtx);
      mtx4fTranslate(&m, dx, dy, dz);
      mtx4fFill(mtx, m.m4);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64Translate(aaxMtx4d mtx, double dx, double dy, double dz)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4d(mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (is_nan64(dx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (is_nan64(dy)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else if (is_nan64(dz)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 3, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      mtx4d_t m;

      mtx4dFill(m.m4, mtx);
      mtx4dTranslate(&m, dx, dy, dz);
      mtx4dFill(mtx, m.m4);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrixRotate(aaxMtx4f mtx, float angle_rad, float x, float y, float z)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4(mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (is_nan(angle_rad)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (is_nan(x)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else if (is_nan(y)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 3, __func__);
      } else if (is_nan(z)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 4, __func__);
      } else {
         rv  = true;
      }
   }

   if (rv)
   {
      mtx4f_t m;

      mtx4fFill(m.m4, mtx);
      mtx4fRotate(&m, angle_rad, x, y, z);
      mtx4fFill(mtx, m.m4);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64Rotate(aaxMtx4d mtx, double angle_rad, double x, double y, double z)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4d(mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (is_nan64(angle_rad)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (is_nan64(x)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else if (is_nan64(y)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 3, __func__);
      } else if (is_nan64(z)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 4, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      mtx4d_t m;

      mtx4dFill(m.m4, mtx);
      mtx4dRotate(&m, angle_rad, x, y, z);
      mtx4dFill(mtx, m.m4);
   }

   return rv;
}


AAX_API bool AAX_APIENTRY
aaxMatrixMultiply(aaxMtx4f mtx1, aaxMtx4f mtx2)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx1 || detect_nan_mtx4(mtx1)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!mtx2 || detect_nan_mtx4(mtx2)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      }
   }

   if (rv)
   {
      mtx4f_t m1, m2, m3;

      mtx4fFill(m1.m4, mtx1);
      mtx4fFill(m2.m4, mtx2);
      mtx4fMul(&m3, &m1, &m2);
      mtx4fFill(mtx1, m3.m4);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64Multiply(aaxMtx4d mtx1, aaxMtx4d mtx2)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx1 || detect_nan_mtx4d(mtx1)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!mtx2 || detect_nan_mtx4d(mtx2)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      mtx4d_t m1, m2, m3;

      mtx4dFill(m1.m4, mtx1);
      mtx4dFill(m2.m4, mtx2);
      mtx4dMul(&m3, &m1, &m2);
      mtx4dFill(mtx1, m3.m4);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrixInverse(aaxMtx4f mtx)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4(mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      mtx4f_t m1, m2;
      mtx4fFill(m1.m4, mtx);
      mtx4fInverseSimple(&m2, &m1);
      mtx4fFill(mtx, m2.m4);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64Inverse(aaxMtx4d mtx)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4d(mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      mtx4d_t m1, m2;
      mtx4dFill(m1.m4, mtx);
      mtx4dInverseSimple(&m2, &m1);
      mtx4dFill(mtx, m2.m4);
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64SetDirection(aaxMtx4d mtx64, aaxVec3d pos, aaxVec3f at)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx64) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!pos || detect_nan_vec3d(pos)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (!at || detect_nan_vec3(at)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      vec3d_t loc;
      mtx4d_t m;

      mtx4dSetIdentity(m.m4);
      if (at[0] || at[1] || at[2])
      {
         aaxVec3f up = { 0.0f, 1.0f, 0.0f }; 
         vec3f_t side, upwd, fwd, back, tmp;

         if ((fabsf(at[0]) < FLT_EPSILON)  && (fabsf(at[2]) < FLT_EPSILON))
         {  
            up[1] = 0.0f;
            if (at[2] < 0.0f) up[2] = -1.0f;
            else              up[2] =  1.0f;
         }  

         vec3fFill(upwd.v3, up);
         vec3fFill(fwd.v3, at);
         vec3fCrossProduct(&side, &fwd, &upwd);

         vec3fNegate(&back, &fwd);
         vec3fNormalize(&tmp, &side);
         vec3dFillf(m.v34[0].v3, tmp.v3);
         vec3fNormalize(&tmp, &upwd);
         vec3dFillf(m.v34[1].v3, tmp.v3);
         vec3fNormalize(&tmp, &back);
         vec3dFillf(m.v34[2].v3, tmp.v3);
      }

      vec3dFill(loc.v3, pos);
      vec3dNegate(&m.v34[3], &loc);
      mtx4dFill(mtx64, m.m4);
#if 0
 printf("SetDirection:\n");
 PRINT_MATRIX(mtx);
#endif
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64SetOrientation(aaxMtx4d mtx64, aaxVec3d pos, aaxVec3f at,
                                                        aaxVec3f up)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx64) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!pos || detect_nan_vec3d(pos)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (!at || detect_nan_vec3(at)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else if (!up || detect_nan_vec3(up)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 3, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      vec3d_t loc;
      mtx4d_t m;

      mtx4dSetIdentity(m.m4);
      if ((at[0] || at[1] || at[2]) || (up[0] || up[1] || up[2]))
      {
         vec3f_t side, upwd, fwd, back, tmp;

         vec3fFill(upwd.v3, up);
         vec3fFill(fwd.v3, at);
         vec3fCrossProduct(&side, &fwd, &upwd);

         vec3fNegate(&back, &fwd);
         vec3fNormalize(&tmp, &side);
         vec3dFillf(m.v34[0].v3, tmp.v3);
         vec3fNormalize(&tmp, &upwd);
         vec3dFillf(m.v34[1].v3, tmp.v3);
         vec3fNormalize(&tmp, &back);
         vec3dFillf(m.v34[2].v3, tmp.v3);
      }

      vec3dFill(loc.v3, pos);
      vec3dNegate(&m.v34[3], &loc);
      mtx4dFill(mtx64, m.m4);

   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrix64GetOrientation(aaxMtx4d mtx, aaxVec3d pos, aaxVec3f at, aaxVec3f up)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      if (pos) {
         vec3dFill(pos, mtx[3]);/* LOCATION */
      }
      if (at) {
         vec3fFilld(at, mtx[2]);/* DIR_UPWD */
      }
      if (up) {
         vec3fFilld(up, mtx[1]); /* DIR_BACK */
      }
   }

   return rv;
}

AAX_API bool AAX_APIENTRY
aaxMatrixToMatrix64(aaxMtx4d mtx64, aaxMtx4f mtx)
{
   bool rv = __release_mode;

   if (!rv)
   {
      if (!mtx64) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!mtx || detect_nan_mtx4(mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      int i, j;
      i = 4;
      do
      {
         i--;
         j = 4;
         do
         {
            j--;
            mtx64[i][j] = (double)mtx[i][j];
         }
         while(j);
      }
      while(i);

   }

   return rv;
}
 
