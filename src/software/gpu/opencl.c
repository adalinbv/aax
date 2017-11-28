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
DECL_FUNCTION(clEnqueueWriteBuffer);
DECL_FUNCTION(clReleaseMemObject);
DECL_FUNCTION(clCreateProgramWithSource);
DECL_FUNCTION(clBuildProgram);
DECL_FUNCTION(clReleaseProgram);
DECL_FUNCTION(clCreateKernel);
DECL_FUNCTION(clReleaseKernel);
DECL_FUNCTION(clSetKernelArg);
DECL_FUNCTION(clGetKernelWorkGroupInfo);
DECL_FUNCTION(clEnqueueNDRangeKernel);
DECL_FUNCTION(clFlush);
DECL_FUNCTION(clFinish);
#if 0
DECL_FUNCTION(clEnqueueNativeKernel);
DECL_FUNCTION(clWaitForEvents);
#endif

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
         TIE_FUNCTION(clEnqueueWriteBuffer);
         TIE_FUNCTION(clReleaseMemObject);
         TIE_FUNCTION(clCreateProgramWithSource);
         TIE_FUNCTION(clBuildProgram);
         TIE_FUNCTION(clReleaseProgram);
         TIE_FUNCTION(clCreateKernel);
         TIE_FUNCTION(clReleaseKernel);
         TIE_FUNCTION(clSetKernelArg);
         TIE_FUNCTION(clGetKernelWorkGroupInfo);
         TIE_FUNCTION(clEnqueueNDRangeKernel);
         TIE_FUNCTION(clFlush);
         TIE_FUNCTION(clFinish);
#if 0
         TIE_FUNCTION(clEnqueueNativeKernel);
         TIE_FUNCTION(clWaitForEvents);
#endif

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
   _aax_opencl_t *handle = calloc(1, sizeof(_aax_opencl_t));
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
            handle->num_devices = 0;
            pclGetPlatformIDs(num_platforms, platforms, NULL);
            pclGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, NULL,
                            &handle->num_devices);
            if (handle->num_devices > 0)
            {
               handle->devices = malloc(handle->num_devices*sizeof(cl_device_id));
               if (handle->devices)
               {
                  const cl_context_properties ctx_props[] = {
                     CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0],
                     0, 0
                  };

                  pclGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU,
                                  handle->num_devices, handle->devices, NULL);
                  handle->context = pclCreateContext(ctx_props,
                                           handle->num_devices, handle->devices,
                                           NULL, NULL, NULL);
                  if (handle->context)
                  {
                     handle->queue = pclCreateCommandQueueWithProperties(
                                                           handle->context,
                                                           handle->devices[0],
                                                           0, NULL);
                  }
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
   pclReleaseProgram(handle->program);
   pclReleaseKernel(handle->kernel);
   pclReleaseCommandQueue(handle->queue);
   pclReleaseContext(handle->context);
   free(handle->devices);
   free(handle);
}


// irnum = convolution->no_samples
// for (q=0; q<snum; ++q) {
//    float volume = *sptr++;
//    rbd->add(hptr++, cptr, irnum, volume, 0.0f);
// }
const char *_aax_convolution_kernel = \
"__kernel void convolution(__global float* c, __global float* h, int q, float g) { "\
  "unsigned int i = get_global_id(0); " \
  "h[q+i] += c[i]*g; }";

void
_aaxOpenCLRunConvolution(_aax_opencl_t *handle, _aaxRingBufferConvolutionData *convolution, float *sptr, unsigned int snum, unsigned int t)
{
   unsigned int q, cnum, hpos;
   size_t global, local;
   MIX_T *hptr, *hcptr;
   cl_context context;
   cl_mem cl_hptr;
   cl_int err;
   float v;

   context = handle->context;
   cnum = convolution->no_samples;

   if (!handle->kernel)
   {
      MIX_T *cptr = convolution->sample;

      handle->program = pclCreateProgramWithSource(context, 1, 
                                          &_aax_convolution_kernel, NULL, &err);
      if (!handle->program || err) return;

      err = pclBuildProgram(handle->program, 1, handle->devices,NULL,NULL,NULL);
      if (err) return;
   
      handle->kernel = pclCreateKernel(handle->program, "convolution", &err);
      if (!handle->kernel || err) return;

       err = pclGetKernelWorkGroupInfo(handle->kernel, handle->devices[0],
                        CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
       if (err) return;
       handle->local = local;

       handle->sample = pclCreateBuffer(context, CL_MEM_READ_ONLY,
                                        cnum*sizeof(float), NULL, 0);
       if (!handle->sample) return;

       err = pclEnqueueWriteBuffer(handle->queue, handle->sample, CL_TRUE, 0,
                                   cnum*sizeof(float), cptr, 0, NULL, NULL);
       if (err) return;
       pclSetKernelArg(handle->kernel, 0, sizeof(cl_mem), &handle->sample);
   }

   hptr = convolution->history[t];
   hpos = convolution->history_start[t];
   hcptr = hptr + hpos;

   cl_hptr = pclCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,
                             (snum+cnum)*sizeof(float), hcptr, 0);
   if (!cl_hptr) return;
   pclSetKernelArg(handle->kernel, 1, sizeof(cl_mem), &cl_hptr);

   local = handle->local;
   global = ceil(cnum/local)*local;

   v = convolution->rms * convolution->delay_gain;
   for (q=0; q<snum; ++q)
   {
      float volume = *sptr++ * v;
      pclSetKernelArg(handle->kernel, 2, sizeof(cl_int), &q);
      pclSetKernelArg(handle->kernel, 3, sizeof(cl_float), &volume);
      err = pclEnqueueNDRangeKernel(handle->queue, handle->kernel, 1, NULL,
                                    &global, &local, 0, NULL, NULL);
      if (err) break;
      pclFinish(handle->queue);
   }
   pclReleaseMemObject(cl_hptr);
}

