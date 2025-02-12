
#include <stdio.h>
#include <stdlib.h>

#include <aax/aax.h>

#define SAMPLE_FREQUENCY	44100
#define DEVNAME			"AeonWave Loopback"

static const char* aaxs_data_sax =   // A2, 200Hz
" <aeonwave>                                          \
    <sound frequency=\"440\">                         \
      <waveform src=\"triangle\"/>                    \
      <waveform src=\"triangle\" processing=\"mix\"/> \
    </sound>                                          \
  </aeonwave>";

void testForError(void *p, char *s)
{
   if (!p)
   {
      printf("%s\n", s);
      exit(-1);
   }
}
void testForState(int i, char *s)
{
   if (!i)
   {
      printf("%s\n", s);
      exit(-1);
   }
}

int main()
{
   aaxConfig config = aaxDriverOpenByName(DEVNAME, AAX_MODE_WRITE_STEREO);
   if (config)
   {
      unsigned int freq = SAMPLE_FREQUENCY;
      aaxBuffer buffer = aaxBufferCreate(config, freq, 1, AAX_AAXS16S);
      testForError(buffer, "Unable to generate buffer\n");

      int res = aaxBufferSetSetup(buffer, AAX_FREQUENCY, freq);
      testForState(res, "aaxBufferSetFrequency");

      res = aaxBufferSetData(buffer, aaxs_data_sax);
      testForState(res, "aaxBufferSetData");

      float rms = aaxBufferGetSetup(buffer, AAX_AVERAGE_VALUE);
      float peak = aaxBufferGetSetup(buffer, AAX_PEAK_VALUE);
      printf("peak: %f, rms: %f\n", peak, rms);
   }
   return 0;
}
