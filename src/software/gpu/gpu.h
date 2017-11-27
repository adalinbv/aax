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

#ifndef _AAX_GPU_H
#define _AAX_GPU_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if 0
#ifdef __APPLE__
# include "OpenCL/opencl.h"
#else
# include "CL/cl.h"
#endif
#endif

#include <ringbuffer.h>
#include <base/types.h>

/* OpenCL declarations */
typedef void* cl_mem;
typedef void* cl_event;
typedef void* cl_context;
typedef void* cl_device_id;
typedef void* cl_platform_id;
typedef void* cl_command_queue;
typedef intptr_t cl_context_properties;

typedef int32_t cl_int;
typedef uint32_t cl_uint;
typedef int64_t cl_ulong;
typedef cl_ulong cl_bitfield;
typedef cl_bitfield cl_mem_flags;
typedef cl_bitfield cl_device_type;
typedef cl_bitfield cl_queue_properties;

#if defined(_WIN32)
# define CL_CALLBACK	__stdcall
#else
# define CL_CALLBACK
#endif

/* cl_device_type - bitfield */
#define CL_DEVICE_TYPE_DEFAULT		(1 << 0)
#define CL_DEVICE_TYPE_CPU		(1 << 1)
#define CL_DEVICE_TYPE_GPU		(1 << 2)
#define CL_DEVICE_TYPE_ACCELERATOR	(1 << 3)
#define CL_DEVICE_TYPE_CUSTOM		(1 << 4)
#define CL_DEVICE_TYPE_ALL		0xFFFFFFFF

/* cl_mem_flags and cl_svm_mem_flags - bitfield */
#define CL_MEM_READ_WRITE		(1 << 0)
#define CL_MEM_WRITE_ONLY		(1 << 1)
#define CL_MEM_READ_ONLY		(1 << 2)
#define CL_MEM_USE_HOST_PTR		(1 << 3)
#define CL_MEM_ALLOC_HOST_PTR		(1 << 4)
#define CL_MEM_COPY_HOST_PTR		(1 << 5)

/* cl_context_properties */
#define CL_CONTEXT_PLATFORM		0x1084

typedef cl_int (*clGetPlatformIDs_proc)(cl_uint, cl_platform_id*, cl_uint*);
typedef cl_int (*clGetDeviceIDs_proc)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);

typedef cl_context (*clCreateContext_proc)(const cl_context_properties*, cl_uint, const cl_device_id*, void (CL_CALLBACK*)(const char*, const void*, size_t, void*), void*, cl_int*);
typedef cl_int (*clReleaseContext_proc)(cl_context);

typedef cl_command_queue (*clCreateCommandQueueWithProperties_proc)(cl_context, cl_device_id, const cl_queue_properties*, cl_int*);
typedef cl_int (*clReleaseCommandQueue_proc)(cl_command_queue);

typedef cl_mem (*clCreateBuffer_proc)(cl_context, cl_mem_flags, size_t, void*, cl_int*);

typedef cl_int (*clEnqueueNativeKernel_proc)(cl_command_queue, void (CL_CALLBACK*)(void *), void*, size_t, cl_uint, const cl_mem*, const void**, cl_uint, const cl_event*, cl_event*);

typedef cl_int (*clWaitForEvents_proc)(cl_uint, const cl_event*);
/* OpenCL declarations */


typedef struct
{
   cl_context context;
   cl_command_queue queue;

} _aax_opencl_t;

int _aaxOpenCLDetect();
_aax_opencl_t* _aaxOpenCLCreate(); 
void _aaxOpenCLDestroy(_aax_opencl_t*);

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

void _aaxOpenCLRunConvolution(_aax_opencl_t*, _aaxRingBufferConvolutionData*, float*, unsigned int, unsigned int);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_GPU_H */

