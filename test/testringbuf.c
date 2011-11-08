
#include <stdio.h>

#include <base/logging.h>
#include <aax/ringbuffer.h>

int main()
{
   static const unsigned char ch = 2;
   static const float sduration = 0.01f;
   static const float dduration = 0.23f;
   static const float f = 8000.0f;
   _oalRingBuffer *rb, *out;
   unsigned int track_size;
   void *sdi, *data;
   float t = 0.0;
   int ret;

   printf("Initializing the ringbuffers.\n");
   rb = _oalRingBufferCreate();
   _oalRingBufferSetFormat(rb, 0, 0, AAX_FORMAT_PCM16);
   _oalRingBufferSetDuration(rb, sduration);
   _oalRingBufferSetNoTracks(rb, ch);
   _oalRingBufferSetFrequency(rb, f);
   _oalRingBufferInit(rb, 0);

   out = _oalRingBufferCreate();
   _oalRingBufferSetFormat(out, 0, 0, AAX_FORMAT_PCM32);
   _oalRingBufferSetDuration(out, dduration);
   _oalRingBufferSetNoTracks(out, 2);
   _oalRingBufferSetFrequency(out, 22050);
   _oalRingBufferInit(out, 0);

   track_size = sizeof(short) * f * sduration;
   printf("Allocating %ux %u bytes of memory.\n", ch, track_size);
   data = calloc(ch, track_size);

   printf("Adding data to the ringbuffer.\n");
   _oalRingBufferFillNonInterleaved(rb, data, 1);
   free(data);

   printf("Start the mixing process, loop the sample for 3 seconds.\n");
   do
   {
      ret = _oalRingBufferMixMulti16(out, rb, 1.0, 1.0);
#if 1
      printf("t: %5.1f\tret: %i\n", t, ret);
#endif
      t += dduration;
      sdi = _oalRingBufferGetDataInterleaved(out);
      free(sdi);

   } while (t < 3.0f);

   printf("Rewinding the source buffer.\n");
   _oalRingBufferRewind(rb);

   printf("Get some samples from the rungbuffer.\n");
   t = 0.0;
   while (t < 3.141596563)
   {
      _oalRingBufferGetNextSample(rb, 0, 272.2);
      t += 0.0712;
   }

   printf("Delete the ringbuffers.\n");
   _oalRingBufferDelete(out);
   _oalRingBufferDelete(rb);

   printf("Finished.\n\n");

   return 0;
}
