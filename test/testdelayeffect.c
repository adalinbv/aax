
#include <stdio.h>
#include <stdlib.h>
#include <dsp/effects.h>

#define TRY(a,b) if (fabsf((a)-(b)) > 1e-6f) { printf("Line %i: expected: %g, but got %g (diff: %g)\n", __LINE__, (a), (b), (a)-(b)); exit(-1); }

int main()
{
   _eff_function_tbl *phasing = &_aaxPhasingEffect;
   _eff_function_tbl *chorus = &_aaxChorusEffect;
   _eff_function_tbl *delay = &_aaxDelayLineEffect;
   float rv;

   /* phasing - set */
   rv = phasing->get_param(10.0f, AAX_MICROSECONDS, AAX_LFO_DEPTH);
   TRY(10e-6f, rv);

   rv = phasing->get_param(10.0f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10e-3f, rv);

   rv = phasing->get_param(0.0f, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(PHASING_MIN, rv);

   rv = phasing->get_param(1.0f, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(PHASING_MAX, rv);

   rv = phasing->get_param(1.0f, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(PHASING_DEPTH, rv);

   /* phasing - get */
   rv = phasing->set_param(10e-6f, AAX_MICROSECONDS, AAX_LFO_OFFSET);
   TRY(10.0f, rv);

   rv = phasing->set_param(10e-3f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10.0f, rv);

   rv = phasing->set_param(PHASING_MIN, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(0.0f, rv);

   rv = phasing->set_param(PHASING_MAX, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(1.0f, rv);

   rv = phasing->set_param(PHASING_MIN+5.0f*PHASING_DEPTH, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(5.0f, rv);
   
   rv = phasing->set_param(PHASING_DEPTH, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(1.0f, rv);

   /* chorus - set */
   rv = chorus->get_param(10.0f, AAX_MICROSECONDS, AAX_LFO_DEPTH);
   TRY(10e-6f, rv);

   rv = chorus->get_param(10.0f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10e-3f, rv);

   rv = chorus->get_param(0.0f, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(CHORUS_MIN, rv);

   rv = chorus->get_param(1.0f, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(CHORUS_MAX, rv);

   rv = chorus->get_param(1.0f, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(CHORUS_DEPTH, rv);

   /* chorus - get */
   rv = chorus->set_param(10e-6f, AAX_MICROSECONDS, AAX_LFO_OFFSET);
   TRY(10.0f, rv);

   rv = chorus->set_param(10e-3f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10.0f, rv);

   rv = chorus->set_param(CHORUS_MIN, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(0.0f, rv);

   rv = chorus->set_param(CHORUS_MIN+1.6f*CHORUS_DEPTH, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(1.6f, rv);

   rv = chorus->set_param(CHORUS_DEPTH, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(1.0f, rv);

   /* delay */
   rv = delay->get_param(100.0f, AAX_MICROSECONDS, AAX_LFO_DEPTH);
   TRY(100e-6f, rv);

   rv = delay->get_param(10.0f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10e-3f, rv);

   rv = delay->get_param(0.0f, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(DELAY_MIN, rv);

   rv = delay->get_param(1.0f, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(DELAY_DEPTH, rv);

   /* delay - get */
   rv = delay->set_param(10e-6f, AAX_MICROSECONDS, AAX_LFO_OFFSET);
   TRY(10.0f, rv);

   rv = delay->set_param(10e-3f, AAX_MILLISECONDS, AAX_LFO_DEPTH);
   TRY(10.0f, rv);

   rv = delay->set_param(DELAY_MIN, AAX_LINEAR, AAX_LFO_OFFSET);
   TRY(0.0f, rv);
   
   rv = delay->set_param(DELAY_DEPTH, AAX_LINEAR, AAX_LFO_DEPTH);
   TRY(1.0f, rv);

   return 0;
}
