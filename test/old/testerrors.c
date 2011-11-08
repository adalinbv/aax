
#include <stdio.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include "driver.h"

int main()
{
   ALCdevice *device[4];
   ALCcontext *context[4][4];
   char *s;
   int i;

   int attributes[] = { ALC_FREQUENCY, 44100,
                        ALC_SYNC, ALC_FALSE,
                        0 };

   printf("testing call to al withouth a proper context\n");
   s = (char *)alGetString(AL_VENDOR);
   testForALError();

   printf("testing an arbitrary context:\n");
   alcMakeContextCurrent((ALCcontext*)0xFF30BE);
   context[0][0] = alcCreateContext((ALCdevice*)0xFF0000, attributes);
   printf("context: %x\n", (unsigned int)context[0][0]);

   printf("Opening a non exsisting 'Test' device:\n");
   device[0] = alcOpenDevice("Test");
   printf("device: %x\n", (unsigned int)device[0]);

   printf("create a new context based on the previous result: \n");
   context[0][0] = alcCreateContext(device[0], attributes);
   printf("context: %x\n", (unsigned int)context[0][0]);

   printf("Make previous context current:\n");
   alcMakeContextCurrent(context[0][0]);
   testForALCError(device[0]);

#if 1
   printf("opening 3 other devices with each 4 contexts.\n");
   for (i=1; i<4; i++)
   {
      int j;
      device[i] = alcOpenDevice(NULL);
      for(j=0; j<4; j++)
      {
         context[i][j] = alcCreateContext(device[i], NULL);
      }
      testForALCError(device[i]);
   }

   printf("Now delete the contexts and devices again.\n");
   for (i=3; i>0; i--)
   {
      int j;
      for(j=0; j<4; j++)
      {
         alcDestroyContext(context[i][j]);
      }
      testForALCError(device[i]);
      alcCloseDevice(device[i]);
   }
   printf("done.\n");
#endif

   context[0][0] = alcGetCurrentContext();
   device[0] = alcGetContextsDevice(context[0][0]);
   alcMakeContextCurrent(NULL);
   alcDestroyContext(context[0][0]);
   alcCloseDevice(device[0]);

   return 0;
}
