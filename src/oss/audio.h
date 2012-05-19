/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef __OSS_AUDIO_H
#define __OSS_AUDIO_H 1

#if HAVE_CONFIG_H
#include "config.h"
#endif

typedef struct oss_sysinfo
{
   char product[32];
   char version[32];
   int versionnum;
   char options[128];
   int numaudios;
   int openedaudio[8];
   int numsynths;
   int nummidis;
   int numtimers;
   int nummixers;
   int openedmidi[8];
   int numcards;
   int numaudioengines;
   char license[16];
   int filler[236];
} oss_sysinfo;

typedef struct oss_audioinfo
{
   int dev;
   char name[64];
   int busy;
   int pid;
   int caps;
   int iformats, oformats;
   int magic;
   char cmd[64];
   int card_number;
   int port_number;
   int mixer_dev;
   int legacy_device;
   int enabled;
   int filler[251];
} oss_audioinfo;

typedef struct audio_buf_info
{
   int fragments;
   int fragstotal;
   int fragsize;
   int bytes;
} audio_buf_info;

typedef struct audio_errinfo
{
   int play_underruns;
   int rec_overruns;
   unsigned int play_ptradjust;
   unsigned int rec_ptradjust;
   int play_errorcount;
   int rec_errorcount;
   int play_lasterror;
   int rec_lasterror;
   int play_errorparm;
   int rec_errorparm;
   int filler[16];
} audio_errinfo;

typedef struct oss_card_info
{
   int card;
   char shortname[16];
   char longname[128];
   int flags;
   char hw_info[400];
   int intr_count;/* reserved */
   int ack_count;/* reserved */
   int filler[154];
} oss_card_info;

#define	AFMT_U8			0x00000008
#define AFMT_S16_LE		0x00000010
#define AFMT_S16_BE		0x00000020
#define AFMT_S8			0x00000040

#define SNDCTL_CARDINFO		0xc498580b
#define SNDCTL_AUDIOINFO	0xc49c5807
#define SNDCTL_AUDIOINFO_EX	0xc49c580d

#define SNDCTL_DSP_SPEED	0xc0045002
#define SNDCTL_DSP_GETBLKSIZE	0xc0045004
#define SNDCTL_DSP_SAMPLESIZE	0xc0045005
#define SNDCTL_DSP_SETFMT	0xc0045005
#define SNDCTL_DSP_CHANNELS	0xc0045006
#define SNDCTL_DSP_SETFRAGMENT	0xc004500a
// #define SNDCTL_DSP_COOKEDMODE	0xc004501e
#define SNDCTL_DSP_COOKEDMODE	0x4004501e

#define OSS_GETVERSION		0x80044d76
#define SNDCTL_DSP_GETOSPACE	0x8010500c
#define SNDCTL_DSP_GETERROR	0x80685019

#define PCM_CAP_INPUT		0x00010000
#define PCM_CAP_OUTPUT		0x00020000

#define SNDCTL_SYSINFO		0x84e05801

typedef int (*ioctl_proc)(int, int, void*);

#endif /* __OSS_AUDIO_H */

