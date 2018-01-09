/*
 * Copyright 2012-2017 by Erik Hofman.
 * Copyright 2012-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>      /* for printf */
#include <math.h>	/* for powf */
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
# if HAVE_STRINGS_H
#  include <strings.h>   /* strcasecmp */
# endif
#endif


#include <aax/aax.h>
#include <xml.h>

#include <base/types.h>
#include "api.h"

#define SYNTHCONFIG_PATH		"/home/erik/.aaxsynth.xml"

#if USE_MIDI
static int _get_bank_num(const char*);
static int _get_inst_num(const char*);

AAX_API aaxController AAX_APIENTRY
aaxControllerCeate(aaxConfig config)
{
    _controller_t* inst = calloc(1, sizeof(_controller_t));
    if (inst)
    {
        inst->id = INSTRUMENT_ID;
        inst->handle = config;
#if 0
        inst->frequency_hz = 44100;
        inst->format = AAX_PCM16S;
        inst->sound_min = 3;
        inst->sound_max = 87;
        inst->sound_step = 12;		/* one octave */
#endif
        inst->frame = aaxAudioFrameCreate(config);
        aaxAudioFrameSetState(inst->frame, AAX_PLAYING);
        aaxMixerRegisterAudioFrame(config, inst->frame);
    }
    return (aaxController)inst;
}

AAX_API int AAX_APIENTRY
aaxControllerDestroy(aaxController controller)
{
    _controller_t* inst = get_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
        aaxAudioFrameSetState(inst->frame, AAX_STOPPED);
        aaxMixerDeregisterAudioFrame(inst->handle, inst->frame);
        aaxAudioFrameDestroy(inst->frame);

        _intBufErase(&inst->note, _AAX_NOTE, free);

        inst->id = 0xdeadbeaf;
        free(inst);
    }
    return rv;
}

int
AAX_APIENTRY aaxControllerSetup(aaxController controller, enum aaxSetupType type, int setup)
{
    _controller_t* inst = get_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
        switch(type)
        {
#if 0
        case AAX_FREQUENCY:
            inst->frequency_hz = setup;
            rv = AAX_TRUE;
            break;
        case AAX_FORMAT:
            inst->format = setup;
            rv = AAX_TRUE;
            break;
#endif
        default:
            break;
        }
    }
    return rv;
}

int
AAX_APIENTRY aaxControllerGetSetup(aaxController controller, enum aaxSetupType type)
{
    _controller_t* inst = get_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
        switch(type)
        {
#if 0
        case AAX_FREQUENCY:
            rv = inst->frequency_hz;
            break;
        case AAX_FORMAT:
            rv = inst->format;
            break;
#endif
        default:
            break;
       }
    }
    return rv;
}


AAX_API int AAX_APIENTRY
aaxControllerLoad(aaxController controller, const char* path, unsigned int bank, unsigned int num)
{
    _controller_t* inst = get_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
    }
    return rv;
}

AAX_API int AAX_APIENTRY
aaxControllerLoadByName(aaxController controller, const char* path, const char* name)
{
   unsigned int bank = _get_bank_num(name);
   unsigned int inst_num = _get_inst_num(name);
   return aaxControllerLoad(controller, path, bank, inst_num);
}

AAX_API int AAX_APIENTRY
aaxControllerRegister(aaxController controller)
{
    _controller_t* inst = get_valid_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
// register audio frame at the mixer
    }
    return rv;
}

AAX_API int AAX_APIENTRY
aaxControllerDeregister(aaxController controller)
{
    _controller_t* inst = get_valid_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
// deregister audio frame from the mixer
    }
    return rv;
}

AAX_API int AAX_APIENTRY
aaxControllerSetParam(aaxController controller,
                      enum aaxControllerParameter param, float value)
{
    _controller_t* inst = get_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
        switch (param)
        {
        case AAX_NOTE_SUSTAIN:
            inst->sustain = value;
// update all notes;
            break;
        case AAX_NOTE_SOFTEN:
            inst->soften = value;
// update all notes;
            break;
        default:
            break;
        }
    }
    return rv;
}

AAX_API float AAX_APIENTRY
aaxControllerGetParam(aaxController controller,
                      enum aaxControllerParameter param)
{
    _controller_t* inst = get_controller(controller);
    float rv = 0.0f;
    if (inst)
    {
        switch (param)
        {
        case AAX_NOTE_SUSTAIN:
            rv = inst->sustain;
            break;
        case AAX_NOTE_SOFTEN:
            rv = inst->soften;
            break;
        default:
            break;
        }
    }
    return rv;
}


AAX_API int AAX_APIENTRY
aaxControllerNoteOn(aaxController controller, unsigned int note)
{
    _controller_t* inst = get_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
// play note
    }
    return rv;
}

AAX_API int AAX_APIENTRY
aaxControllerNoteOff(aaxController controller, unsigned int note)
{
    _controller_t* inst = get_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
// stop note playback
    }
    return rv;
}

AAX_API int AAX_APIENTRY
aaxControllerNoteSetParam(aaxController controller, unsigned int note_no,
                         enum aaxControllerParameter param, float value)
{
    _controller_t* inst = get_controller(controller);
    int rv = AAX_FALSE;
    if (inst)
    {
        if (!is_nan(value))
        {
            if (note_no <= 128)
            {
                unsigned int pos = note_no % inst->polyphony;
                _note_t *note;
// TODO: Find the proper note in the buffer list
                switch (param)
                {
                case AAX_NOTE_PITCHBEND:
                    note->pitchbend = _MINMAX(value, 0.0f, 1.0f);
                    break;
                case AAX_NOTE_VELOCITY:
                    note->velocity = _MINMAX(value, 0.0f, 1.0f);
                    break;
                case AAX_NOTE_AFTERTOUCH:
                    note->aftertouch = _MINMAX(value, 0.0f, 1.0f);
                    break;
                case AAX_NOTE_DISPLACEMENT:
                    note->displacement = _MINMAX(value, 0.0f, 1.0f);
                    break;
                case AAX_NOTE_SUSTAIN:
//                  note->sustain = _MINMAX(value, 0.0f, 1.0f);
//                  break;
                case AAX_NOTE_SOFTEN:
//                  note->soften = _MINMAX(value, 0.0f, 1.0f);
//                  break;
                default:
                    break;
                }
            }
        }
    }
    return rv;
}

AAX_API float AAX_APIENTRY
aaxControllerNoteGetParam(aaxController controller, unsigned int note_no,
                         enum aaxControllerParameter param)
{
    _controller_t* inst = get_controller(controller);
    float rv = 0.0f;
    if (inst)
    {
        if (note_no <= 128)
        {
            unsigned int pos = note_no % inst->polyphony;
            _note_t *note;
// TODO: Find the proper note in the buffer list
            switch (param)
            {
            case AAX_NOTE_PITCHBEND:
                rv = note->pitchbend;
                break;
            case AAX_NOTE_VELOCITY:
                rv = note->velocity;
                break;
            case AAX_NOTE_AFTERTOUCH:
                rv = note->aftertouch;
                break;
            case AAX_NOTE_DISPLACEMENT:
                rv = note->displacement;
                break;
            case AAX_NOTE_SUSTAIN:
//              rv = note->sustain;
                rv = inst->sustain;
                break;
            case AAX_NOTE_SOFTEN:
//              rv = note->soften;
                rv = inst->soften;
                break;
            default:
                break;
            }
        }
    }
    return rv;
}

/* ------------------------------------------------------------------------- */

typedef struct
{
    enum aaxWaveformType waveform;
    enum aaxProcessingType type;
    float frequency_factor;
    float gain_ratio;
} _waveform_t;

static void _add_buffers(_controller_t*, void *);
static void _add_filters(_controller_t*, void *);
static void _add_effects(_controller_t*, void *);

_controller_t*
get_controller(aaxController inst)
{
   _controller_t *controller = (_controller_t *)inst;
   _controller_t *rv = NULL;

   if (controller && controller->id == INSTRUMENT_ID) {
      rv = controller;
   }
   return rv;
}

_controller_t*
get_valid_controller(aaxController inst)
{
   _controller_t *controller = (_controller_t *)inst;
   _controller_t *rv = NULL;

   if (controller && controller->id == INSTRUMENT_ID && controller->handle) {
      rv = controller;
   }
   return rv;
}

static float
_note_to_frequency(float n)
{
    float npow = (n-69.0f)/12.0f;	/* A4 = note 69 for midi layout */
    return 440.0f*powf(2.0f, npow);
}

static int
_construct_controller(void* xid, aaxController controller)
{
    _controller_t *inst = (_controller_t*)controller;
    int rv = AAX_FALSE;
    void *xbid, *xeid;

//  inst->sound_min = xmlNodeGetInt(xid, "note-min");
//  inst->sound_max = xmlNodeGetInt(xid, "note-max");
//  if (inst->sound_max <= inst->sound_min) inst->sound_max = 128;
//  inst->sound_step = _MINMAX(xmlNodeGetInt(xid, "note-step"), 1, 128);
    inst->polyphony = xmlNodeGetInt(xid, "polyphony");	/* no emitters */

    xbid = xmlNodeGet(xid, "buffer");
    if (xbid)
    {
        _add_buffers(inst, xbid);
        xmlFree(xbid);

        xeid = xmlNodeGet(xid, "emitter");
        if (xeid)
        {
            _add_filters(inst, xeid);
            _add_effects(inst, xeid);
            xmlFree(xeid);
        }
    }
    return rv;
}

static void
_add_buffers(_controller_t* inst, void *xbid)
{
    _waveform_t waveform[16];
    unsigned int i, j, num;
    void *xwid;
    char *ptr;

#if 0
    num = (inst->sound_max-inst->sound_min);
    inst->sound = malloc(num*(sizeof(_timbre_t*)+sizeof(_timbre_t)));
    ptr = (char*)(inst->sound + num);
    for (i=0; i<num; i++)
    {
        inst->sound[i] = (_timbre_t*)ptr;
        ptr += sizeof(_timbre_t);
    }
#endif

    xwid = xmlMarkId(xbid);
    num = xmlNodeGetNum(xwid, "waveform");
    if (num>16) num = 16;
    for (i=0; i<num; i++)
    {
        if (xmlNodeGetPos(xbid, xwid, "waveform", i) != 0)
        {
            void *node;
            float f;

            node = xmlNodeGet(xwid, "processing-type");
            if (!xmlCompareString(node, "set")) {
                waveform[i].waveform = AAX_OVERWRITE;
            } else if (!xmlCompareString(node, "add")) {
                    waveform[i].waveform = AAX_ADD;
            } else if (!xmlCompareString(node, "mix")) {
                waveform[i].waveform = AAX_MIX;
            } else if (!xmlCompareString(node, "modulate")) {
                waveform[i].waveform = AAX_RINGMODULATE;
            } else {
                char *s = xmlGetString(node);
                printf("Unknown processing type: '%s'\n", s); 
                free(s);
            }   

            node = xmlNodeGet(xwid, "waveform-type");
            if (!xmlCompareString(node, "triangle")) {
                waveform[i].type = AAX_TRIANGLE_WAVE;
            } else if (!xmlCompareString(node, "sine")) {
                waveform[i].type = AAX_SINE_WAVE;
            } else if (!xmlCompareString(node, "square")) {
                waveform[i].type = AAX_SQUARE_WAVE;
            } else if (!xmlCompareString(node, "sawtooth")) {
                    waveform[i].type = AAX_SAWTOOTH_WAVE;
            } else if (!xmlCompareString(node, "impulse")) {
                waveform[i].type = AAX_IMPULSE_WAVE;
            } else if (!xmlCompareString(node, "white-noise")) {
                waveform[i].type = AAX_WHITE_NOISE;
            } else if (!xmlCompareString(node, "pink-noise")) {
                waveform[i].type = AAX_PINK_NOISE;
            } else if (!xmlCompareString(node, "brownian-noise")) {
                waveform[i].type = AAX_BROWNIAN_NOISE;
            }
            else
            {
                char *s = xmlGetString(node);
                printf("Unknown waveform type: '%s'\n", s);
                free(s);
            }

            f = (float)xmlNodeGetDouble(xwid, "frequency_factor");
            waveform[i].frequency_factor = f ? f : 1.0f;

            f = (float)xmlNodeGetDouble(xwid, "gain-ratio");
            waveform[i].gain_ratio = f ? f : 0.5f;
        }
    }

#if 0
// TIMBRES
    for (j=inst->sound_min; j<inst->sound_max; j += inst->sound_step)
    {
        enum aaxFormat format = AAX_PCM16S;
        unsigned int n;
        aaxBuffer buf;
        float freq;
        int k;

         /*
          * using (j+inst->sound_step-1) makes the last note of the lot
          * have a pitch of 1.0f and every other note in the lot a pitch
          * below 1.0f which has better interpolation than pitch > 1.0f.
          */
        freq = _note_to_frequency((float)(j+inst->sound_step-1));
        buf = aaxBufferCreate(inst->handle, (unsigned int)(0.25f*freq), 1, format);
        for (i=0; i<num; i++)
        {
            float rate = waveform[i].frequency_factor;
            float ratio = waveform[i].gain_ratio;
            aaxBufferProcessWaveform(buf, rate, waveform[i].waveform,
                                          ratio, waveform[i].type);
        }

        k = j - inst->sound_min;
        for (n=0; n<inst->sound_step; n++)
        {
           float f = _note_to_frequency((float)(j+n));
           inst->sound[k+n]->pitch = f/freq;
           inst->sound[k+n]->buffer = buf;
        }
    }
#endif
    free(xwid);
}

static void
_add_filters(_controller_t* inst, void *xbid)
{
    aaxConfig config = inst->handle;
    unsigned int i, num;
    void *xefid;
    char *ptr;

    num = inst->polyphony;
    inst->note = malloc(num*(sizeof(_note_t*)+sizeof(_note_t)));
    for (i=0; i<num; i++)
    {
        inst->note[i] = (_note_t*)ptr;
        inst->note[i]->emitter = aaxEmitterCreate();
        ptr += sizeof(_note_t);
    }

    xefid = xmlMarkId(xbid);
    num = xmlNodeGetNum(xefid, "filter");
    for (i=0; i<num; i++)
    {
        if (xmlNodeGetPos(xbid, xefid, "filter", i) != 0)
        {
            enum aaxFilterType type = AAX_FILTER_NONE;
            void *node;

            node = xmlNodeGet(xefid, "type");
            if (!xmlCompareString(node, "timed gain")) {
                type = AAX_TIMED_GAIN_FILTER;
            } else if (!xmlCompareString(node, "tremolo")) {
                type = AAX_DYNAMIC_GAIN_FILTER;
            }

            if (type != AAX_FILTER_NONE)
            {
                unsigned int slot, numslots;
                aaxFilter filter;
                void *xsid;

                filter = aaxFilterCreate(config, type);
                inst->filter[type] = filter;

                xsid = xmlMarkId(xefid);
                numslots = xmlNodeGetNum(xsid, "slot");
                for (slot=0; slot<numslots; slot++)
                {
                    if (xmlNodeGetPos(xefid, xsid, "slot", slot) != 0)
                    {
                        int n = xmlAttributeGetInt(xsid, "n");
                        float p[4];
                        p[0] = (float)xmlNodeGetDouble(xsid, "p0");
                        p[1] = (float)xmlNodeGetDouble(xsid, "p1");
                        p[2] = (float)xmlNodeGetDouble(xsid, "p2");
                        p[3] = (float)xmlNodeGetDouble(xsid, "p3");
                        if (!n) n = slot;
                        aaxFilterSetSlotParams(filter, n, AAX_LINEAR, p);
                    }
                }
                free(xsid);
                aaxFilterSetState(filter, AAX_TRUE);
            }
        }
    }
}

static void
_add_effects(_controller_t* inst, void *xbid)
{
    aaxConfig config = inst->handle;
    unsigned int i, num;
    void *xefid;

    xefid = xmlMarkId(xbid);
    num = xmlNodeGetNum(xefid, "effect");
    for (i=0; i<num; i++)
    {
        if (xmlNodeGetPos(xbid, xefid, "effect", i) != 0)
        {
            enum aaxEffectType type = AAX_EFFECT_NONE;
            void *node;

            node = xmlNodeGet(xefid, "type");
            if (!xmlCompareString(node, "timed pitch")) {
                type = AAX_TIMED_PITCH_EFFECT;
            } else if (!xmlCompareString(node, "vibrato")) {
                type = AAX_DYNAMIC_PITCH_EFFECT;
            }

            if (type != AAX_EFFECT_NONE)
            {
                unsigned int slot, numslots;
                aaxEffect effect;
                void *xsid;

                effect = aaxEffectCreate(config, type);
                inst->effect[type] = effect;

                xsid = xmlMarkId(xefid);
                numslots = xmlNodeGetNum(xsid, "slot");
                for (slot=0; slot<numslots; slot++)
                {
                    if (xmlNodeGetPos(xefid, xsid, "slot", slot) != 0)
                    {
                        int n = xmlAttributeGetInt(xsid, "n");
                        float p[4];
                        p[0] = (float)xmlNodeGetDouble(xsid, "p0");
                        p[1] = (float)xmlNodeGetDouble(xsid, "p1");
                        p[2] = (float)xmlNodeGetDouble(xsid, "p2");
                        p[3] = (float)xmlNodeGetDouble(xsid, "p3");
                        if (!n) n = slot;
                        aaxEffectSetSlotParams(effect, n, AAX_LINEAR, p);
                    }
                }
                free(xsid);
                aaxEffectSetState(effect, AAX_TRUE);
            }
        }
    }
}

int
_get_inst_num(const char* name)
{
    int rv = AAX_FALSE;
    char *path;
    void *rid;

    path = SYNTHCONFIG_PATH;
    rid = xmlOpen(path);
    if (rid)
    {
        void *xsid = xmlNodeGet(rid, "soundbank");
        if (xsid)
        {
            unsigned int bank, numbanks;
            void *xbid;

            xbid = xmlMarkId(xsid);
            numbanks = xmlNodeGetNum(xbid, "bank");
            for (bank=0; bank<numbanks; bank++)
            {
                if (xmlNodeGetPos(xsid, xbid, "bank", bank) != 0)
                {
                    unsigned int i, num;
                    void *xcid = xmlMarkId(xbid);
                    num = xmlNodeGetNum(xcid, "controller");
                    for (i=0; i<num; i++)
                    {
                        if (xmlNodeGetPos(xbid, xcid, "controller", i)  != 0)
                        {
                            if (!xmlAttributeCompareString(xcid, "name", name))
                            {
                                rv = i;
                                break;
                            }
                        }
                    }
                    xmlFree(xcid);
                }
                if (rv) break;
            }
            xmlFree(xbid);
            xmlFree(xsid);
        }
    }

    return rv;
}

int
_get_bank_num(const char* name)
{
    int rv = AAX_FALSE;
    char *path;
    void *rid;

    path = SYNTHCONFIG_PATH;
    rid = xmlOpen(path);
    if (rid)
    {
        void *xsid = xmlNodeGet(rid, "soundbank");
        if (xsid)
        {
            unsigned int bank, numbanks;
            void *xbid;

            xbid = xmlMarkId(xsid);
            numbanks = xmlNodeGetNum(xbid, "bank");
            for (bank=0; bank<numbanks; bank++)
            {
                if (xmlNodeGetPos(xsid, xbid, "bank", bank) != 0)
                {
                    unsigned int i, num;
                    void *xiid = xmlMarkId(xbid);
                    num = xmlNodeGetNum(xiid, "controller");
                    for (i=0; i<num; i++)
                    {
                        if (xmlNodeGetPos(xbid, xiid, "controller", i)  != 0)
                        {
                            if (!xmlNodeCompareString(xiid, "name", name))
                            {
                                rv = bank;
                                break;
                            }
                        }
                    }
                    xmlFree(xiid);
                }
                if (rv) break;
            }
            xmlFree(xbid);
            xmlFree(xsid);
        }
    }

    return rv;
}
#endif
