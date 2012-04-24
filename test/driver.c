
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>	/* nanosleep */
#include <string.h>	/* strrchr */

#include <base/logging.h>

#include "driver.h"
#include "copyright.h"

enum
{
   NULL_DEVICE=0,
   SOFTWARE_DEVICE,
   OSS_DEVICE,
   ALSASOFT_DEVICE,
   ALSA_DEVICE,
   DMEDIA_DEVICE,
   MAX_DEVICES
};

char *
getDeviceName(int argc, char **argv)
{
   static char devname[255];
   int len = 255;
   char *s;

   s = getCommandLineOption(argc, argv, "-d");
   if (s)
   {
      strncpy((char *)&devname, s, len);
      len -= strlen(s);

      s = getCommandLineOption(argc, argv, "-r");
      if (s)
      {
         strncat((char *)&devname, " on ", len);
         len -= 4;

         strncat((char *)&devname, s, len);
      }
      s = (char *)&devname;
   }

   return s;
}

char *
getCaptureName(int argc, char **argv)
{
   static char devname[255];
   int len = 255;
   char *s;

   s = getCommandLineOption(argc, argv, "-c");
   if (s)
   {
      strncpy((char *)&devname, s, len);
      len -= strlen(s);
   }

   return s;
}

char *
getRenderer(int argc, char **argv)
{
   char *renderer = 0;
   renderer = getCommandLineOption(argc, argv, "-r");
   return renderer;
}

int
getNumSources(int argc, char **argv)
{
   int num = 1;
   char *ret = getCommandLineOption(argc, argv, "-n");
   if (ret) num = atoi(ret);
   return num;
}

float
getPitch(int argc, char **argv)
{
   float num = 1.0;
   char *ret = getCommandLineOption(argc, argv, "-p");
   if (ret) num = atof(ret);
   return num;
}

float
getGain(int argc, char **argv)
{
   float num = 1.0;
   char *ret = getCommandLineOption(argc, argv, "-g");
   if (ret) num = atof(ret);
   return num;
}

int
printCopyright(int argc, char **argv)
{
   char *ret = getCommandLineOption(argc, argv, "-c");
   if (ret) {
      printf("%s\n", ___COPYING);
   }
   return ret ? -1 : 0;
}

int
getMode(int argc, char **argv)
{
   int mode = AAX_MODE_WRITE_STEREO;
   char *ret = getCommandLineOption(argc, argv, "-m");
   if (ret)
   {
      if (!strcasecmp(ret, "hrtf")) mode = AAX_MODE_WRITE_HRTF;
      else if (!strcasecmp(ret, "spatial")) mode = AAX_MODE_WRITE_SPATIAL;
      else if (!strcasecmp(ret, "surround")) mode = AAX_MODE_WRITE_SURROUND;
   }
   return mode;
}

char *
getInputFile(int argc, char **argv, const char *filename)
{
   char *fn;

   fn = getCommandLineOption(argc, argv, "-i");
   if (!fn) fn = (char *)filename;

   return fn;
}

void
testForError(void *p, char *s)
{
   if (p == NULL)
   {
      int err = aaxGetErrorNo();
      printf("\nError: %s\n", s);
      if (err) {
         printf("%s\n\n", aaxGetErrorString(err));
      }
      exit(-1);
   }
}

void
testForState(int res, const char *func)
{
   if (res != AAX_TRUE)
   {
      int err = aaxGetErrorNo();
      printf("%s:\t\t%i\n", func, res);
      printf("(%i) %s\n\n", err, aaxGetErrorString(err));
      exit(-1);
   }
}

char *
getCommandLineOption(int argc, char **argv, char *option)
{
   int slen = strlen(option);
   char *rv = 0;
   int i;
   
   for (i=0; i<argc; i++)
   {
      if (strncmp(argv[i], option, slen) == 0)
      {
         rv = "";
         rv = "";
         i++;
         if (i<argc) rv = argv[i];
      }
   }

   return rv;
}

void nanoSleep(unsigned long st)
{
   struct timespec sleept;

   sleept.tv_sec = 0;
   sleept.tv_nsec = st;
   nanosleep(&sleept, 0);
}

char *strDup(const char *s)
{
   unsigned int len = strlen(s)+1;
   char *p = malloc(len);
   if (p) memcpy(p, s,len);
   return p;
}
