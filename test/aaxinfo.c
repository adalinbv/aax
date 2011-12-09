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

#include <stdio.h>
#ifndef NDEBUG
#include <rmalloc.h>
#endif
#include <aaxdefs.h>
#include "driver.h"

static const char *_filter_s[AAX_FILTER_MAX] =
{
   "_",
   "AAX_equalizer",
   "AAX_volume_filter",
   "AAX_tremolo_filter",
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
   "AAX_vibrato_effect",
   "AAX_timed_pitch_effect",
   "AAX_distortion_effect",
   "AAX_phasing_effect",
   "AAX_chorus_effect",
   "AAX_flanging_effect",
   "AAX_velocity_effect"
};

int main(int argc, char **argv)
{
   unsigned i, x, y, z, max;
   aaxConfig cfg;
   const char *s;
   char *devname;
   int mode;

   if (printCopyright(argc, argv))
      return 0;

   printf("Run %s -copyright to read the copyright information.\n\n", argv[0]);

   for (mode = AAX_MODE_READ; mode <= AAX_MODE_WRITE_STEREO; mode++)
   {
      char *desc[2] = { "capture", "playback"};

      printf("Devices that support %s:\n", desc[mode]);

      max = aaxDriverGetCount(mode);
      for (x=0; x<max; x++)
      {
         cfg = aaxDriverGetByPos(x, mode);
         if (cfg) {
            unsigned max_device;
            const char *d;

            d = aaxDriverGetSetup(cfg, AAX_NAME_STRING);
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
      printf("\nVersion: %s (%i.%i)\n", s, x, y);

      if (cfg)
      {
         int res =  aaxMixerInit(cfg);
         testForState(res, "aaxMixerInit");

         s = aaxDriverGetSetup(cfg, AAX_NAME_STRING);
         printf("Driver: %s\n", s);

         s = aaxDriverGetVendor(cfg);
         printf("Vendor: %s\n", s);

         s = aaxDriverGetRenderer(cfg);
         printf("Renderer: %s\n", s);

         x = aaxMixerGetFrequency(cfg);
         printf("Mixer frequency: %i Hz\n", x);

         x = aaxMixerGetRefreshRate(cfg);
         printf("Mixer refresh rate: %u Hz\n", x);

         x = aaxMixerGetSetup(cfg, AAX_MONO_EMITTERS);
         y = aaxMixerGetSetup(cfg, AAX_STEREO_EMITTERS);
         printf("Number of available  mono  emitters: %i\n", x);
         printf("Number of available stereo emitters: %i\n", y);
         x = aaxMixerGetSetup(cfg, AAX_AUDIO_FRAMES);
         printf("Number of available  audio  frames:  %i\n", x);

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
   else
   {
      printf("\nDefault driver not available.\n");

      x = aaxMixerGetNoMonoSources();
      y = aaxMixerGetNoStereoSources();
      printf("Number of available  mono  sources: %i\n", x);
      printf("Number of available stereo sources: %i\n", y);
   }
}
