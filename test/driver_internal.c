
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <strings.h>

#include <oss/device.h>
#include <alsa/device.h>
#include <alsasoft/device.h>
#include <dmedia/device.h>
#include <software/device.h>

#include "driver.h"

static const _aaxDriverBackend *_backends[] =
{
   &_aaxNoneDriverBackend,
   &_aaxLoopbackDriverBackend,
   &_aaxSoftwareDriverBackend,
   &_aaxOSSDriverBackend,
   &_aaxALSASoftDriverBackend,
   &_aaxALSADriverBackend,
   &_aaxDMediaDriverBackend
};

_aaxDriverBackend *
getDriverBackend(int argc, char **argv, char **renderer)
{
   const _aaxDriverBackend *be = 0;
   int i, slen = 0;
   char *dn;

   dn = getDeviceName(argc, argv);
   
   if (dn)
      slen = strlen(dn);

   i = 3;
   do
   {
      do
      {
         i--;
         be = _backends[i];
         if (i == 0)
         {
            dn = 0;
            break;
         }
      }
      while (be->detect(AAX_MODE_WRITE_STEREO) == 0);
      if (!dn) break;
   }
   while (strncasecmp(be->driver, dn, slen) != 0);

   *renderer = getRenderer(argc, argv);

   return (_aaxDriverBackend *)be;
}
