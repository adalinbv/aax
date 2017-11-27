/*
 * Copyright 2017 by Erik Hofman.
 * Copyright 2017 by Adalin B.V.
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

#include <math.h>

#include <base/dlsym.h>

#include <api.h>

#include "gpu.h"

DECL_FUNCTION(clGetDeviceIDs);
DECL_FUNCTION(clGetPlatformIDs);
DECL_FUNCTION(clCreateContext);
DECL_FUNCTION(clReleaseContext);
DECL_FUNCTION(clCreateCommandQueueWithProperties);
DECL_FUNCTION(clReleaseCommandQueue);
DECL_FUNCTION(clCreateBuffer);
DECL_FUNCTION(clEnqueueNativeKernel);
DECL_FUNCTION(clWaitForEvents);

int
_aaxOpenCLDetect()
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;

   if TEST_FOR_FALSE(rv) {
      audio = _aaxIsLibraryPresent("OpenCL", "1");
   }

   if (audio)
   {
      _aaxGetSymError(0);

      TIE_FUNCTION(clGetPlatformIDs);
      if (pclGetPlatformIDs)
      {
         char *error = 0;

         TIE_FUNCTION(clGetDeviceIDs);
         TIE_FUNCTION(clCreateContext);
         TIE_FUNCTION(clReleaseContext);
         TIE_FUNCTION(clCreateCommandQueueWithProperties);
         TIE_FUNCTION(clReleaseCommandQueue);
         TIE_FUNCTION(clCreateBuffer);
         TIE_FUNCTION(clEnqueueNativeKernel);
         TIE_FUNCTION(clWaitForEvents);

         error = _aaxGetSymError(0);
         if (!error)
         {
            cl_platform_id *platforms = NULL;
            cl_uint num_platforms = 0;

            pclGetPlatformIDs(0, NULL, &num_platforms);
            if (num_platforms > 0)
            {
               platforms = malloc(num_platforms*sizeof(cl_platform_id));
               if (platforms)
               {
                  cl_uint num_devices = 0;
                  pclGetPlatformIDs(num_platforms, platforms, NULL);
                  pclGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, NULL,
                                  &num_devices);
                  if (num_devices > 0) {
                     rv = AAX_TRUE;
                  }
                  free(platforms);
               }
            }
         }
      }
   }

   return rv;
}

_aax_opencl_t*
_aaxOpenCLCreate()
{
   _aax_opencl_t *handle = malloc(sizeof(_aax_opencl_t));
   if (handle)
   {
      cl_platform_id *platforms = NULL;
      cl_uint num_platforms = 0;

      pclGetPlatformIDs(0, NULL, &num_platforms);
      if (num_platforms > 0)
      {
         platforms = malloc(num_platforms*sizeof(cl_platform_id));
         if (platforms)
         {
            cl_uint num_devices = 0;
            pclGetPlatformIDs(num_platforms, platforms, NULL);
            pclGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, NULL,
                            &num_devices);
            if (num_devices > 0)
            {
               cl_device_id *devices = malloc(num_devices*sizeof(cl_device_id));
               if (devices)
               {
                  const cl_context_properties ctx_props[] = {
                     CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0],
                     0, 0
                  };

                  pclGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU,
                                  num_devices, devices, NULL);
                  handle->context = pclCreateContext(ctx_props,
                                                     num_devices, devices,
                                                     NULL, NULL, NULL);
                  if (handle->context)
                  {
                     handle->queue = pclCreateCommandQueueWithProperties(
                                                           handle->context,
                                                           devices[0], 0, NULL);
                  }
                  free(devices);
               }
            }
            free(platforms);
         }
      }

      if (!handle->queue)
      {
         free(handle);
         handle = NULL;
      }
   }
   return handle;
}

void
_aaxOpenCLDestroy(_aax_opencl_t *handle)
{
   pclReleaseCommandQueue(handle->queue);
   pclReleaseContext(handle->context);
   free(handle);
}


static void
_aax_convolution_kernel(void *args)
{
   _aax_convolution_t *data = args;
   float *sptr, *cptr, *hcptr;
   unsigned int i, j;

   sptr = data->ptr[SPTR];
   cptr = data->ptr[CPTR];
   hcptr = data->ptr[HCPTR];

   for (j=0; j<data->cnum; j += data->step)
   {
      float volume = cptr[j] * data->v;
      if (fabsf(volume) > data->threshold)
      {
         for (i=0; i<data->snum; i++) {
            hcptr[j+i] += sptr[i]*volume;
         }
      }
   }
}

void
_aaxOpenCLRunConvolution(_aax_opencl_t *handle, _aaxRingBufferConvolutionData *convolution, float *sptr, unsigned int snum, unsigned int track)
{
   _aax_convolution_t d;
   const void *mem_loc;
   float *cptr, *hcptr;
   cl_context context;
   unsigned int hpos;
   cl_mem clptr[3];
   cl_event event;

   context = handle->context;
   hpos = convolution->history_start[track];
   
   d.step = convolution->step;
   d.threshold = convolution->threshold * (float)(1<<23);
   d.v = convolution->rms * convolution->delay_gain;

   d.snum = snum;
   d.cnum = convolution->no_samples - hpos;

   cptr = convolution->sample;
   hcptr = convolution->history[track] + hpos;

   clptr[SPTR] = pclCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_USE_HOST_PTR,
                                d.snum*sizeof(float), sptr, 0);
   clptr[CPTR] = pclCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_USE_HOST_PTR,
                                d.snum*sizeof(float), cptr, 0);
   clptr[HCPTR] = pclCreateBuffer(context,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,
                                 d.cnum*sizeof(float), hcptr, 0);

   mem_loc = (const void*)&d.ptr;
   pclEnqueueNativeKernel(handle->queue, &_aax_convolution_kernel, &d,sizeof(d),
                         3, clptr, &mem_loc, 0, 0, &event);
   pclWaitForEvents(1, &event);
}

