/*
 * Copyright 2007-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <aax/aax.h>

#include <base/gmath.h>
#include <base/geometry.h>

#include "api.h"

AAX_API int AAX_APIENTRY
aaxMatrixCopyMatrix(aaxMtx4f dmtx, const aaxMtx4f smtx)
{
   int rv = AAX_FALSE;
   if (dmtx && smtx) {
        mtx4fFill(dmtx, smtx);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrix64CopyMatrix64(aaxMtx4d dmtx, const aaxMtx4d smtx)
{
   int rv = AAX_FALSE;
   if (dmtx && smtx) {
        mtx4dFill(dmtx, smtx);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrixSetIdentityMatrix(aaxMtx4f mtx)
{
   int rv = AAX_FALSE;
   if (mtx)
   {
      mtx4fSetIdentity(mtx);
      rv = AAX_TRUE;
   }
   else {
      __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrix64SetIdentityMatrix(aaxMtx4d mtx)
{
   int rv = AAX_FALSE;
   if (mtx)
   {
      mtx4dSetIdentity(mtx);
      rv = AAX_TRUE;
   }
   else {
      __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrixTranslate(aaxMtx4f mtx, float dx, float dy, float dz)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4((const float(*)[4])mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (is_nan(dx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (is_nan(dy)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else if (is_nan(dz)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 3, __func__);
      } else {
         rv  = AAX_TRUE;
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

AAX_API int AAX_APIENTRY
aaxMatrix64Translate(aaxMtx4d mtx, double dx, double dy, double dz)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4d((const double(*)[4])mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (is_nan64(dx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (is_nan64(dy)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else if (is_nan64(dz)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 3, __func__);
      } else {
         rv = AAX_TRUE;
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

AAX_API int AAX_APIENTRY
aaxMatrixRotate(aaxMtx4f mtx, float angle_rad, float x, float y, float z)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4((const float(*)[4])mtx)) {
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
         rv  = AAX_TRUE;
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

AAX_API int AAX_APIENTRY
aaxMatrix64Rotate(aaxMtx4d mtx, double angle_rad, double x, double y, double z)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4d((const double(*)[4])mtx)) {
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
         rv = AAX_TRUE;
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


AAX_API int AAX_APIENTRY
aaxMatrixMultiply(aaxMtx4f mtx1, const aaxMtx4f mtx2)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx1 || detect_nan_mtx4((const float(*)[4])mtx1)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!mtx2 || detect_nan_mtx4((const float(*)[4])mtx2)) {
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

AAX_API int AAX_APIENTRY
aaxMatrix64Multiply(aaxMtx4d mtx1, const aaxMtx4d mtx2)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx1 || detect_nan_mtx4d((const double(*)[4])mtx1)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!mtx2 || detect_nan_mtx4d((const double(*)[4])mtx2)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else {
         rv = AAX_TRUE;
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

AAX_API int AAX_APIENTRY
aaxMatrixInverse(aaxMtx4f mtx)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4((const float(*)[4])mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else {
         rv = AAX_TRUE;
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

AAX_API int AAX_APIENTRY
aaxMatrix64Inverse(aaxMtx4d mtx)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx || detect_nan_mtx4d((const double(*)[4])mtx)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else {
         rv = AAX_TRUE;
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

AAX_API int AAX_APIENTRY
aaxMatrixSetDirection(aaxMtx4f mtx, const aaxVec3f pos, const aaxVec3f at)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!pos || detect_nan_vec3(pos)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (!at || detect_nan_vec3(at)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      vec3f_t loc;
      mtx4f_t m;

      mtx4fSetIdentity(m.m4);
      if (at[0] || at[1] || at[2])
      {
         aaxVec3f up = { 0.0f, 1.0f, 0.0f }; 
         vec3f_t side, upwd, fwd, back;

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
         vec3fNormalize(&m.v34[0], &side);
         vec3fNormalize(&m.v34[1], &upwd);
         vec3fNormalize(&m.v34[2], &back);
      }

      vec3fFill(loc.v3, pos);
      vec3fNegate(&m.v34[3], &loc);
      mtx4fFill(mtx, m.m4);
#if 0
 printf("SetDirection:\n");
 PRINT_MATRIX(mtx);
#endif
   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrixSetOrientation(aaxMtx4f mtx, const aaxVec3f pos, const aaxVec3f at,
                                                          const aaxVec3f up)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!pos || detect_nan_vec3(pos)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else if (!at || detect_nan_vec3(at)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 2, __func__);
      } else if (!up || detect_nan_vec3(up)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 3, __func__);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      vec3f_t loc;
      mtx4f_t m;

      mtx4fSetIdentity(m.m4);
      if ((at[0] || at[1] || at[2]) || (up[0] || up[1] || up[2]))
      {
         vec3f_t side, upwd, fwd, back;

         vec3fFill(upwd.v3, up);
         vec3fFill(fwd.v3, at);
         vec3fCrossProduct(&side, &fwd, &upwd);

         vec3fNegate(&back, &fwd);
         vec3fNormalize(&m.v34[0], &side);
         vec3fNormalize(&m.v34[1], &upwd);
         vec3fNormalize(&m.v34[2], &back);
      }

      vec3fFill(&loc.v3, pos);
      vec3fNegate(&m.v34[3], &loc);
      mtx4fFill(mtx, &m.m4);

   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrixGetOrientation(aaxMtx4f mtx, aaxVec3f pos, aaxVec3f at, aaxVec3f up)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      if (pos) {
         vec3fFill(pos, mtx[3]);/* LOCATION */
      }
      if (at) {
         vec3fFill(at, mtx[2]);	/* DIR_UPWD */
      }
      if (up) {
         vec3fFill(up, mtx[1]); /* DIR_BACK */
      }
   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrix64ToMatrix(aaxMtx4f mtx, const aaxMtx4d mtx64)
{
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx) {
         __aaxErrorSet(AAX_INVALID_PARAMETER, __func__);
      } else if (!mtx64 || detect_nan_mtx4d((const double(*)[4])mtx64)) {
         __aaxErrorSet(AAX_INVALID_PARAMETER + 1, __func__);
      } else {
         rv = AAX_TRUE;
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
            mtx[i][j] = (float)mtx64[i][j];
         }
         while(j);
      }
      while(i);

   }

   return rv;
}
 
