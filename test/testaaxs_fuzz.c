
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <aax/aax.h>

#define TRY(a) if ((a) == 0) { \
  printf("%i: %s\n", __LINE__, aaxGetErrorString(aaxGetErrorNo())); \
  aaxDriverDestroy(config); exit(-1); \
}

int main()
{
   char aaxs[] = \
   " <aeonwave>                  \
      <sound frequency=\"440\">  \
       <waveform src=\"sawtooth\"/>  \
      </sound>                   \
      <emitter looping\"true\"/> \
     </aeonwave>";

   aaxConfig config;
   int val, pos, len;

   srand(time(NULL));

   len = strlen(aaxs);
   pos = rand() % len;
   val = rand() % 255;
   aaxs[pos] = val;

   config = aaxDriverOpenDefault(AAX_MODE_WRITE_STEREO);
   if (config)
   {
      aaxEmitter emitter;
      aaxBuffer buffer;
      aaxFrame frame;

      TRY( buffer = aaxBufferCreate(config, 1, 1, AAX_AAXS16S) );
      TRY( aaxBufferSetSetup(buffer, AAX_FREQUENCY, 22050.0f) );
      TRY( aaxBufferSetData(buffer, aaxs) );

      TRY( aaxMixerSetState(config, AAX_INITIALIZED) );
//    TRY( aaxMixerSetState(config, AAX_PLAYING) );

      TRY( emitter = aaxEmitterCreate() );
      TRY( aaxEmitterAddBuffer(emitter, buffer) );
      TRY( aaxEmitterSetState(emitter, AAX_PLAYING) );

      TRY( frame = aaxAudioFrameCreate(config) );
      TRY( aaxAudioFrameSetState(frame, AAX_PLAYING) );
      TRY( aaxMixerRegisterAudioFrame(config, frame) );
      TRY( aaxAudioFrameRegisterEmitter(frame, emitter) );

      sleep(1);

      TRY( aaxEmitterSetState(emitter, AAX_PROCESSED) );
      TRY( aaxAudioFrameSetState(frame, AAX_STOPPED) );
//    TRY( aaxMixerSetState(config, AAX_STOPPED) );

      TRY( aaxAudioFrameDeregisterEmitter(frame, emitter) );
      TRY( aaxMixerDeregisterAudioFrame(config, frame) );

      TRY( aaxAudioFrameDestroy(frame) );
      TRY( aaxEmitterDestroy(emitter) );
      TRY( aaxBufferDestroy(buffer) );

      TRY( aaxDriverClose(config) );
      TRY( aaxDriverDestroy(config) );
   }
}
