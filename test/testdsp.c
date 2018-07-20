
#include <stdio.h>
#include <stdlib.h>
#include <aax/aax.h>

#define TRY(a) if ((a) == 0) { printf("%i: %s\n", __LINE__, aaxGetErrorString(aaxGetErrorNo())); exit(-1); }

static const char* aaxs = \
" <aeonwave>					\
    <sound frequency=\"440\">			\
      <waveform src=\"sine\"/>			\
    </sound>					\
  </aeonwave>";

int main()
{
   aaxConfig config;

   config = aaxDriverOpenDefault(AAX_MODE_WRITE_STEREO);
   if (config)
   {
      aaxEmitter emitter;
      aaxBuffer buffer;
      aaxFrame frame;
      int i;

      TRY( buffer = aaxBufferCreate(config, 1, 1, AAX_AAXS16S) );
      TRY( aaxBufferSetSetup(buffer, AAX_FREQUENCY, 22050.0f) );
      TRY( aaxBufferSetData(buffer, aaxs) );

      TRY( aaxMixerSetState(config, AAX_INITIALIZED) );
      TRY( aaxMixerSetState(config, AAX_PLAYING) );

      TRY( emitter = aaxEmitterCreate() );
      TRY( aaxEmitterAddBuffer(emitter, buffer) );
      TRY( aaxEmitterSetMode(emitter, AAX_LOOPING, AAX_TRUE) );
      TRY( aaxEmitterSetState(emitter, AAX_PLAYING) );

      TRY( frame = aaxAudioFrameCreate(config) );
      TRY( aaxAudioFrameSetState(frame, AAX_PLAYING) );
      TRY( aaxMixerRegisterAudioFrame(config, frame) );
      TRY( aaxAudioFrameRegisterEmitter(frame, emitter) );

      for (i=AAX_VOLUME_FILTER; i<AAX_GRAPHIC_EQUALIZER; i++)
      {
         aaxFilter filter;

         printf("filter: %-30s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxEmitterSetFilter(emitter, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxEmitterGetFilter(emitter, i) );
         TRY( aaxFilterSetState(filter, AAX_TRUE) );
         TRY( aaxEmitterSetFilter(emitter, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxEmitterGetFilter(emitter, i) );
         TRY( aaxFilterSetState(filter, AAX_FALSE) );
         TRY( aaxEmitterSetFilter(emitter, filter) );
         TRY( aaxFilterDestroy(filter) );

         printf("ok\n");
      }

      for (i=AAX_PITCH_EFFECT; i<AAX_REVERB_EFFECT; i++)
      {
         aaxEffect effect;

         printf("effect: %-30s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxEmitterSetEffect(emitter, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxEmitterGetEffect(emitter, i) );
         TRY( aaxEffectSetState(effect, AAX_TRUE) );
         TRY( aaxEmitterSetEffect(emitter, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxEmitterGetEffect(emitter, i) );
         TRY( aaxEffectSetState(effect, AAX_FALSE) );
         TRY( aaxEmitterSetEffect(emitter, effect) );
         TRY( aaxEffectDestroy(effect) );

         printf("ok\n");
      }


      TRY( aaxEmitterSetState(emitter, AAX_PROCESSED) );
      TRY( aaxAudioFrameSetState(frame, AAX_STOPPED) );
      TRY( aaxMixerSetState(config, AAX_STOPPED) );

      TRY( aaxAudioFrameDeregisterEmitter(frame, emitter) );
      TRY( aaxMixerDeregisterAudioFrame(config, frame) );

      TRY( aaxAudioFrameDestroy(frame) );
      TRY( aaxEmitterDestroy(emitter) );
      TRY( aaxBufferDestroy(buffer) );
   }

   TRY( aaxDriverClose(config) );
   TRY( aaxDriverDestroy(config) );
}
