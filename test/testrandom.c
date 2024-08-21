
#include <stdio.h>
#include <base/random.h>

int main()
{
   double avg;
   float min, max;
   int i;

   _aax_srandom();

   avg = 0.0;
   min = max = 0.0f;
   for (i=0; i<2048*2048; ++i) {
      float r = _aax_rand_sample();
      if (r > max) max = r;
      else if (r < min) min = r;
      avg += r;
   }
   avg /= (2048.0*2048.0);
   printf("_aax_rand_sample\n");
   printf("min: %f, max: %f, ag: %f\n\n", min, max, avg);

   avg = 0.0;
   min = max = 0.0f;
   for (i=0; i<2048*2048; ++i) {
      float r = _aax_random();
      if (r > max) max = r;
      else if (r < min) min = r;
      avg += r;
   }
   avg /= (2048.0*2048.0);
   printf("_aax_random\n");
   printf("min: %f, max: %f, ag: %f\n\n", min, max, avg);

   avg = 0.0;
   min = max = 0.0f;
   _aax_srand(xoroshiro128plus());
   for (i=0; i<2048*2048; ++i) {
      float r = (float)((double)_aax_rand()/(double)INT64_MAX) - 1.0f;
      if (r > max) max = r;
      else if (r < min) min = r;
      avg += r;
   }
   avg /= (2048.0*2048.0);
   printf("_aax_rand\n");
   printf("min: %f, max: %f, ag: %f\n\n", min, max, avg);

   return 0;
}
