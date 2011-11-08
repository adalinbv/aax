
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#define BITS		11
#define MAXBITS24	23
#define MAXBITS32	31
#define SHIFTBITS	(MAXBITS32-BITS)
#define MAXSHIFT	(uint64_t)(1<<MAXBITS32)
#define NUM		(1<<BITS)
#define MAX		((1<<SHIFTBITS)-1)
#define MAXPOS		((MAXSHIFT-1)>>BITS)
#define FNUM		(double)NUM

double tbl[NUM];

int main()
{
   uint32_t start = (1<<SHIFTBITS)-1;
   uint32_t end = (uint32_t)(MAXSHIFT-1);
   double fact = 1.0/FNUM;
   double fstart;
   int32_t q;

   printf("max int32_t: 0x7FFFFFFF\n");
   printf("MAXBITS32:   0x%08X\n", (uint32_t)(1<<MAXBITS32)-1);
   printf("MAXBITS24:   0x%08X\n", (1<<MAXBITS24)-1);
   printf("SHIFTBITS:   %i\n", SHIFTBITS);
   printf("DIFFBITS:    %i\n", MAXBITS24-(MAXBITS32-BITS));

   fstart = (double)start;
   for(q=start; q<end; q += start)
   {
      unsigned int pos = q>>SHIFTBITS;
      double v, f;

      f = log((double)(pos+1))/log(2.0);
      f = f/(double)(BITS);
      f = pow(f, 1.375);
      f = f * 8.0; // (double)(MAXBITS24-(MAXBITS32-BITS));
      f = pow(2.0, f);
      tbl[pos] = 1.0 / f;
   }

   printf("static float _compress_tbl[%i] = {\n   ", NUM);
   for (q=0; q<NUM; q++)
   {
      static int c = 0;

      printf("%9.8f, ", tbl[q]);
      if (++c > 5)
      {
         printf("\n   ");
         c = 0;
      }
   }
   printf("\n};\n");

   printf("\n\nstart: %i (%i)\n", start, q>>SHIFTBITS);
   printf("end: %i, max: %i\n", end, (1<<MAXBITS24)-1);
   printf("\n   ");
   for(q=start; q < end; q += start)
   {
      double max = (double)((1<<MAXBITS24)-1);
      double min = -max;
      unsigned int i, pos = q>>SHIFTBITS;
      static int c = 0;
      double val;

      val = (double)q * tbl[pos];

#if 0
      printf("%9.1f, ", val);
      if (++c > 5)
      {
         printf("\n   ");
         c = 0;
      }
#endif

      if (val < min || val > max)
      {
         printf("\ntoo large:\n");
         printf("  q:   %i (%08x), tbl[%i (%08x)]: %f\n", q, q, pos, pos, tbl[pos]);
         printf("  val: %f (%i / %08x)\n", val, q, q);
         printf("  max: %f (%i / %08x)\n", max, (int32_t)max, (int32_t)max);
         exit(-1);
      }
   }
   printf("\nend: %i, max: %i\n", end, (1<<MAXBITS24)-1);

   return 0;
}
