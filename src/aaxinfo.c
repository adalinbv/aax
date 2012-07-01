/* -*- mode: C; tab-width:8; c-basic-offset:8 -*-
 * vi:set ts=8:
 *
 * aaxinfo.c
 *
 * aaxinfo display info about a ALC extension and OpenAL renderer
 *
 * This file is in the Public Domain and comes with no warranty.
 * Erik Hofman <erik@ehofman.com>
 *
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#include <stdio.h>
#ifndef NDEBUG
# ifdef HAVE_RMALLOC_H
#  include <rmalloc.h>
# endif
#endif
#include <aaxdefs.h>
#include "wavfile.h"
#include "driver.h"

static const char *_filter_s[AAX_FILTER_MAX] =
{
   "_",
   "AAX_equalizer",
   "AAX_volume_filter",
   "AAX_dynamic_gain_filter",
   "AAX_timed_gain_filter",
   "AAX_angular_filter",
   "AAX_distance_filter",
   "AAX_frequency_filter",
   "AAX_graphic_equalizer"
};

static const char *_effect_s[AAX_EFFECT_MAX] =
{
   "_",
   "AAX_pitch_effect",
   "AAX_dynamic_pitch_effect",
   "AAX_timed_pitch_effect",
   "AAX_distortion_effect",
   "AAX_phasing_effect",
   "AAX_chorus_effect",
   "AAX_flanging_effect",
   "AAX_velocity_effect"
};

int main(int argc, char **argv)
{
   unsigned int i, x, y, z, max;
   aaxConfig cfg;
   const char *s;
   char *devname;
   int mode;

   if (printCopyright(argc, argv) || playAudioTune(argc, argv)) {
      return 0;
   }

   printf("Run %s -copyright to read the copyright information.\n", argv[0]);

   for (mode = AAX_MODE_READ; mode <= AAX_MODE_WRITE_STEREO; mode++)
   {
      char *desc[2] = { "capture", "playback"};

      printf("\nDevices that support %s:\n", desc[mode]);

      max = aaxDriverGetCount(mode);
      for (x=0; x<max; x++)
      {
         cfg = aaxDriverGetByPos(x, mode);
         if (cfg) {
            unsigned max_device;
            const char *d;

            d = aaxDriverGetSetup(cfg, AAX_DRIVER_STRING);
            max_device = aaxDriverGetDeviceCount(cfg, mode);
            if (max_device)
            {
               for (y=0; y<max_device; y++)
               {
                  unsigned int max_interface;
                  const char *r;

                  r = aaxDriverGetDeviceNameByPos(cfg, y, mode);

                  max_interface = aaxDriverGetInterfaceCount(cfg, r, mode);
                  if (max_interface)
                  {
                     for (z=0; z<max_interface; z++)
                     {
                        const char *ifs;

                        ifs = aaxDriverGetInterfaceNameByPos(cfg, r, z, mode);
                        printf(" '%s on %s: %s'\n", d, r, ifs);
                     }
                  }
                  else {
                     printf(" '%s on %s'\n", d, r);
                  }
               }
            } else {
               printf(" '%s'\n", d);
            }
            aaxDriverDestroy(cfg);
         } else {
            printf("\t%i. not found\n", x);
         }
      }
   }

   mode = AAX_MODE_WRITE_STEREO;
   devname = getDeviceName(argc, argv);
   cfg = aaxDriverGetByName(devname, mode);
   if (cfg)
   {
#if 0
      s = aaxDriverGetSetup(cfg, AAX_NAME_STRING);
      printf("Default driver: %s\n", s);
#endif

      cfg = aaxDriverOpen(cfg);
      x = aaxGetMajorVersion();
      y = aaxGetMinorVersion();
      s = (char *)aaxGetVersionString(cfg);
      printf("\nVersion : %s (%i.%i)\n", s, x, y);

      if (cfg)
      {
         int res =  aaxMixerInit(cfg);
         testForState(res, "aaxMixerInit");

         s = aaxDriverGetVendor(cfg);
         printf("Vendor  : %s\n", s);

         s = aaxDriverGetDriver(cfg);
         printf("Driver  : %s\n", s);

         s = aaxDriverGetRenderer(cfg);
         printf("Renderer: %s\n", s);

         x = aaxMixerGetFrequency(cfg);
         printf("Mixer frequency: %i Hz\n", x);

         x = aaxMixerGetRefreshRate(cfg);
         printf("Mixer refresh rate: %u Hz\n", x);

         x = aaxMixerGetSetup(cfg, AAX_MONO_EMITTERS);
         y = aaxMixerGetSetup(cfg, AAX_STEREO_EMITTERS);
         printf("Available mono emitters:   ");
         if (x == UINT_MAX) printf("infinite\n");
         else printf("%3i\n", x);
         printf("Available stereo emitters: ");
         if (y == UINT_MAX/2) printf("infinite\n");
         else printf("%3i\n", y);
         x = aaxMixerGetSetup(cfg, AAX_AUDIO_FRAMES);
         printf("Available audio-frames: ");
         if (x == UINT_MAX) printf("   infinite\n");
         else printf("%6i\n", x);
      }

      printf("\nSupported Filters:\n ");
      for (i=1; i<AAX_FILTER_MAX; i++)
      {
         static int len = 1;
         if (aaxIsFilterSupported(cfg, _filter_s[i]))
         {
            len += strlen(_filter_s[i])+1;	/* one for leading space */
               if (len >= 78)
            {
               printf("\n ");
               len = strlen(_filter_s[i])+1;
            }
            printf(" %s", _filter_s[i]);
         }
      }

      printf("\n\nSupported Effects:\n ");
      for (i=1; i<AAX_EFFECT_MAX; i++)
      {
         static int len = 1;
         if (aaxIsEffectSupported(cfg, _effect_s[i]))
         {
            len += strlen(_effect_s[i])+1;	/* one for leading space */
            if (len >= 78)
            {
               printf("\n ");
               len = strlen(_effect_s[i])+1;
            }
            printf(" %s", _effect_s[i]);
         }
      }
      printf("\n\n");

      if (cfg)
      {
         aaxDriverClose(cfg);
         aaxDriverDestroy(cfg);
      }
   }
   else {
      printf("\nDefault driver not available.\n\n");
   }

   return 0;
}
