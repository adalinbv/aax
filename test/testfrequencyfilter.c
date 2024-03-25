
#include <stdio.h>
#include <stdlib.h>

#include <src/software/cpu/arch2d_simd.h>
#include <src/dsp/dsp.h>

#define MAXNUM          1024

extern _batch_freqfilter_float_proc _batch_freqfilter_float;
_batch_freqfilter_float_proc batch_freqfilter_float;

#define DIFF(a,b)       ((fabsf(a)-fabsf(b))/fabsf(a))
#define TESTFN(a,d1,d2,m) do { double max = 0.0f; \
   for (i=0; i<MAXNUM; ++i) { double diff = DIFF(d1[i], d2[i]); \
       if (diff > max) { max = diff; } } \
   if (max > 1e-4f) printf("\t| max error <= %3.2f%%\n", max*100.0f); \
   else if (max > 0) printf("\t| max error < 0.01%%\n"); else printf("\n"); \
} while(0)
#define TESTF(a,d1,d2)  TESTFN(a,d1,d2,4.0f)

int main()
{
   _aaxRingBufferFreqFilterHistoryData history;
   _aaxRingBufferFreqFilterData flt;
   float *src, *dst1, *dst2;
   _data_t *buf;
   int i;

   buf = _aaxDataCreate(3, MAXNUM*sizeof(double)/sizeof(float), sizeof(float));
   if (!buf) return -1;

   src = (float*)_aaxDataGetData(buf, 0);
   dst1 = (float*)_aaxDataGetData(buf, 1);
   dst2 = (float*)_aaxDataGetData(buf, 2);

   for (i=0; i<MAXNUM; ++i) {
      src[i] = (double)(1<<23) * (double)rand()/(double)(RAND_MAX);
   }
   memset(dst1, 0, MAXNUM*sizeof(double));
   memcpy(dst2, src, MAXNUM*sizeof(float));

   memset(&flt, 0, sizeof(_aaxRingBufferFreqFilterData));
   flt.freqfilter = &history;
   flt.fs = 44100.0f;
   flt.run = _freqfilter_run;
   flt.high_gain = 1.0f;
   flt.low_gain = 0.0f;
   flt.no_stages = 1;
   flt.Q = 1.0f;
   flt.type = LOWPASS;
   flt.state = AAX_BUTTERWORTH;
   _aax_butterworth_compute(2200.0f, &flt);

   batch_freqfilter_float = _batch_freqfilter_float_cpu;

   // test whether filtering into a new buffer
   memset(&history, 0, sizeof(history));
   batch_freqfilter_float(dst1, src, 0, MAXNUM, &flt);

   // is different from filtering to the same buffer
   memset(&history, 0, sizeof(history));
   batch_freqfilter_float(dst2, dst2, 0, MAXNUM, &flt);
   TESTF("freqfilter ", dst1, dst2);

   _aaxDataDestroy(buf);

   return 0;
}
