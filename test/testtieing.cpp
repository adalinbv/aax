
#include <cstdio>
#include <cstring>

#include <aax/aeonwave>

namespace aax = aeonwave;

// Note:
// Absolue means absolute values for position and velocity and direction
// Relative means relative values for position and velocity and direction
//                (relative to the sensors values)
#define MODE	AAX_ABSOLUTE

const char *sine = "<?xml version='1.0'?>	\
 <aeonwave>					\
  <sound frequency='440' duration='0.1'>	\
   <waveform src='sine'/>			\
  </sound>					\
 </aeonwave>";

#define TEST(a, b, c) \
 if ((a) != (b)) printf("%s differs: %lx should be: %lx\n", (c), (a), (b));

#define TEST_FP(a, b, c) \
 if ((a) != (b)) printf("%s differs: %f should be: %f\n", (c), (a), (b))

int main()
{
   // sensor state
   aax::AeonWave aax(AAX_MODE_WRITE_STEREO);
   aax.set(AAX_INITIALIZED);
   aax.set(AAX_PLAYING);
   aax.set(AAX_SUSPENDED);

   // emitter state
   aax::Buffer buf(aax, 1600, 1, AAX_AAXS16S);
   buf.set(AAX_FREQUENCY, 16000);
   buf.fill(sine);

   aax::Emitter emitter(MODE);
   emitter.set(AAX_INITIALIZED);
   emitter.add(buf);
   emitter.set(AAX_PLAYING);
   aax.add(emitter);

   aax::Param tremolo_freq = 1.0f;
   aax::Param tremolo_depth = 0.0f;
   aax::Param tremolo_offset = 0.0f;
   aax::Status tremolo_state = false;

   emitter.tie(tremolo_freq, AAX_DYNAMIC_GAIN_FILTER, AAX_LFO_FREQUENCY);
   emitter.tie(tremolo_depth, AAX_DYNAMIC_GAIN_FILTER, AAX_LFO_DEPTH);
   emitter.tie(tremolo_offset, AAX_DYNAMIC_GAIN_FILTER, AAX_LFO_OFFSET);
   emitter.tie(tremolo_state, AAX_DYNAMIC_GAIN_FILTER);

   // update
   aax.set(AAX_UPDATE);

   tremolo_freq = 5.0f;
   tremolo_depth = 0.05;
   tremolo_offset = 0.95;
   tremolo_state = AAX_SINE;

   // update
   aax.set(AAX_UPDATE);

   aax::dsp dsp = emitter.get(AAX_DYNAMIC_GAIN_FILTER);
   float freq = dsp.get(AAX_LFO_FREQUENCY);
   float depth = dsp.get(AAX_LFO_DEPTH);
   float offset = dsp.get(AAX_LFO_OFFSET);
   uint64_t state = dsp.state();

   TEST_FP(freq, float(tremolo_freq), "frequency");
   TEST_FP(depth, float(tremolo_depth), "depth");
   TEST_FP(offset, float(tremolo_offset), "offset");
   TEST(state, uint64_t(tremolo_state), "state");

   emitter.set(AAX_PROCESSED);
   aax.remove(emitter);

   return 0;
}
