/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#pragma once

#include <fcntl.h>              /* SEEK_*, O_* */
#include <base/xpoll.h>

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
   int flags;
   int min_rate, max_rate;
   int min_channels;
   int max_channels;
   int binding;
   int rate_source;
   char handle[32];
   unsigned int nrates;
   unsigned int rates[20];
   char song_name[64];
   char label[16];
   int latency;
   char devnode[32];
   int next_play_engine;
   int next_rec_engine;
   int filler[184];
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

#define	AFMT_U8				0x00000008
#define AFMT_S16_LE			0x00000010
#define AFMT_S16_BE			0x00000020
#define AFMT_S8				0x00000040

#define SOUND_MIXER_WRITE_VOLUME	0xc0044d00
#define SOUND_MIXER_READ_VOLUME		0x80044d00
#define SNDCTL_DSP_SETPLAYVOL		0xc0045018
#define SNDCTL_DSP_SETRECVOL		0xc0045029
#define SOUND_MIXER_WRITE_IGAIN		0xc0044d0c
#define SOUND_MIXER_READ_DEVMASK	0x80044dfe
#define SOUND_MIXER_WRITE_OGAIN		0xc0044d0d
#define SNDCTL_GETLABEL			0x80105904
#define SOUND_MIXER_READ_RECMASK	0x80044dfd
#define SNDCTL_DSP_GETRECVOL 		0x80045029
#define SNDCTL_DSP_GETPLAYVOL		0x80045018
#define SOUND_MIXER_READ_IGAIN		0x80044d0c
#define SOUND_MIXER_READ_OGAIN		0x80044d0d

#define SOUND_MIXER_NRDEVICES		28
#define SOUND_MIXER_VOLUME		0
#define SOUND_MIXER_BASS		1
#define SOUND_MIXER_TREBLE		2
#define SOUND_MIXER_SYNTH		3
#define SOUND_MIXER_PCM			4
#define SOUND_MIXER_SPEAKER		5
#define SOUND_MIXER_LINE		6
#define SOUND_MIXER_MIC			7
#define SOUND_MIXER_CD			8
#define SOUND_MIXER_IMIX		9	/*  Recording monitor  */
#define SOUND_MIXER_ALTPCM		10
#define SOUND_MIXER_RECLEV		11	/* Recording level */
#define SOUND_MIXER_IGAIN		12	/* Input gain */
#define SOUND_MIXER_OGAIN		13	/* Output gain */

#define SOUND_MASK_VOLUME		(1 << SOUND_MIXER_VOLUME)
#define SOUND_MASK_BASS			(1 << SOUND_MIXER_BASS)
#define SOUND_MASK_TREBLE		(1 << SOUND_MIXER_TREBLE)
#define SOUND_MASK_SYNTH		(1 << SOUND_MIXER_SYNTH)
#define SOUND_MASK_PCM			(1 << SOUND_MIXER_PCM)
#define SOUND_MASK_SPEAKER		(1 << SOUND_MIXER_SPEAKER)
#define SOUND_MASK_LINE			(1 << SOUND_MIXER_LINE)
#define SOUND_MASK_MIC			(1 << SOUND_MIXER_MIC)
#define SOUND_MASK_CD			(1 << SOUND_MIXER_CD)
#define SOUND_MASK_IMIX			(1 << SOUND_MIXER_IMIX)
#define SOUND_MASK_ALTPCM		(1 << SOUND_MIXER_ALTPCM)
#define SOUND_MASK_RECLEV		(1 << SOUND_MIXER_RECLEV)
#define SOUND_MASK_IGAIN		(1 << SOUND_MIXER_IGAIN)
#define SOUND_MASK_OGAIN		(1 << SOUND_MIXER_OGAIN)
#define SOUND_MASK_LINE1		(1 << SOUND_MIXER_LINE1)
#define SOUND_MASK_LINE2		(1 << SOUND_MIXER_LINE2)
#define SOUND_MASK_LINE3		(1 << SOUND_MIXER_LINE3)
#define SOUND_MASK_DIGITAL1		(1 << SOUND_MIXER_DIGITAL1)
#define SOUND_MASK_DIGITAL2		(1 << SOUND_MIXER_DIGITAL2)
#define SOUND_MASK_DIGITAL3		(1 << SOUND_MIXER_DIGITAL3)
#define SOUND_MASK_MONO			(1 << SOUND_MIXER_MONO)
#define SOUND_MASK_PHONE		(1 << SOUND_MIXER_PHONE)
#define SOUND_MASK_RADIO		(1 << SOUND_MIXER_RADIO)
#define SOUND_MASK_VIDEO		(1 << SOUND_MIXER_VIDEO)
#define SOUND_MASK_DEPTH		(1 << SOUND_MIXER_DEPTH)
#define SOUND_MASK_REARVOL		(1 << SOUND_MIXER_REARVOL)
#define SOUND_MASK_CENTERVOL		(1 << SOUND_MIXER_CENTERVOL)
#define SOUND_MASK_SIDEVOL		(1 << SOUND_MIXER_SIDEVOL)

#define SNDCTL_CARDINFO			0xc498580b
#define SNDCTL_AUDIOINFO		0xc49c5807
#define SNDCTL_AUDIOINFO_EX		0xc49c580d

#define SNDCTL_DSP_SPEED		0xc0045002
#define SNDCTL_DSP_GETBLKSIZE		0xc0045004
#define SNDCTL_DSP_SAMPLESIZE		0xc0045005
#define SNDCTL_DSP_SETFMT		0xc0045005
#define SNDCTL_DSP_CHANNELS		0xc0045006
#define SNDCTL_DSP_SETFRAGMENT		0xc004500a
// #define SNDCTL_DSP_COOKEDMODE	0xc004501e
#define SNDCTL_DSP_COOKEDMODE		0x4004501e

#define OSS_GETVERSION			0x80044d76
#define SNDCTL_DSP_GETOSPACE		0x8010500c
#define SNDCTL_DSP_GETISPACE		0x8010500d
#define SNDCTL_DSP_GETODELAY		0x80045017
#define SNDCTL_DSP_GETERROR		0x80685019

#define PCM_CAP_INPUT			0x00010000
#define PCM_CAP_OUTPUT			0x00020000
#define PCM_CAP_VIRTUAL			0x00040000

#define DSP_CH_MASK			0x06000000
#define DSP_CH_ANY			0x00000000
#define DSP_CH_MONO			0x02000000
#define DSP_CH_STEREO			0x04000000
#define DSP_CH_MULTI			0x06000000

#define SNDCTL_SYSINFO			0x84e05801

#define OSS_LABEL_SIZE			16
typedef char oss_label_t[OSS_LABEL_SIZE];

typedef int (*ioctl_proc)(int, int, void*);
typedef int (*poll_proc)(struct pollfd[], nfds_t, int);
