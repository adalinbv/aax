#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <base/memory.h>

#define PRINTFP80	1

int main()
{
   long double ld = 44100.0009;
   __float80 nfp80, fp80 = ld;
   size_t size;
   uint8_t *ch;
   uint64_t i;
   double d;

   printf("original:    %Lf\n", ld);

#if PRINTFP80
   d = ld;
   memcpy(&i, &d, sizeof(d));
   printf(" double: %8" PRIx64 " - ", i);
   printf("  fp80: ");
   ch = (uint8_t*)&fp80;
   for (i=0; i<10; ++i) {
      printf("%02x", ch[i]);
   }
   printf("\n");
#endif

   // convert fp80 to double
   size = sizeof(fp80);
   ch = (uint8_t*)&fp80;
   d = readfp80le(&ch, &size);
#if !PRINTFP80
   printf("readfp80le:  %lf\n", d);
#endif

   // convert double to fp80
   nfp80 = 0;
   size = sizeof(nfp80);
   ch = (uint8_t*)&nfp80;
   writefp80le(&ch, ld, &size);
   
#if PRINTFP80
   memcpy(&i, &d, sizeof(d));
   printf(" double: %8" PRIx64 " - ", i);
   printf(" nfp80: ");
   ch = (uint8_t*)&nfp80;
   for (i=0; i<10; ++i) {
      printf("%02x", ch[i]);
   }
   printf("\n");
#endif

   // convert nfp80 back to double
   size = sizeof(nfp80);
   ch = (uint8_t*)&nfp80;
   d = readfp80le(&ch, &size);
#if PRINTFP80
   memcpy(&i, &d, sizeof(d));
   printf(" double: %8" PRIx64 " - ", i);
#endif
   printf("writefp80le: %lf\n", d);

   (void)i;
}
