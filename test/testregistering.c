

#include <stdio.h>
#include <unistd.h>
#include <aax/aax.h>

#define TEST(rv) if (!rv) { printf("%i: %s\n", __LINE__, \
                                   aaxGetErrorString(aaxGetErrorNo())); }

int main()
{
   aaxConfig config = aaxDriverOpenDefault(AAX_MODE_WRITE_STEREO);
   if (config)
   {
      aaxFrame parent = aaxAudioFrameCreate(config);
      aaxFrame frame = aaxAudioFrameCreate(config);

      if (parent && frame)
      {
         TEST( aaxMixerRegisterAudioFrame(config, parent) 	);
         TEST( aaxMixerRegisterAudioFrame(config, frame) 	);

         sleep(0);

         TEST( aaxMixerDeregisterAudioFrame(config, frame) 	);
         TEST( aaxAudioFrameRegisterAudioFrame(parent, frame) 	);

         sleep(0);

         TEST( aaxMixerSetState(config, AAX_INITIALIZED) 	);
         TEST( aaxMixerSetState(config, AAX_PLAYING) 		);

         sleep(0);

         TEST( aaxAudioFrameDeregisterAudioFrame(parent, frame) );
         TEST( aaxMixerRegisterAudioFrame(config, frame) 	);

         sleep(0);

         TEST( aaxMixerDeregisterAudioFrame(config, frame) 	);
         TEST( aaxAudioFrameRegisterAudioFrame(parent, frame) 	);

         sleep(0);

         TEST( aaxAudioFrameDeregisterAudioFrame(parent, frame) );
         TEST( aaxAudioFrameDestroy(frame) 			);

         sleep(0);

         TEST( aaxMixerDeregisterAudioFrame(config, parent) 	);
         TEST( aaxAudioFrameDestroy(parent) 			);
      }
      aaxDriverDestroy(config);
   }
}
