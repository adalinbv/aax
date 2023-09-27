
#include <stdio.h>
#include <stdlib.h>
#include <dsp/effects.h>

#define TRY(a,b) if (fabsf((a)-(b)) > 1e-6f) { printf("Line %i: expected: %g, but got %g\n", __LINE__, (a), (b)); exit(-1); }

int main()
{
   _eff_function_tbl *phasing = &_aaxPhasingEffect;
   _eff_function_tbl *chorus = &_aaxChorusEffect;
   _eff_function_tbl *delay = &_aaxDelayLineEffect;
   float rv;

   /* phasing - set */
   rv = phasing->set(10.0f, AAX_MICROSECONDS, AAX_LFO_DEPTH);
   TRY(10e-6f, rv);

   rv = phasing->set(10.0f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10e-3f, rv);

   rv = phasing->set(0.0f, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(PHASING_MIN, rv);

   rv = phasing->set(1.0f, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(PHASING_DEPTH, rv);

   /* phasing - get */
   rv = phasing->get(10e-6f, AAX_MICROSECONDS, AAX_LFO_OFFSET);
   TRY(10.0f, rv);

   rv = phasing->get(10e-3f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10.0f, rv);

   rv = phasing->get(PHASING_MIN, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(0.0f, rv);
   
   rv = phasing->get(PHASING_DEPTH, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(1.0f, rv);

   /* chorus - set */
   rv = chorus->set(10.0f, AAX_MICROSECONDS, AAX_LFO_DEPTH);
   TRY(10e-6f, rv);

   rv = chorus->set(10.0f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10e-3f, rv);

   rv = chorus->set(0.0f, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(CHORUS_MIN, rv);

   rv = chorus->set(1.0f, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(CHORUS_DEPTH/CHORUS_NORM_FACT, rv);

   /* chorus - get */
   rv = chorus->get(10e-6f, AAX_MICROSECONDS, AAX_LFO_OFFSET);
   TRY(10.0f, rv);

   rv = chorus->get(10e-3f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10.0f, rv);

   rv = chorus->get(CHORUS_MIN, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(0.0f, rv);

   rv = chorus->get(CHORUS_DEPTH/CHORUS_NORM_FACT, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(1.0f, rv);

   /* delay */
   rv = delay->set(100.0f, AAX_MICROSECONDS, AAX_LFO_DEPTH);
   TRY(100e-6f, rv);

   rv = delay->set(10.0f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10e-3f, rv);

   rv = delay->set(0.0f, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(DELAY_MIN, rv);

   rv = delay->set(1.0f, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(DELAY_DEPTH/DELAY_NORM_FACT, rv);

   /* delay - get */
   rv = delay->get(10e-6f, AAX_MICROSECONDS, AAX_LFO_OFFSET);
   TRY(10.0f, rv);

   rv = delay->get(10e-3f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10.0f, rv);

   rv = delay->get(DELAY_MIN, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(0.0f, rv);
   
   rv = delay->get(DELAY_DEPTH/DELAY_NORM_FACT, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(1.0f, rv);

   return 0;
}
