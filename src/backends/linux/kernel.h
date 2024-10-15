/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _ALSA_KERNEL_H
#define _ALSA_KERNEL_H 1

#include <fcntl.h>		/* SEEK_*, O_* */
#include <base/xpoll.h>

# ifndef __force
#  define __force
# endif
# ifndef __bitwise
#  define __bitwise
# endif
# ifndef __user
#  define __user
# endif
#if HAVE_LINUX_TYPES_H
# include <linux/types.h>
#else
typedef uint32_t __u32;
#endif

#ifdef WIN32
#define MAP_FILE	0
#define MAP_FAILED	((void*)-1)
#define PROT_READ	0x1
#define PROT_WRITE	0x2
#define MAP_SHARED	0x1
void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
int munmap(void *addr, size_t len);
#else
# include <sys/mman.h>
#endif

#include "alsa.h"

typedef int (*ioctl_proc)(int, unsigned long, ...);
typedef int (*poll_proc)(struct pollfd[], nfds_t, int);
typedef void* (*mmap_proc)(void*, size_t, int, int, int, off_t);
typedef int (*munmap_proc)(void*, size_t);

/*
 * all from the alsa documentation
 * http://fossies.org/dox/alsa-lib-1.0.28/modules.html
 */

#define SNDRV_PCM_HW_PARAM_ACCESS		0  
#define SNDRV_PCM_HW_PARAM_FORMAT		1  
#define SNDRV_PCM_HW_PARAM_SUBFORMAT		2  
#define SNDRV_PCM_HW_PARAM_FIRST_MASK SNDRV_PCM_HW_PARAM_ACCESS
#define SNDRV_PCM_HW_PARAM_LAST_MASK SNDRV_PCM_HW_PARAM_SUBFORMAT

typedef int __bitwise snd_pcm_subformat_t;
#define SNDRV_PCM_SUBFORMAT_STD ((__force snd_pcm_subformat_t)0)
#define SNDRV_PCM_SUBFORMAT_LAST SNDRV_PCM_SUBFORMAT_STD

#define SNDRV_PCM_HW_PARAM_SAMPLE_BITS		8
#define SNDRV_PCM_HW_PARAM_FRAME_BITS		9
#define SNDRV_PCM_HW_PARAM_CHANNELS		10
#define SNDRV_PCM_HW_PARAM_RATE			11
#define SNDRV_PCM_HW_PARAM_PERIOD_SIZE		13
#define SNDRV_PCM_HW_PARAM_PERIOD_BYTES		14
#define SNDRV_PCM_HW_PARAM_PERIODS		15
#define SNDRV_PCM_HW_PARAM_TICK_TIME		19
#define SNDRV_PCM_HW_PARAM_FIRST_INTERVAL SNDRV_PCM_HW_PARAM_SAMPLE_BITS
#define SNDRV_PCM_HW_PARAM_LAST_INTERVAL SNDRV_PCM_HW_PARAM_TICK_TIME

#define SNDRV_PCM_INFO_MMAP			0x00000001  
#define SNDRV_PCM_INFO_MMAP_VALID		0x00000002  
#define SNDRV_PCM_INFO_INTERLEAVED		0x00000100  
#define SNDRV_PCM_INFO_NONINTERLEAVED		0x00000200  
#define SNDRV_PCM_INFO_RESUME			0x00040000  
#define SNDRV_PCM_INFO_PAUSE			0x00080000  
#define SNDRV_PCM_INFO_SYNC_START		0x00400000  
#define SNDRV_PCM_INFO_NO_PERIOD_WAKEUP		0x00800000  

#define SNDRV_PCM_HW_PARAMS_NORESAMPLE		(1<<0)  
#define SNDRV_PCM_HW_PARAMS_EXPORT_BUFFER	(1<<1) 
#define SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP	(1<<2)


enum {
   SNDRV_PCM_TSTAMP_NONE = 0,
   SNDRV_PCM_TSTAMP_ENABLE,
   SNDRV_PCM_TSTAMP_LAST = SNDRV_PCM_TSTAMP_ENABLE,
};

#define SNDRV_PCM_MMAP_OFFSET_DATA    0x00000000
#define SNDRV_PCM_MMAP_OFFSET_STATUS  0x80000000
#define SNDRV_PCM_MMAP_OFFSET_CONTROL 0x81000000

struct snd_ctl_card_info {
   int card;
   int pad;
   unsigned char id[16];
   unsigned char driver[16];
   unsigned char name[32];
   unsigned char longname[80];
   unsigned char reserved_[16];
   unsigned char mixername[80];
   unsigned char components[128];
};

#define SNDRV_CTL_IOCTL_CARD_INFO _IOR('U', 0x01, struct snd_ctl_card_info)
#define SNDRV_PCM_IOCTL_DELAY _IOR('A', 0x21, snd_pcm_sframes_t)
#define SNDRV_PCM_IOCTL_HWSYNC _IO('A', 0x22)
#define SNDRV_PCM_IOCTL_SYNC_PTR _IOWR('A', 0x23, struct snd_pcm_sync_ptr)


union snd_pcm_sync_id {
   unsigned char id[16];
   unsigned short id16[8];
   unsigned int id32[4];
};

struct snd_pcm_info {
   unsigned int device;
   unsigned int subdevice;
   int stream;
   int card;
   unsigned char id[64];
   unsigned char name[80];
   unsigned char subname[32];
   int dev_class;
   int dev_subclass;
   unsigned int subdevices_count;
   unsigned int subdevices_avail;
   union snd_pcm_sync_id sync;
   unsigned char reserved[64];
};

#define SNDRV_PCM_IOCTL_INFO _IOR('A', 0x01, struct snd_pcm_info)


struct snd_interval {
   unsigned int min, max;
   unsigned int openmin:1,
   openmax:1,
   integer:1,
   empty:1;
};

#define SNDRV_MASK_MAX 256
struct snd_mask {
   __u32 bits[(SNDRV_MASK_MAX+31)/32];
};

struct snd_pcm_hw_params {
   unsigned int flags;
   struct snd_mask masks[SNDRV_PCM_HW_PARAM_LAST_MASK -
   SNDRV_PCM_HW_PARAM_FIRST_MASK + 1];
   struct snd_mask mres[5];
   struct snd_interval intervals[SNDRV_PCM_HW_PARAM_LAST_INTERVAL -
   SNDRV_PCM_HW_PARAM_FIRST_INTERVAL + 1];
   struct snd_interval ires[9];
   unsigned int rmask;
   unsigned int cmask;
   unsigned int info;
   unsigned int msbits;
   unsigned int rate_num;
   unsigned int rate_den;
   snd_pcm_uframes_t fifo_size;
   unsigned char reserved[64];
};

#define SNDRV_PCM_IOCTL_HW_REFINE _IOWR('A', 0x10, struct snd_pcm_hw_params)
#define SNDRV_PCM_IOCTL_HW_PARAMS _IOWR('A', 0x11, struct snd_pcm_hw_params)


struct snd_pcm_sw_params {
   int tstamp_mode;
   unsigned int period_step;
   unsigned int sleep_min;
   snd_pcm_uframes_t avail_min;
   snd_pcm_uframes_t xfer_align;
   snd_pcm_uframes_t start_threshold;
   snd_pcm_uframes_t stop_threshold;
   snd_pcm_uframes_t silence_threshold;
   snd_pcm_uframes_t silence_size;
   snd_pcm_uframes_t boundary;
   unsigned char reserved[64];
};

#define SNDRV_PCM_IOCTL_SW_PARAMS _IOWR('A', 0x13, struct snd_pcm_sw_params)


struct snd_pcm_mmap_status {
   snd_pcm_state_t state;
   int pad1;
   snd_pcm_uframes_t hw_ptr;
   struct timespec tstamp;
   snd_pcm_state_t suspended_state;
};

struct snd_pcm_mmap_control {
   snd_pcm_uframes_t appl_ptr;
   snd_pcm_uframes_t avail_min;
};

#define SNDRV_PCM_SYNC_PTR_HWSYNC	(1<<0)  
#define SNDRV_PCM_SYNC_PTR_APPL		(1<<1)  
#define SNDRV_PCM_SYNC_PTR_AVAIL_MIN	(1<<2) 

struct snd_pcm_sync_ptr {
   unsigned int flags;
   union {
      struct snd_pcm_mmap_status status;
      unsigned char reserved[64];
   } s;
   union {
      struct snd_pcm_mmap_control control;
      unsigned char reserved[64];
   } c;
};

#define SNDRV_PCM_IOCTL_SYNC_PTR _IOWR('A', 0x23, struct snd_pcm_sync_ptr)


struct snd_xferi {
   snd_pcm_sframes_t result;
   void __user* buf;
   snd_pcm_uframes_t frames;
};

struct snd_xfern {
   snd_pcm_sframes_t result;
   void __user * __user* bufs;
   snd_pcm_uframes_t frames;
};

#define SNDRV_PCM_IOCTL_PREPARE _IO('A', 0x40)
#define SNDRV_PCM_IOCTL_RESET _IO('A', 0x41)
#define SNDRV_PCM_IOCTL_START _IO('A', 0x42)
#define SNDRV_PCM_IOCTL_DROP _IO('A', 0x43)
#define SNDRV_PCM_IOCTL_PAUSE _IOW('A', 0x45, int)
#define SNDRV_PCM_IOCTL_REWIND _IOW('A', 0x46, snd_pcm_uframes_t)
#define SNDRV_PCM_IOCTL_RESUME _IO('A', 0x47)
#define SNDRV_CTL_IOCTL_POWER _IOWR('U', 0xd0, int)
#define SNDRV_CTL_IOCTL_POWER_STATE _IOR('U', 0xd1, int)
#define SNDRV_PCM_IOCTL_XRUN _IO('A', 0x48)
#define SNDRV_PCM_IOCTL_WRITEI_FRAMES _IOW('A', 0x50, struct snd_xferi)
#define SNDRV_PCM_IOCTL_READI_FRAMES _IOR('A', 0x51, struct snd_xferi)
#define SNDRV_PCM_IOCTL_WRITEN_FRAMES _IOW('A', 0x52, struct snd_xfern)
#define SNDRV_PCM_IOCTL_READN_FRAMES _IOR('A', 0x53, struct snd_xfern)

#endif /* _ALSA_KERNEL_H */

