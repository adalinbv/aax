/*
 * Copyright 2012 by Erik Hofman.
 * Copyright 2012 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <ringbuffer.h>
#include <objects.h>
#include <api.h>

char
_aaxEmittersProcess(_oalRingBuffer *dest_rb, _aaxMixerInfo *info,
                    _oalRingBuffer2dProps *sp2d, _oalRingBuffer3dProps *sp3d,
                    _oalRingBuffer2dProps *fp2d, _oalRingBuffer3dProps *fp3d,
                    _intBuffers *e2d, _intBuffers *e3d,
                    const _aaxDriverBackend* be, void *be_handle)
{
   _oalRingBuffer3dProps *pp3d = fp3d ? fp3d : sp3d;
   _oalRingBuffer2dProps *pp2d = fp2d ? fp2d : sp2d;
   unsigned int num, stage;
   _intBuffers *he = e3d;
   char rv = AAX_FALSE;
   float dt;

   num = 0;
   stage = 0;
   dt = _oalRingBufferGetParamf(dest_rb, RB_DURATION_SEC);
   do
   {
      unsigned int i, no_emitters;

      no_emitters = _intBufGetMaxNum(he, _AAX_EMITTER);
      num += no_emitters;

      for (i=0; i<no_emitters; i++)
      {
         _intBufferData *dptr_src;
         _emitter_t *emitter;
         _aaxEmitter *src;

         dptr_src = _intBufGet(he, _AAX_EMITTER, i);
         if (!dptr_src) continue;

         dest_rb->curr_pos_sec = 0.0f;

         emitter = _intBufGetDataPtr(dptr_src);
         src = emitter->source;
         if (_IS_PLAYING(src))
         {
            _intBufferData *dptr_sbuf;
            unsigned int nbuf, rv = 0;
            int streaming;

            rv = AAX_TRUE;

            nbuf = _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
            assert(nbuf > 0);

            streaming = (nbuf > 1);
            dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, src->pos);
            if (dptr_sbuf)
            {
               _embuffer_t *embuf = _intBufGetDataPtr(dptr_sbuf);
               _oalRingBuffer *src_rb = embuf->ringbuffer;
               do
               {
                  if (_IS_STOPPED(src)) {
                     _oalRingBufferStop(src_rb);
                  }
                  else if (_oalRingBufferGetParami(src_rb, RB_IS_PLAYING) == 0)
                  {
                     if (streaming) {
                        _oalRingBufferStartStreaming(src_rb);
                     } else {
                        _oalRingBufferStart(src_rb);
                     }
                  }

                  --src->update_ctr;
                   
                  /* 3d mixing */
                  if (stage == 0)
                  {
                     assert(_IS_POSITIONAL(src));
                     if (!src->update_ctr) {
                        be->prepare3d(sp3d, fp3d, info, pp2d, src);
                     }
                     if (src->curr_pos_sec >= pp2d->delay_sec) {
                        rv = be->mix3d(be_handle, dest_rb, src_rb, src->props2d,
                                       pp2d, emitter->track,
                                       src->update_ctr, nbuf);
                     }
                  }
                  else
                  {
                     assert(!_IS_POSITIONAL(src));
                     rv = be->mix2d(be_handle, dest_rb, src_rb, src->props2d,
                                           pp2d, 1.0, 1.0, src->update_ctr,
                                           nbuf);
                  }

                  if (!src->update_ctr) {
                     src->update_ctr = src->update_rate;
                  }

                  src->curr_pos_sec += dt;

                  /*
                   * The current buffer of the source has finished playing.
                   * Decide what to do next.
                   */
                  if (rv)
                  {
                     if (streaming)
                     {
                        /* is there another buffer ready to play? */
                        if (++src->pos == nbuf)
                        {
                           /*
                            * The last buffer was processed, return to the
                            * first buffer or stop? 
                            */
                           if TEST_FOR_TRUE(emitter->looping) {
                              src->pos = 0;
                           }
                           else
                           {
                              _SET_STOPPED(src);
                              _SET_PROCESSED(src);
                              break;
                           }
                        }

                        rv &= _oalRingBufferGetParami(dest_rb, RB_IS_PLAYING);
                        if (rv)
                        {
                           _intBufReleaseData(dptr_sbuf,_AAX_EMITTER_BUFFER);
                           dptr_sbuf = _intBufGet(src->buffers,
                                              _AAX_EMITTER_BUFFER, src->pos);
                           embuf = _intBufGetDataPtr(dptr_sbuf);
                           src_rb = embuf->ringbuffer;
                        }
                     }
                     else /* !streaming */
                     {
                        _SET_PROCESSED(src);
                        break;
                     }
                  }
               }
               while (rv);
               _intBufReleaseData(dptr_sbuf, _AAX_EMITTER_BUFFER);
            }
            _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);
            _oalRingBufferStart(dest_rb);
         }
         _intBufReleaseData(dptr_src, _AAX_EMITTER);
      }
      _intBufReleaseNum(he, _AAX_EMITTER);

      /*
       * stage == 0 is 3d positional audio
       * stage == 1 is stereo audio
       */
      if (stage == 0) {
         he = e2d;	/* switch to stereo */
      }
   }
   while (++stage < 2); /* process 3d positional and stereo emitters */

   _PROP_MTX_CLEAR_CHANGED(pp3d);
   _PROP_PITCH_CLEAR_CHANGED(pp3d);

   return rv;
}

