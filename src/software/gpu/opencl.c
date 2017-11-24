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

#ifdef __APPLE__
        #include "OpenCL/opencl.h"
#else
        #include "CL/cl.h"
#endif

#include <ringbuffer.h>

typedef struct
{
   cl_context context;
   cl_command_queue queue;

} _aax_opencl_t;

void*
_aaxOpenCLCreate()
{
   _aax_opencl_t *handle = malloc(sizeof(_aax_opencl_t));
   if (handle)
   {
      cl_platform_id *platforms = NULL;
      cl_uint num_platforms = 0;

      clGetPlatformIDs (0, NULL, &num_platforms);
      if (num_platforms > 0)
      {
         platforms = malloc(num_platforms*sizeof(num_platforms));
         if (platforms)
         {
            cl_uint num_devices = 0;
            clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, NULL,
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

                  clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU,
                                 num_devices, devices, NULL);
                  handle->context = clCreateContext(ctx_props,
                                                    num_devices, devices,
                                                    NULL, NULL, NULL);
                  if (handle->context)
                  {
                     handle->queue = clCreateCommandQueueWithProperties(
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
   clReleaseCommandQueue(handle->queue);
   clReleaseContext(handle->context);
}


enum {
   SPTR = 0,
   CPTR,
   HCPTR
};

typedef struct {
   unsigned int snum, cnum, step;
   float v, threshold;
   float *ptr[3];
} _aax_convolution_t;

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

   clptr[SPTR] = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_USE_HOST_PTR,
                                d.snum*sizeof(float), sptr, 0);
   clptr[CPTR] = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_USE_HOST_PTR,
                                d.snum*sizeof(float), cptr, 0);
   clptr[HCPTR] = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,
                                 d.cnum*sizeof(float), hcptr, 0);

   mem_loc = (const void*)&d.ptr;
   clEnqueueNativeKernel(handle->queue, &_aax_convolution_kernel, &d, sizeof(d),
                         3, clptr, &mem_loc, 0, 0, &event);
   clWaitForEvents(1, &event);
}

