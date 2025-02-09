
#include <stdio.h>
#include <stdlib.h>

#include <aax/aax.h>

#define SAMPLE_FREQUENCY	44100
#define DEVNAME			"AeonWave Loopback"

#if 0
static const char* aaxs_data_sax =   // A2, 200Hz
" <aeonwave>                                          \
    <sound frequency=\"440\">                         \
      <waveform src=\"triangle\"/>                    \
      <waveform src=\"triangle\" processing=\"mix\"/> \
    </sound>                                          \
  </aeonwave>";
#else
static const char* aaxs_data_sax =   // A2, 200Hz
" <aeonwave>                                    \
    <sound frequency=\"440\">                   \
      <waveform src=\"triangle\"/>              \
      <waveform src=\"triangle\" processing=\"mix\" pitch=\"2.0\" ratio=\"0.333\"/>             \
      <waveform src=\"triangle\" processing=\"add\" pitch=\"4.0\" ratio=\"-0.2\"/>              \
    </sound>                                    \
    <emitter>                                   \
      <filter type=\"envelope\">                \
       <slot n=\"0\">                           \
        <p1>0.01</p1>                           \
        <p2>1.2</p2>                            \
        <p3>0.05</p3>                           \
       </slot>                                  \
       <slot n=\"1\">                           \
         <p0>0.7</p0>                           \
         <p1>0.1</p1>                           \
         <p2>0.6</p2>                           \
         <p3>0.05</p3>                          \
       </slot>                                  \
       <slot n=\"2\">                           \
         <p0>0.45</p0>                          \
         <p1>1.2</p1>                           \
       </slot>                                  \
      </filter>                                 \
    </emitter>                                  \
  </aeonwave>";
#endif

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
printf("1\n");
      aaxBuffer buffer = aaxBufferCreate(config, freq, 1, AAX_AAXS16S);
      testForError(buffer, "Unable to generate buffer\n");

printf("2\n");
      int res = aaxBufferSetSetup(buffer, freq, SAMPLE_FREQUENCY);
      testForState(res, "aaxBufferSetFrequency");

printf("3\n");
      res = aaxBufferSetData(buffer, aaxs_data_sax);
      testForState(res, "aaxBufferSetData");

printf("4\n");
      float rms = aaxBufferGetSetup(buffer, AAX_AVERAGE_VALUE);
      printf("rms: %f\n", rms);
printf("5\n");
   }
   return 0;
}
