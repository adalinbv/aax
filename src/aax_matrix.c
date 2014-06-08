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
aaxMatrixSetIdentityMatrix(aaxMtx4f mtx)
{
   int rv = AAX_FALSE;
   if (mtx)
   {
      mtx4Copy(mtx, aaxIdentityMatrix);
      rv = AAX_TRUE;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrix64SetIdentityMatrix(aaxMtx4d mtx)
{
   int rv = AAX_FALSE;
   if (mtx)
   {
      mtx4dCopy(mtx, aaxIdentityMatrix64);
      rv = AAX_TRUE;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrixTranslate(aaxMtx4f mtx, float dx, float dy, float dz)
{
   if (_client_release_mode) goto finish;

   if (mtx && !detect_nan_mtx4((const float(*)[4])mtx))
   {
      if (!is_nan(dx))
      {
         if (!is_nan(dy))
         {
            if (!is_nan(dz))
finish:
            {
               mtx4_t m;

               mtx4Copy(m, mtx);
               mtx4Translate(m, dx, dy, dz);
               mtx4Copy(mtx, m);
               return AAX_TRUE;
            }
            else {
               _aaxErrorSet(AAX_INVALID_PARAMETER + 3);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 2);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrix64Translate(aaxMtx4d mtx, double dx, double dy, double dz)
{
   if (_client_release_mode) goto finish;

   if (mtx && !detect_nan_mtx4d((const double(*)[4])mtx))
   {
      if (!is_nan64(dx))
      {
         if (!is_nan64(dy))
         {
            if (!is_nan64(dz))
finish:
            {
               mtx4d_t m;

               mtx4dCopy(m, mtx);
               mtx4dTranslate(m, dx, dy, dz);
               mtx4dCopy(mtx, m);
               return AAX_TRUE;
            }
            else {
               _aaxErrorSet(AAX_INVALID_PARAMETER + 3);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 2);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrixRotate(aaxMtx4f mtx, float angle_rad, float x, float y, float z)
{
   if (_client_release_mode) goto finish;

   if (mtx && !detect_nan_mtx4((const float(*)[4])mtx))
   {
      if (!is_nan(angle_rad))
      {
         if (!is_nan(x))
         {
            if (!is_nan(y))
            {
               if (!is_nan(z))
finish:
               {
                  mtx4_t m;

                  mtx4Copy(m, mtx);
                  mtx4Rotate(m, angle_rad, x, y, z);
                  mtx4Copy(mtx, m);
                  return AAX_TRUE;
               }
               else {
                  _aaxErrorSet(AAX_INVALID_PARAMETER + 4);
               }
            }
            else {
               _aaxErrorSet(AAX_INVALID_PARAMETER + 3);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 2);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrix64Rotate(aaxMtx4d mtx, double angle_rad, double x, double y, double z)
{
   if (_client_release_mode) goto finish;

   if (mtx && !detect_nan_mtx4d((const double(*)[4])mtx))
   {
      if (!is_nan64(angle_rad))
      {
         if (!is_nan64(x))
         {
            if (!is_nan64(y))
            {
               if (!is_nan64(z))
finish:
               {
                  mtx4d_t m;

                  mtx4dCopy(m, mtx);
                  mtx4dRotate(m, angle_rad, x, y, z);
                  mtx4dCopy(mtx, m);
                  return AAX_TRUE;
               }
               else {
                  _aaxErrorSet(AAX_INVALID_PARAMETER + 4);
               }
            }
            else {
               _aaxErrorSet(AAX_INVALID_PARAMETER + 3);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 2);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}


AAX_API int AAX_APIENTRY
aaxMatrixMultiply(aaxMtx4f mtx1, aaxMtx4f mtx2)
{
   if (_client_release_mode) goto finish;

   if (mtx1 && !detect_nan_mtx4((const float(*)[4])mtx1))
   {
      if (mtx2 && !detect_nan_mtx4((const float(*)[4])mtx2))
finish:
      {
         mtx4_t m1, m2, m3;

         mtx4Copy(m1, mtx1);
         mtx4Copy(m2, mtx2);
         mtx4Mul(m3, m1, m2);
         mtx4Copy(mtx1, m3);
         return AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrix64Multiply(aaxMtx4d mtx1, aaxMtx4d mtx2)
{
   if (_client_release_mode) goto finish;

   if (mtx1 && !detect_nan_mtx4d((const double(*)[4])mtx1))
   {
      if (mtx2 && !detect_nan_mtx4d((const double(*)[4])mtx2))
finish:
      {
         mtx4d_t m1, m2, m3;

         mtx4dCopy(m1, mtx1);
         mtx4dCopy(m2, mtx2);
         mtx4dMul(m3, m1, m2);
         mtx4dCopy(mtx1, m3);
         return AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrixInverse(aaxMtx4f mtx)
{
   if (_client_release_mode) goto finish;

   if (mtx && !detect_nan_mtx4((const float(*)[4])mtx))
finish:
   {
      mtx4_t m1, m2;
      mtx4Copy(m1, mtx);
      mtx4InverseSimple(m2, m1);
      mtx4Copy(mtx, m2);
      return AAX_TRUE;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrix64Inverse(aaxMtx4d mtx)
{
   if (_client_release_mode) goto finish;

   if (mtx && !detect_nan_mtx4d((const double(*)[4])mtx))
finish:
   {
      mtx4d_t m1, m2;
      mtx4dCopy(m1, mtx);
      mtx4dInverseSimple(m2, m1);
      mtx4dCopy(mtx, m2);
      return AAX_TRUE;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrixSetDirection(aaxMtx4f mtx, const aaxVec3f pos, const aaxVec3f at)
{
   if (_client_release_mode) goto finish;

   if (mtx)
   {
      if (pos && !detect_nan_vec3(pos))
      {
          if (at && !detect_nan_vec3(at))
finish:
          {
            mtx4Copy(mtx, aaxIdentityMatrix);
            if (at[0] || at[1] || at[2])
            {
               aaxVec3f up = { 0.0f, 1.0f, 0.0f }; 
               vec3_t side, upwd, fwd, back;

               if ((fabs(at[0]) < FLT_EPSILON)  && (fabs(at[2]) < FLT_EPSILON))
               {  
                  up[1] = 0.0f;
                  if (at[2] < 0.0f) up[2] = -1.0f;
                  else              up[2] =  1.0f;
               }  

               vec3Copy(upwd, up);
               vec3Copy(fwd, at);
               vec3CrossProduct(side, fwd, upwd);

               vec3Negate(back, fwd);
               vec3Normalize(mtx[0], side);
               vec3Normalize(mtx[1], up);
               vec3Normalize(mtx[2], back);
            }
            vec3Negate(mtx[3], pos);
#if 0
 printf("SetDirection:\n");
 PRINT_MATRIX(mtx);
#endif
            return AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 2);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrixSetOrientation(aaxMtx4f mtx, const aaxVec3f pos, const aaxVec3f at,
                                                          const aaxVec3f up)
{
   if (_client_release_mode) goto finish;

   if (mtx)
   {
      if (pos && !detect_nan_vec3(pos))
      { 
         if (at && !detect_nan_vec3(at))
         {
            if (up && !detect_nan_vec3(up))
finish:
            {
               mtx4Copy(mtx, aaxIdentityMatrix);
               if (at[0] || at[1] || at[2] || up[0] || up[1] || up[2])
               {
                  vec3_t side, upwd, fwd, back;

                  vec3Copy(upwd, up);
                  vec3Copy(fwd, at);
                  vec3CrossProduct(side, fwd, upwd);

                  vec3Negate(back, fwd);
                  vec3Normalize(mtx[0], side);
                  vec3Normalize(mtx[1], up);
                  vec3Normalize(mtx[2], back);
               }
               vec3Negate(mtx[3], pos);
               return AAX_TRUE;
            }
            else {
               _aaxErrorSet(AAX_INVALID_PARAMETER + 3);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 2);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxMatrixGetOrientation(aaxMtx4f mtx, aaxVec3f pos, aaxVec3f at, aaxVec3f up)
{
   int rv = AAX_FALSE;
   if (mtx)
   {
      if (pos || at || up)
      {
         if (pos) {
            vec3Copy(pos, mtx[3]);	/* LOCATION */
         }
         if (at) {
            vec3Copy(at, mtx[2]);	/* DIR_UPWD */
         }
         if (up) {
            vec3Copy(up, mtx[1]); /* DIR_BACK */
         }
         rv = AAX_TRUE;
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMatrix64ToMatrix(aaxMtx4f mtx, aaxMtx4d mtx64)
{
   if (_client_release_mode) goto finish;

   if (mtx && mtx64 && !detect_nan_mtx4d((const double(*)[4])mtx64))
finish:
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

      return AAX_TRUE;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return AAX_FALSE;
}
 
