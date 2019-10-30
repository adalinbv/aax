/*
 * Copyright (C) 2018 by Erik Hofman.
 * Copyright (C) 2018 by Adalin B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ADALIN B.V. ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL ADALIN B.V. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUTOF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>

#if HAVE_EBUR128
#include <ebur128.h>
#endif

#include <xml.h>
#include <aax/aax.h>

#include <base/types.h>

#include "driver.h"
#include "wavfile.h"

#if defined(WIN32)
# define TEMP_DIR		getenv("TEMP")
# define PATH_SEPARATOR		'\\'
#else    /* !WIN32 */
# define TEMP_DIR		"/tmp"
# define PATH_SEPARATOR		'/'
#endif
#define LEVEL_16DB		0.15848931670f
#define LEVEL_20DB		0.1f

static char debug = 0;
static float freq = 220.0f;
static char* false_const = "false";

static float _lin2log(float v) { return log10f(v); }
//  static float _log2lin(float v) { return powf(10.f,v); }
static float _lin2db(float v) { return 20.f*log10f(v); }
static float _db2lin(float v) { return _MINMAX(powf(10.f,v/20.f),0.f,10.f); }

static char* prttystr(char *s) {
    if (s) {
        char capital = 1;
        for (int i=0; s[i]; ++i) {
            if (isspace(s[i]) || s[i] == '(') capital = 1;
            else {
                if (capital) s[i] = toupper(s[i]);
                else s[i] = tolower(s[i]);
                capital = 0;
            }
        }
    }
    return s;
}

static char* lwrstr(char *s) {
    if (s) for (int i=0; s[i]; ++i) { s[i] = tolower(s[i]); }
    return s;
}

enum type_t
{
    WAVEFORM = 0,
    FILTER,
    EFFECT,
    EMITTER,
    FRAME
};

static const char* format_float3(float f)
{
    static char buf[32];

    if (f >= 100.0f) {
        snprintf(buf, 20, "%.1f", f);
    }
    else
    {
        snprintf(buf, 20, "%.3g", f);
        if (!strchr(buf, '.')) {
            strcat(buf, ".0");
        }
    }
    return buf;
}

static const char* format_float6(float f)
{
    static char buf[32];

    if (f >= 100.0f) {
        snprintf(buf, 20, "%.1f", f);
    }
    else
    {
        snprintf(buf, 20, "%.6g", f);
        if (!strchr(buf, '.')) {
            strcat(buf, ".0");
        }
    }
    return buf;
}

static float note2freq(uint8_t d) {
    return 440.0f*powf(2.0f, ((float)d-69.0f)/12.0f);
}

static uint8_t freq2note(float freq) {
    return 12.0f*log2f(freq/440.0f)+69.0f;
}

#if 0
static float note2pitch(uint8_t d, float freq) {
    return note2freq(d)/freq;
}

static uint8_t pitch2note(float pitch, float freq) {
    return freq2note(pitch*freq);
}
#endif

struct info_t
{
    char *path;

    float pan;
    int16_t program;
    int16_t bank;
    char* name;

    char *license;
    struct copyright_t
    {
        unsigned from, until;
        char *by;
    } copyright[2];

    struct note_t
    {
        uint8_t polyphony;
        uint8_t min, max, step;
    } note;

    int aftertouch_mode;
};

void fill_info(struct info_t *info, void *xid, const char *filename)
{
    void *xtid;
    char *ptr;

    ptr = strrchr(filename, PATH_SEPARATOR);
    if (ptr)
    {
        size_t size = ptr-filename;
        info->path = malloc(size+1);
        if (info->path)
        {
            memcpy(info->path, filename, size);
            info->path[size] = 0;
        }
    }

    info->pan = _MINMAX(xmlAttributeGetDouble(xid, "pan"), -1.0f, 1.0f);
    info->program = info->bank = -1;

    if (xmlAttributeExists(xid, "program")) {
        info->program = _MINMAX(xmlAttributeGetInt(xid, "program"), 0, 127);
    }
    if (xmlAttributeExists(xid, "bank")) {
        info->bank = _MINMAX(xmlAttributeGetInt(xid, "bank"), 0, 127);
    }
    info->name = prttystr(xmlAttributeGetString(xid, "name"));

    xtid = xmlNodeGet(xid, "aftertouch");
    if (xtid)
    {
        info->aftertouch_mode = xmlAttributeGetInt(xtid, "mode");
        xmlFree(xtid);
    }

    xtid = xmlNodeGet(xid, "note");
    if (xtid)
    {
        info->note.polyphony = _MAX(xmlAttributeGetInt(xtid, "polyphony"), 0);
        info->note.min = _MINMAX(xmlAttributeGetInt(xtid, "min"), 0, 127);
        info->note.max = _MINMAX(xmlAttributeGetInt(xtid, "max"), 0, 127);
        info->note.step = _MAX(xmlAttributeGetInt(xtid, "step"), 0);
        xmlFree(xtid);
    }
    if (info->note.polyphony == 0) info->note.polyphony = 1;

    xtid = xmlNodeGet(xid, "license");
    if (xtid)
    {
        unsigned int c, cnum = xmlNodeGetNum(xid, "copyright");
        void *xcid = xmlMarkId(xtid);

        for (c=0; c<cnum; c++)
        {
            if (c == 2) break;
            if (xmlNodeGetPos(xid, xcid, "copyright", c) != 0)
            {
                info->copyright[c].from = xmlAttributeGetInt(xcid, "from");
                info->copyright[c].until = xmlAttributeGetInt(xcid, "until");
                info->copyright[c].by = xmlAttributeGetString(xcid, "by");
            }
        }
        info->license = xmlAttributeGetString(xtid, "type");
        xmlFree(xcid);
        xmlFree(xtid);
    }
}

void print_info(struct info_t *info, FILE *output, char commons)
{
    struct tm* tm_info;
    time_t timer;
    char year[5];
    int c, i;

    time(&timer);
    tm_info = localtime(&timer);
    strftime(year, 5, "%Y", tm_info);

    fprintf(output, " <info");
    if (info->name) fprintf(output, " name=\"%s\"", info->name);
    if (info->bank >= 0) fprintf(output, " bank=\"%i\"", info->bank);
    if (info->program >= 0) fprintf(output, " program=\"%i\"", info->program);
    fprintf(output, ">\n");

    if (commons & 0x80) {
        fprintf(output, "  <license type=\"Attribution-ShareAlike 4.0 International\"/>\n");
    } else if (info->license && strcmp(info->license, "Attribution-ShareAlike 4.0 International")) {
        fprintf(output, "  <license type=\"%s\"/>\n", info->license);
    }
    else
    {
        if (commons & 0x7f) {
            fprintf(output, "  <license type=\"Attribution-ShareAlike 4.0 International\"/>\n");
        } else {
            fprintf(output, "  <license type=\"Proprietary/Commercial\"/>\n");
        }
    }

    c = 0;
    for (i=0; i<2; ++i)
    {
       if (info->copyright[i].by)
       {
           fprintf(output, "  <copyright from=\"%i\" until=\"%s\" by=\"%s\"/>\n", info->copyright[i].from, year, info->copyright[i].by);
           c++;
       }
    }
    if (c == 0)
    {
        fprintf(output, "  <copyright from=\"2017\" until=\"%s\" by=\"Erik Hofman\"/>\n", year);
        fprintf(output, "  <copyright from=\"2017\" until=\"%s\" by=\"Adalin B.V.\"/>\n", year);
    }

    if (info->note.polyphony)
    {
        fprintf(output, "  <note polyphony=\"%i\"", info->note.polyphony);
        if (info->note.min) fprintf(output, " min=\"%i\"", info->note.min);
        if (info->note.max) fprintf(output, " max=\"%i\"", info->note.max);
        if (info->note.step) fprintf(output, " step=\"%i\"", info->note.step);
        fprintf(output, "/>\n");
    }

    if (info->aftertouch_mode) {
        fprintf(output, "  <aftertouch mode=\"%i\"/>\n", info->aftertouch_mode);
    }
    fprintf(output, " </info>\n\n");
}

void free_info(struct info_t *info)
{
    int i;

    assert(info);

    if (info->path) free(info->path);
    if (info->name) xmlFree(info->name);
    if (info->license) xmlFree(info->license);
    for (i=0; i<2; ++i) {
       if (info->copyright[i].by) xmlFree(info->copyright[i].by);
    }

}

struct dsp_t
{
    enum type_t dtype;
    char *type;
    char *src;
    int stereo;
    char *repeat;
    int optional;
    float release_factor;

    uint8_t no_slots;
    struct slot_t
    {
        struct param_t
        {
            float value;
            float pitch;
            float adjust;
        } param[4];
    } slot[4];
};

float fill_dsp(struct dsp_t *dsp, void *xid, enum type_t t, char timed_gain, float env_fact, char simplify)
{
    char *keep_volume = getenv("KEEP_VOLUME");
    unsigned int s, snum;
    float max = 0.0f;
    char dist = 0;
    char env = 0;
    void *xsid;

    dsp->dtype = t;
    dsp->type = lwrstr(xmlAttributeGetString(xid, "type"));
    if (!timed_gain && (!strcasecmp(dsp->type, "volume") || !strcasecmp(dsp->type, "timed-gain") || !strcasecmp(dsp->type, "dynamic-gain"))) {
        dsp->src = false_const;
    } else {
        dsp->src = lwrstr(xmlAttributeGetString(xid, "src"));
    }
    dsp->stereo = xmlAttributeGetBool(xid, "stereo");
    dsp->repeat = lwrstr(xmlAttributeGetString(xid, "repeat"));
    dsp->optional = xmlAttributeGetBool(xid, "optional");
    if (!strcasecmp(dsp->type, "timed-gain")) {
        dsp->release_factor = _MAX(xmlAttributeGetDouble(xid, "release-factor"), 0.0f);
        env = 1;
    } else if (!strcasecmp(dsp->type, "distortion")) {
        dist = 1;
    }

    xsid = xmlMarkId(xid);
    dsp->no_slots = snum = xmlNodeGetNum(xid, "slot");
    for (s=0; s<snum; s++)
    {
        if (xmlNodeGetPos(xid, xsid, "slot", s) != 0)
        {
            unsigned int p, pnum = xmlNodeGetNum(xsid, "param");
            void *xpid = xmlMarkId(xsid);
            int sn = s;

            if (xmlAttributeExists(xsid, "n")) {
                sn = _MINMAX(xmlAttributeGetInt(xsid, "n"), 0, 3);
            }

            for (p=0; p<pnum; p++)
            {
                if (xmlNodeGetPos(xsid, xpid, "param", p) != 0)
                {
                    float adjust, value, v;
                    int pn = p;

                    if (xmlAttributeExists(xpid, "n")) {
                        pn = _MINMAX(xmlAttributeGetInt(xpid, "n"), 0, 3);
                    }

                    dsp->slot[sn].param[pn].pitch = _MAX(xmlAttributeGetDouble(xpid, "pitch"), 0.0f);

                    if (dsp->slot[sn].param[pn].adjust == 0.0f) {
                       adjust = xmlAttributeGetDouble(xpid, "auto-sustain");
                    }

                    adjust = xmlAttributeGetDouble(xpid, "auto");
                    value = xmlGetDouble(xpid);
                    v = _MAX(value - adjust*_lin2log(220.0f), 0.01f);

                    if (env && (p % 2 == 0) && v > max) max = v;
//                  else if (dist && !p) max = -_lin2db(3.0f*powf(v, 1.f/3.5f));

                    if (simplify)
                    {
                        if (adjust) {
                            value = v;
                        }
#if 0
                        if (env && value > 1.0f && (pn % 2)) {
                            value = AAX_FPINFINITE;
                        }
#endif
                        adjust = 0.0f;
                    }

                    if (!keep_volume && env && (pn % 2) == 0)
                    {
                        adjust *= env_fact;
                        value *= env_fact;
                    }

                    dsp->slot[sn].param[pn].adjust = adjust;
                    dsp->slot[sn].param[pn].value = value;
                }
            }
            xmlFree(xpid);
        }
    }
    xmlFree(xsid);

    return max;
}

void print_dsp(struct dsp_t *dsp, struct info_t *info, FILE *output)
{
    unsigned int s, p;

    if (dsp->src == false_const) {
        return;
    }

    if (dsp->dtype == FILTER) {
        fprintf(output, "  <filter type=\"%s\"", dsp->type);
    } else {
        fprintf(output, "  <effect type=\"%s\"", dsp->type);
    }
    if (dsp->src && (strcmp(dsp->src, "12db") && strcmp(dsp->src, "true"))) {
        fprintf(output, " src=\"%s\"", dsp->src);
    }
    if (dsp->repeat) fprintf(output, " repeat=\"%s\"", dsp->repeat);
    if (dsp->stereo) fprintf(output, " stereo=\"true\"");
    if (dsp->optional) fprintf(output, " optional=\"true\"");
    if (dsp->release_factor > 0.1f) {
        fprintf(output, " release-factor=\"%s\"", format_float3(dsp->release_factor));
    }
    fprintf(output, ">\n");

    for(s=0; s<dsp->no_slots; ++s)
    {
        fprintf(output, "   <slot n=\"%i\">\n", s);
        for(p=0; p<4; ++p)
        {
            float adjust = dsp->slot[s].param[p].adjust;
            float pitch = dsp->slot[s].param[p].pitch;

            fprintf(output, "    <param n=\"%i\"", p);
            if (pitch)
            {
                fprintf(output, " pitch=\"%s\"", format_float3(pitch));
                dsp->slot[s].param[p].value = freq*pitch;
            }
            if (adjust) {
                fprintf(output, " auto=\"%s\"", format_float3(adjust));
            }

            fprintf(output, ">%s</param>", format_float3(dsp->slot[s].param[p].value));
            if (debug && adjust)
            {
               if (info->note.min && info->note.max)
               {
                   float freq1 = note2freq(info->note.min);
                   float freq2 = note2freq(info->note.max);
                   float value = dsp->slot[s].param[p].value;
                   float lin = _MAX(value - adjust*_lin2log(freq), 0.01f);
                   float lin1 = _MAX(value - adjust*_lin2log(freq1), 0.01f);
                   float lin2 = _MAX(value - adjust*_lin2log(freq2), 0.01f);
                   fprintf(output, "  <!-- %iHz: %s", (int)freq1, format_float3(lin1));
                   fprintf(output, " - %iHz: %s" , (int)freq2, format_float3(lin2));
                   fprintf(output, ", %iHz: %s -->\n", (int)freq, format_float3(lin));
               }
               else
               {
                   float value = dsp->slot[s].param[p].value;
                   float lin = _MAX(value - adjust*_lin2log(freq), 0.01f);
                   fprintf(output, "  <!-- %s -->", format_float3(lin));
               }
            }
            fprintf(output, "\n");
        }
        fprintf(output, "   </slot>\n");
    }

    if (dsp->dtype == FILTER) {
        fprintf(output, "  </filter>\n");
    } else {
        fprintf(output, "  </effect>\n");
    }
}

void free_dsp(struct dsp_t *dsp)
{
    if (dsp->type) xmlFree(dsp->type);
    if (dsp->repeat) xmlFree(dsp->repeat);
    if (dsp->src != false_const) xmlFree(dsp->src);
}

struct waveform_t
{
    char *src;
    char *processing;
    float ratio;
    float pitch;
    float staticity;
    int voices;
    float spread;
    char phasing;
};

char fill_waveform(struct waveform_t *wave, void *xid, char simplify)
{
    wave->src = lwrstr(xmlAttributeGetString(xid, "src"));
    wave->processing = lwrstr(xmlAttributeGetString(xid, "processing"));
    wave->ratio = xmlAttributeGetDouble(xid, "ratio");
    wave->pitch = _MAX(xmlAttributeGetDouble(xid, "pitch"), 0.0f);
    wave->staticity =_MINMAX(xmlAttributeGetDouble(xid, "staticity"),0.0f,1.0f);
    if (!simplify)
    {
        wave->voices = _MIN(abs(xmlAttributeGetInt(xid, "voices")), 9);
        wave->spread = _MINMAX(xmlAttributeGetDouble(xid, "spread"), 0.0f,1.0f);
        wave->phasing = xmlAttributeGetBool(xid, "phasing");
    }

    return strstr(wave->src, "noise") ? 1 : 0;
}

void print_waveform(struct waveform_t *wave, FILE *output)
{
    fprintf(output, "  <waveform src=\"%s\"", wave->src);
    if (wave->processing) fprintf(output, " processing=\"%s\"", wave->processing);
    if (wave->ratio) {
        if (wave->processing && !strcasecmp(wave->processing, "mix") && wave->ratio != 0.5f) {
            fprintf(output, " ratio=\"%s\"", format_float6(wave->ratio));
        } else if (wave->ratio != 1.0f) {
            fprintf(output, " ratio=\"%s\"", format_float6(wave->ratio));
        }
    }
    if (wave->pitch && wave->pitch != 1.0f) fprintf(output, " pitch=\"%s\"", format_float6(wave->pitch));
    if (wave->staticity > 0) fprintf(output, " staticity=\"%s\"", format_float3(wave->staticity));
    if (wave->voices > 1)
    {
        fprintf(output, " voices=\"%i\"", wave->voices);
        if (wave->spread) {
            fprintf(output, " spread=\"%s\"", format_float3(wave->spread));
            if (wave->phasing) fprintf(output, " phasing=\"true\"");
        }
    }
    fprintf(output, "/>\n");
}

void free_waveform(struct waveform_t *wave)
{
    if (wave->src) xmlFree(wave->src);
    if (wave->processing) xmlFree(wave->processing);
}

struct sound_t
{
    int mode;
    float gain, db;
    float frequency;
    float duration;
    int voices;
    float spread;
    char phasing;

    float loop_start;
    float loop_end;
    char *file;

    uint8_t no_entries;
    struct entry_t
    {
        enum type_t type;
        union
        {
            struct waveform_t waveform;
            struct dsp_t dsp;
        } slot;
    } entry[32];
};

void fill_sound(struct sound_t *sound, struct info_t *info, void *xid, float gain, float db, char simplify)
{
    unsigned int p, e, emax;
    char noise;
    void *xeid;

    if (!info->program && xmlAttributeExists(xid, "program")) {
        info->program = _MINMAX(xmlAttributeGetInt(xid, "program"), 0, 127);
    }
    if (!info->bank && xmlAttributeExists(xid, "bank")) {
        info->bank = _MINMAX(xmlAttributeGetInt(xid, "bank"), 0, 127);
    }
    if (!info->name && xmlAttributeExists(xid, "name")) {
        info->name = xmlAttributeGetString(xid, "name");
    }

    if (xmlAttributeExists(xid, "live")) {
        sound->mode = xmlAttributeGetBool(xid, "live") ? 1 : -1;
    } else if (xmlAttributeExists(xid, "mode")) {
        sound->mode = _MINMAX(xmlAttributeGetInt(xid, "mode"), 0, 2);
    }

    if (xmlAttributeExists(xid, "fixed-gain")) {
        sound->gain = -1.0*_MAX(xmlAttributeGetDouble(xid, "fixed-gain"), 0.0f);
    } else if (gain == 1.0f) {
        sound->gain = _MAX(xmlAttributeGetDouble(xid, "gain"), 0.0f);
    } else {
        sound->gain = gain;
    }
#if 0
    if (sound->db == -AAX_FPINFINITE) {
        sound->db = _MIN(xmlAttributeGetDouble(xid, "db"), 0.0f);
    } else {
        sound->db = db;
    }
#endif

    if (xmlAttributeExists(xid, "file")) {
        sound->file = xmlAttributeGetString(xid, "file");
    }
    sound->loop_start = xmlAttributeGetDouble(xid, "loop-start");
    if (xmlAttributeExists(xid, "loop-end")) {
        sound->loop_end = xmlAttributeGetDouble(xid, "loop-end");
    }

    sound->frequency = _MINMAX(xmlAttributeGetDouble(xid, "frequency"), 8.176f, 12543.854f);
    if (xmlAttributeGetDouble(xid, "duration")) {
        sound->duration = _MAX(xmlAttributeGetDouble(xid, "duration"), 0.0f);
    } else {
        sound->duration = 1.0f;
    }
    if (!simplify)
    {
        sound->voices = _MIN(abs(xmlAttributeGetInt(xid, "voices")), 9);
        sound->spread = _MINMAX(xmlAttributeGetDouble(xid, "spread"),0.0f,1.0f);
        sound->phasing = xmlAttributeGetBool(xid, "phasing");
    }

    p = 0;
    noise = 0;
    xeid = xmlMarkId(xid);
    emax = xmlNodeGetNum(xid, "*");
    for (e=0; e<emax; e++)
    {
        if (xmlNodeGetPos(xid, xeid, "*", e) != 0)
        {
            char *name = xmlNodeGetName(xeid);
            if (!strcasecmp(name, "waveform"))
            {
                sound->entry[p].type = WAVEFORM;
                noise |= fill_waveform(&sound->entry[p++].slot.waveform, xeid, simplify);
            }
            else if (!strcasecmp(name, "filter"))
            {
                sound->entry[p].type = FILTER;
                fill_dsp(&sound->entry[p++].slot.dsp, xeid, FILTER, 1, 1.0f, 0);
            }
            else if (!strcasecmp(name, "effect"))
            {
                sound->entry[p].type = EFFECT;
                fill_dsp(&sound->entry[p++].slot.dsp, xeid, EFFECT, 1, 1.0f, 0);
            }

            xmlFree(name);
        }
    }
    sound->no_entries = p;
    xmlFree(xeid);

    if (noise) sound->duration = _MAX(sound->duration, 0.3f);
}

void print_sound(struct sound_t *sound, struct info_t *info, FILE *output, char tmp)
{
    unsigned int e;

    fprintf(output, " <sound");
    if (sound->mode) {
        fprintf(output, " mode=\"%i\"", sound->mode);
    }

    if (sound->file)
    {
        if (tmp) {
            fprintf(output, " file=\"%s/%s\"", info->path, sound->file);
        } else {
            fprintf(output, " file=\"%s\"", sound->file);
        }
        if (sound->gain == 0.0f) {
            sound->gain = 1.0f;
        }
    }

    if (!tmp)
    {
       if (sound->gain < 0.0) {
           fprintf(output, " fixed-gain=\"%3.2f\"", -1.0*sound->gain);
       } else {
           fprintf(output, " gain=\"%3.2f\"", sound->gain);
       }
    }
    fprintf(output, " db=\"%3.1f\"", sound->db);

    if (sound->loop_start > 0) {
        fprintf(output, " loop-start=\"%g\"", sound->loop_start);
    }

    if (sound->loop_end > 0) {
        fprintf(output, " loop-start=\"%g\"", sound->loop_end);
    }

    if (sound->frequency)
    {
        uint8_t note;

        freq = sound->frequency;
        if (info->note.min && info->note.max)
        {
            note = _MINMAX(freq2note(freq), info->note.min, info->note.max);
            freq = sound->frequency = note2freq(note);
        }
        fprintf(output, " frequency=\"%g\"", sound->frequency);
    }

    if (sound->duration && sound->duration != 1.0f) {
        fprintf(output, " duration=\"%s\"", format_float3(sound->duration));
    }

    if (sound->voices > 1)
    {
        fprintf(output, " voices=\"%i\"", sound->voices);
        if (sound->spread) {
            fprintf(output, " spread=\"%s\"", format_float3(sound->spread));
            if (sound->phasing) fprintf(output, " phasing=\"true\"");
        }
    }
    fprintf(output, ">\n");

    for (e=0; e<sound->no_entries; ++e)
    {
        if (sound->entry[e].type == WAVEFORM) {
            print_waveform(&sound->entry[e].slot.waveform, output);
        } else {
            print_dsp(&sound->entry[e].slot.dsp, info, output);
        }
    }
    fprintf(output, " </sound>\n\n");
}

void free_sound(struct sound_t *sound)
{
    if (sound->file) free(sound->file);
    assert(sound);
}

struct object_t		// emitter and audioframe
{
    char *mode;
    int looping;
    float pan;

    uint8_t no_dsps;
    struct dsp_t dsp[16];
};

float fill_object(struct object_t *obj, void *xid, float env_fact, char timed_gain, char simplify)
{
    char emitter = (env_fact >= 0.0f);
    unsigned int p, d, dnum;
    float max = 0.0f;
    void *xdid;

    obj->mode = lwrstr(xmlAttributeGetString(xid, "mode"));
    obj->looping = xmlAttributeGetBool(xid, "looping");
    obj->pan = _MINMAX(xmlAttributeGetDouble(xid, "pan"), -1.0f, 1.0f);

    p = 0;
    xdid = xmlMarkId(xid);
    dnum = xmlNodeGetNum(xdid, "filter");
    for (d=0; d<dnum; d++)
    {
        if (xmlNodeGetPos(xid, xdid, "filter", d) != 0)
        {
            float m = fill_dsp(&obj->dsp[p++], xdid, FILTER, timed_gain, env_fact, simplify);
            if (!max) max = m;
        }
    }
    xmlFree(xdid);

    xdid = xmlMarkId(xid);
    dnum = xmlNodeGetNum(xdid, "effect");
    for (d=0; d<dnum; d++)
    {
        if (xmlNodeGetPos(xid, xdid, "effect", d) != 0)
        {
            char *type = lwrstr(xmlAttributeGetString(xdid, "type"));
            if (!simplify || (!emitter && (!strcasecmp(type, "distortion") ||
                                         !strcasecmp(type, "ringmodulator"))) ||
                             (emitter && !strcasecmp(type, "timed-pitch")))
            {
                float m = fill_dsp(&obj->dsp[p++], xdid, EFFECT, timed_gain, env_fact, simplify);
                if (!max) max = m;
            }
        }
    }
    xmlFree(xdid);
    obj->no_dsps = p;

    return max;
}

void print_object(struct object_t *obj, enum type_t type, struct info_t *info, FILE *output)
{
    unsigned int d;

    if (type == FRAME)
    {
//      if (!obj->no_dsps) return;
        fprintf(output, " <audioframe");
    }
    else {
        fprintf(output, " <emitter");
    }

    if (obj->mode) fprintf(output, " mode=\"%s\"", obj->mode);
    if (obj->looping) fprintf(output, " looping=\"true\"");
    if (type == FRAME && info->pan) {
        fprintf(output, " pan=\"%s\"", format_float3(-info->pan));
    }
    if (obj->pan) fprintf(output, " pan=\"%s\"", format_float3(obj->pan));

    if (obj->no_dsps)
    {
        fprintf(output, ">\n");

        for (d=0; d<obj->no_dsps; ++d) {
            print_dsp(&obj->dsp[d], info, output);
        }

        if (type == EMITTER) {
            fprintf(output, " </emitter>\n\n");
        } else {
            fprintf(output, " </audioframe>\n\n");
        }
    }
    else {
        fprintf(output, "/>\n\n");
    }
}

void free_object(struct object_t *obj)
{
    if (obj->mode) xmlFree(obj->mode);
}

struct aax_t
{
    struct info_t info;
    struct sound_t sound;
    struct object_t emitter;
    struct object_t audioframe;
};

void fill_aax(struct aax_t *aax, const char *filename, char simplify, float gain, float db, float env_fact, char timed_gain)
{
    void *xid;

    memset(aax, 0, sizeof(struct aax_t));
    xid = xmlOpen(filename);
    if (xid)
    {
        void *xaid = xmlNodeGet(xid, "/aeonwave");
        if (xaid)
        {
            void *xtid = xmlNodeGet(xaid, "instrument");
            if (!xtid) xtid = xmlNodeGet(xaid, "info");
            if (xtid)
            {
                fill_info(&aax->info, xtid, filename);
                xmlFree(xtid);
            }

            xtid = xmlNodeGet(xaid, "sound");
            if (xtid)
            {
                fill_sound(&aax->sound, &aax->info, xtid, gain, db, simplify);
                xmlFree(xtid);
            }

            xtid = xmlNodeGet(xaid, "emitter");
            if (xtid)
            {
                float m = fill_object(&aax->emitter, xtid, env_fact, timed_gain, simplify);
                if (m > 0) aax->sound.db = _lin2db(1.0f/m);
                xmlFree(xtid);
            }

            xtid = xmlNodeGet(xaid, "audioframe");
            if (xtid)
            {
                float m = fill_object(&aax->audioframe, xtid, -1.f, timed_gain, simplify);
                if (m < 0) aax->sound.db -= m;
                xmlFree(xtid);
            }

            xmlFree(xaid);
        }
        else {
            printf("%s does not seem to be AAXS compatible.\n", filename);
        }
        xmlClose(xid);
    }
    else {
        printf("%s not found.\n", filename);
    }
}

void print_aax(struct aax_t *aax, const char *outfile, char commons, char tmp)
{
    FILE *output;
    struct tm* tm_info;
    time_t timer;
    char year[5];

    time(&timer);
    tm_info = localtime(&timer);
    strftime(year, 5, "%Y", tm_info);

    if (outfile)
    {
        output = fopen(outfile, "w");
        testForError(output, "Output file could not be created.");
    }
    else {
        output = stdout;
    }

    fprintf(output, "<?xml version=\"1.0\"?>\n\n");

    fprintf(output, "<!--\n");
    fprintf(output, " * Copyright (C) 2017-%s by Erik Hofman.\n", year);
    fprintf(output, " * Copyright (C) 2017-%s by Adalin B.V.\n", year);
    fprintf(output, " * All rights reserved.\n");
    if ((!aax->info.license || (commons & 0x80) || !strcmp(aax->info.license, "Attribution-ShareAlike 4.0 International")) && commons)
    {
        fprintf(output, " *\n");
        fprintf(output, " * This file is part of AeonWave and covered by the\n");
        fprintf(output, " * Creative Commons Attribution-ShareAlike 4.0 International Public License\n");
        fprintf(output, " * https://creativecommons.org/licenses/by-sa/4.0/legalcode\n");
    }
    else
    {
        fprintf(output, " *\n");
        fprintf(output, " * This is UNPUBLISHED PROPRIETARY SOURCE CODE; the contents of this file may\n");
        fprintf(output, " * not be disclosed to third parties, copied or duplicated in any form, in\n");
        fprintf(output, " * whole or in part, without the prior written permission of the author.\n");
    }
    fprintf(output, "-->\n\n");

    fprintf(output, "<aeonwave>\n\n");
    print_info(&aax->info, output, commons);
    print_sound(&aax->sound, &aax->info, output, tmp);
    print_object(&aax->emitter, EMITTER, &aax->info, output);
    print_object(&aax->audioframe, FRAME, &aax->info, output);
    fprintf(output, "</aeonwave>\n");

    if (outfile) {
        fclose(output);
    }
}

void free_aax(struct aax_t *aax)
{
    free_info(&aax->info);
    free_sound(&aax->sound);
    free_object(&aax->emitter);
    free_object(&aax->audioframe);
}

void help()
{
    printf("aaxsstandardize version %i.%i.%i\n\n", AAX_UTILS_MAJOR_VERSION,
                                                    AAX_UTILS_MINOR_VERSION,
                                                    AAX_UTILS_MICRO_VERSION);
    printf("Usage: aaxsstandardize [options]\n");
    printf("Reads a user generated .aaxs configuration file and outputs a\n");
    printf("standardized version of the file.\n");

    printf("\nOptions:\n");
    printf(" -i, --input <file>\t\tthe .aaxs configuration file to standardize.\n");
    printf(" -o, --output <file>\t\twrite the new .aaxs configuration to this file.\n");
    printf("     --debug\t\t\tAdd some debug information to the AAXS file.\n");
    printf("     --omit-cc-by\t\tDo not add the CC-BY license reference.\n");
    printf("     --force-cc-by\t\tForce the CC-BY license reference.\n");
    printf("     --force-simplify\t\tForce simplification of the configuration file.\n");
    printf(" -h, --help\t\t\tprint this message and exit\n");

    printf("\nWhen no output file is specified then stdout will be used.\n");

    printf("\n");
    exit(-1);
}

int main(int argc, char **argv)
{
    char *infile, *outfile;
    char simplify = 0;
    char commons = 1;

    if (argc == 1 || getCommandLineOption(argc, argv, "-h") ||
                    getCommandLineOption(argc, argv, "--help"))
    {
        help();
    }

    if (getCommandLineOption(argc, argv, "--omit-cc-by")) {
        commons = 0;
    } else if (getCommandLineOption(argc, argv, "--force-cc-by")) {
        commons |= 0x80;
    }

    if (getCommandLineOption(argc, argv, "--force-simplify")) {
        simplify = 1;
    }

    if (getCommandLineOption(argc, argv, "--debug")) {
        debug = 1;
    }

    infile = getInputFile(argc, argv, NULL);
    outfile = getOutputFile(argc, argv, NULL);
    if (infile)
    {
        char tmpfile[128], aaxsfile[128];
        float dt, step, gain, env_fact;
        double loudness, peak;
        float fval, db;
        struct aax_t aax;
        aaxBuffer buffer;
        aaxConfig config;
        aaxEmitter emitter;
        aaxFrame frame;
        void **data;
        char *ptr;
        int res;

        ptr = strrchr(infile, PATH_SEPARATOR);
        if (!ptr) ptr = infile;
        else ptr++;
        snprintf(aaxsfile, 120, "%s/%s.aaxs", TEMP_DIR, ptr);
        snprintf(tmpfile, 120, "AeonWave on Audio Files: %s/%s.wav", TEMP_DIR, ptr);

        /* mixer */
        config = aaxDriverOpenByName(tmpfile, AAX_MODE_WRITE_STEREO);
        testForError(config, "unable to open the temporary file.");

        res = aaxMixerSetSetup(config, AAX_FORMAT, AAX_FLOAT);
        testForState(res, "aaxMixerSetSetup, format");

        res = aaxMixerSetSetup(config, AAX_TRACKS, 1);
        testForState(res, "aaxMixerSetSetup, no_tracks");

        res = aaxMixerSetSetup(config, AAX_REFRESH_RATE, 90);
        testForState(res, "aaxMixerSetSetup, refresh rate");

        res = aaxMixerSetState(config, AAX_INITIALIZED);
        testForState(res, "aaxMixerInit");

        fill_aax(&aax, infile, simplify, 1.0f, 1.0f, -AAX_FPINFINITE, 0);
        print_aax(&aax, aaxsfile, commons, 1);
        gain = aax.sound.gain;
        db = aax.sound.db;
        free_aax(&aax);

        /* buffer */
        buffer = aaxBufferReadFromStream(config, aaxsfile);
        testForError(buffer, "Unable to create a buffer from an aaxs file.");

        /* emitter */
        emitter = aaxEmitterCreate();
        testForError(emitter, "Unable to create a new emitter");

        res = aaxEmitterAddBuffer(emitter, buffer);
        testForState(res, "aaxEmitterAddBuffer");

        /* frame */
        frame = aaxAudioFrameCreate(config);
        testForError(frame, "Unable to create a new audio frame");

        res = aaxMixerRegisterAudioFrame(config, frame);
        testForState(res, "aaxMixerRegisterAudioFrame");

        res = aaxAudioFrameRegisterEmitter(frame, emitter);
        testForState(res, "aaxAudioFrameRegisterEmitter");

        res = aaxAudioFrameAddBuffer(frame, buffer);

        /* playback */
        res = aaxEmitterSetState(emitter, AAX_PLAYING);
        testForState(res, "aaxEmitterStart");

        res = aaxAudioFrameSetState(frame, AAX_PLAYING);
        testForState(res, "aaxAudioFrameStart");

        res = aaxMixerSetState(config, AAX_PLAYING);
        testForState(res, "aaxMixerStart");

        dt = 0.0f;
        step = 1.0f/aaxMixerGetSetup(config, AAX_REFRESH_RATE);
        do
        {
            aaxMixerSetState(config, AAX_UPDATE);
            dt += step;
        }
        while (dt < 2.5f && aaxEmitterGetState(emitter) == AAX_PLAYING);

        res = aaxEmitterSetState(emitter, AAX_SUSPENDED);
        testForState(res, "aaxEmitterStop");

        res = aaxAudioFrameSetState(frame, AAX_STOPPED);
        res = aaxAudioFrameDeregisterEmitter(frame, emitter);
        res = aaxMixerDeregisterAudioFrame(config, frame);
        res = aaxMixerSetState(config, AAX_STOPPED);
        res = aaxAudioFrameDestroy(frame);
        res = aaxEmitterDestroy(emitter);
        res = aaxBufferDestroy(buffer);

        res = aaxDriverClose(config);
        res = aaxDriverDestroy(config);

        config = aaxDriverOpenByName("None", AAX_MODE_WRITE_STEREO);
        testForError(config, "No default audio device available.");

        snprintf(tmpfile, 120, "%s/%s.wav", TEMP_DIR, ptr);
        buffer = aaxBufferReadFromStream(config, tmpfile);
        testForError(buffer, "Unable to read the buffer.");

        peak = loudness = 0.0;
        aaxBufferSetSetup(buffer, AAX_FORMAT, AAX_FLOAT);
        data = aaxBufferGetData(buffer);
        if (data)
        {
            float *bdata = data[0];
            size_t no_samples = aaxBufferGetSetup(buffer, AAX_NO_SAMPLES);
#if HAVE_EBUR128
            size_t tracks = aaxBufferGetSetup(buffer, AAX_TRACKS);
            size_t freq = aaxBufferGetSetup(buffer, AAX_FREQUENCY);
            ebur128_state *st;

            st = ebur128_init(tracks, freq, EBUR128_MODE_I|EBUR128_MODE_SAMPLE_PEAK);
            if (st)
            {
                ebur128_add_frames_float(st, bdata, no_samples);
                ebur128_loudness_global(st, &loudness);
                ebur128_sample_peak(st, 0, &peak);
                ebur128_destroy(&st);
                loudness = _db2lin(loudness);
            }
#endif
#if 0
            double rms_total = 0.0;
            size_t j = no_samples;
            do
            {
                float samp = (float)*bdata++;
                rms_total += samp*samp;
            }
            while (--j);

            db = _lin2db(sqrt(rms_total/no_samples));
#endif
            aaxFree(data);
        }
        aaxBufferDestroy(buffer);

        aaxDriverClose(config);
        aaxDriverDestroy(config);

        fval = 6.0f*_MAX(peak, 0.1f)*(_db2lin(-24.0f)/loudness);

        printf("%-32s: peak: % -3.1f, R128: % -3.1f", infile, peak, loudness);
        printf(", new gain: %4.1f\n", (gain > 0.0f) ? fval : -gain);

        env_fact = 1.0f;
        if (gain > 0.0f && fabsf(gain-fval) > 0.1f)
        {
            env_fact = gain/fval;
            gain = fval;
        }
        env_fact *= getGain(argc, argv);
        fill_aax(&aax, infile, simplify, gain, db, env_fact, 1);
        print_aax(&aax, outfile, commons, 0);
        free_aax(&aax);

        remove(aaxsfile);
        remove(tmpfile);
    }
    else {
        help();
    }

    return 0;
}
