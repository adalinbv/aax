
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <xml.h>
#include <aax/aax.h>

#include "driver.h"
#include "wavfile.h"

static float freq = 22.0f;
static FILE *output;

enum type_t
{
   WAVEFORM = 0,
   FILTER,
   EFFECT,
   EMITTER,
   FRAME
};

const char* format_float(float f)
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

struct instrument_t
{
    uint8_t program;
    uint8_t bank;
    const char* name;

    struct note_t
    {
        uint8_t polyphony;
        uint8_t min, max, step;
    } note;

     struct position_t {
        double x, y, z;
    } position;
};

void fill_instrument(struct instrument_t *inst, void *xid)
{
    void *xtid;

    inst->program = xmlAttributeGetInt(xid, "program");
    inst->bank = xmlAttributeGetInt(xid, "bank");
    inst->name = xmlAttributeGetString(xid, "name");

    xtid = xmlNodeGet(xid, "note");
    if (xtid)
    {
        inst->note.polyphony = xmlAttributeGetInt(xtid, "polyphony");
        inst->note.min = xmlAttributeGetInt(xtid, "min");
        inst->note.max = xmlAttributeGetInt(xtid, "max");
        inst->note.step = xmlAttributeGetInt(xtid, "step");
        xmlFree(xtid);
    }

    xtid = xmlNodeGet(xid, "position");
    if (xtid)
    {
        inst->position.x = xmlAttributeGetDouble(xtid, "x");
        inst->position.y = xmlAttributeGetDouble(xtid, "y");
        inst->position.z = xmlAttributeGetDouble(xtid, "z");
        xmlFree(xtid);
    }
}

void print_instrument(struct instrument_t *inst)
{
    fprintf(output, " <instrument");
    fprintf(output, " program=\"%i\"", inst->program);
    fprintf(output, " bank=\"%i\"", inst->bank);
    if (inst->name) fprintf(output, " name=\"%s\"", inst->name);
    fprintf(output, ">\n");

    fprintf(output, "  <note");
    if (inst->note.polyphony) fprintf(output, " polyphony=\"%i\"", inst->note.polyphony);
    if (inst->note.min) fprintf(output, " min=\"%i\"", inst->note.min);
    if (inst->note.max) fprintf(output, " max=\"%i\"", inst->note.max);
    if (inst->note.step) fprintf(output, " min=\"%i\"", inst->note.step);
    fprintf(output, "/>\n");

    if (inst->position.x && inst->position.y && inst->position.z) {
        fprintf(output, "  <position x=\"%3.f\" y=\"%3.1f\" z=\"%3.1f\"/>",
                  inst->position.x, inst->position.y, inst->position.z);
    };

    fprintf(output, " </instrument>\n\n");
}

struct dsp_t
{
    enum type_t dtype;
    const char *type;
    const char *src;
    int stereo;
    int repeat;
    int optional;

    uint8_t no_slots;
    struct slot_t
    {
        struct param_t
        {
           float value;
           float pitch;
           float sustain;
        } param[4];
    } slot[4];
};

void fill_dsp(struct dsp_t *dsp, void *xid, enum type_t t)
{
    unsigned int s, snum;
    void *xsid;

    dsp->dtype = t;
    dsp->type = xmlAttributeGetString(xid, "type");
    dsp->src = xmlAttributeGetString(xid, "src");
    dsp->stereo = xmlAttributeGetInt(xid, "stereo");
    dsp->repeat = xmlAttributeGetInt(xid, "repeat");
    dsp->optional = xmlAttributeGetBool(xid, "optional");

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
                sn = xmlAttributeGetInt(xsid, "n");
            }

            for (p=0; p<pnum; p++)
            {
                if (xmlNodeGetPos(xsid, xpid, "param", p) != 0)
                {
                    int pn = p;

                    if (xmlAttributeExists(xpid, "n")) {
                        pn = xmlAttributeGetInt(xpid, "n");
                    }

                    dsp->slot[sn].param[pn].value = xmlGetDouble(xpid);
                    dsp->slot[sn].param[pn].pitch = xmlAttributeGetDouble(xpid, "pitch");
                    dsp->slot[sn].param[pn].sustain = xmlAttributeGetDouble(xpid, "auto-sustain");
                }
            }
            xmlFree(xpid);
        }
    }
    xmlFree(xsid);
}

void print_dsp(struct dsp_t *dsp)
{
   unsigned int s, p;

   if (dsp->dtype == FILTER) {
      fprintf(output, "  <filter type=\"%s\"", dsp->type);
   } else {
      fprintf(output, "  <effect type=\"%s\"", dsp->type);
   }
   if (dsp->src) fprintf(output, " src=\"%s\"", dsp->src);
   if (dsp->repeat) fprintf(output, " repeat=\"%i\"", dsp->repeat);
   if (dsp->stereo) fprintf(output, " stereo=\"true\"");
   if (dsp->optional) fprintf(output, " optional=\"true\"");
   fprintf(output, ">\n");

   for(s=0; s<dsp->no_slots; ++s)
   {
       fprintf(output, "   <slot n=\"%i\">\n", s);
       for(p=0; p<4; ++p)
       {
           float sustain = dsp->slot[s].param[p].sustain;
           float pitch = dsp->slot[s].param[p].pitch;

           fprintf(output, "    <param n=\"%i\"", p);
           if (pitch && pitch != 1.0f)
           {
               fprintf(output, " pitch=\"%s\"", format_float(pitch));
               dsp->slot[s].param[p].value = freq*pitch;
           }
           if (sustain) {
               fprintf(output, " auto-sustain=\"%s\"", format_float(sustain));
           }

           fprintf(output, ">%s</param>\n", format_float(dsp->slot[s].param[p].value));
       }
       fprintf(output, "   </slot>\n");
   }

   if (dsp->dtype == FILTER) {
      fprintf(output, "  </filter>\n");
   } else {
      fprintf(output, "  </effect>\n");
   }
}

struct waveform_t
{
    const char *src;
    const char *processing;
    float ratio;
    float pitch;
    int voices;
    float spread;
};

void fill_waveform(struct waveform_t *wave, void *xid)
{
    wave->src = xmlAttributeGetString(xid, "src");
    wave->processing = xmlAttributeGetString(xid, "processing");
    wave->ratio = xmlAttributeGetDouble(xid, "ratio");
    wave->pitch = xmlAttributeGetDouble(xid, "pitch");
    wave->voices = xmlAttributeGetInt(xid, "voices");
    wave->spread = xmlAttributeGetDouble(xid, "spread");
}

void print_waveform(struct waveform_t *wave)
{
    fprintf(output, "  <waveform src=\"%s\"", wave->src);
    if (wave->processing) fprintf(output, " processing=\"%s\"", wave->processing);
    if (wave->ratio) fprintf(output, " ratio=\"%s\"", format_float(wave->ratio));
    if (wave->pitch && wave->pitch != 1.0f) fprintf(output, " pitch=\"%s\"", format_float(wave->pitch));
    if (wave->voices)
    {
        fprintf(output, " voices=\"%i\"", wave->voices);
        if (wave->spread) fprintf(output, " spread=\"%2.1f\"", wave->spread);
    }
    fprintf(output, "/>\n");
}

struct sound_t
{
    float gain;
    int frequency;
    float duration;
    int voices;
    float spread;

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

void fill_sound(struct sound_t *sound, void *xid, float gain)
{
    unsigned int p, e, emax;
    void *xeid;

    sound->gain = gain; // xmlAttributeGetDouble(xid, "gain");
    sound->frequency = xmlAttributeGetInt(xid, "frequency");
    sound->duration = xmlAttributeGetDouble(xid, "duration");
    sound->voices = xmlAttributeGetInt(xid, "voices");
    sound->spread = xmlAttributeGetDouble(xid, "spread");

    p = 0;
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
                fill_waveform(&sound->entry[p++].slot.waveform, xeid);
            }
            else if (!strcasecmp(name, "filter"))
            {
                sound->entry[p].type = FILTER;
                fill_dsp(&sound->entry[p++].slot.dsp, xeid, FILTER);
            }
            else if (!strcasecmp(name, "filter"))
            {
                sound->entry[p].type = EFFECT;
                fill_dsp(&sound->entry[p++].slot.dsp, xeid, EFFECT);
            }

            xmlFree(name);
        }
    }
    sound->no_entries = p;
    xmlFree(xeid);
}

void print_sound(struct sound_t *sound)
{
    unsigned int e;

    fprintf(output, " <sound");
    if (sound->gain) fprintf(output, " gain=\"%3.2f\"", sound->gain);
    if (sound->frequency)
    {
        freq = sound->frequency;
        fprintf(output, " frequency=\"%i\"", sound->frequency);
    }
    if (sound->duration) fprintf(output, " duration=\"%s\"", format_float(sound->duration));
    if (sound->voices)
    {
        fprintf(output, " voices=\"%i\"", sound->voices);
        if (sound->spread) fprintf(output, " spread=\"%2.1f\"", sound->spread);
    }
    fprintf(output, ">\n");

    for (e=0; e<sound->no_entries; ++e)
    {
        if (sound->entry[e].type == WAVEFORM) {
            print_waveform(&sound->entry[e].slot.waveform);
        } else {
            print_dsp(&sound->entry[e].slot.dsp);
        }
    }
    fprintf(output, " </sound>\n\n");
}

struct object_t		// emitter and audioframe
{
    const char *mode;
    int looping;

    uint8_t no_dsps;
    struct dsp_t dsp[16];
};

void fill_object(struct object_t *obj, void *xid)
{
    unsigned int p, d, dnum;
    void *xdid;

    obj->mode = xmlAttributeGetString(xid, "mode");
    obj->looping = xmlAttributeGetBool(xid, "looping");

    p = 0;
    xdid = xmlMarkId(xid);
    dnum = xmlNodeGetNum(xdid, "filter");
    for (d=0; d<dnum; d++)
    {
        if (xmlNodeGetPos(xid, xdid, "filter", d) != 0) {
            fill_dsp(&obj->dsp[p++], xdid, FILTER);
        }
    }
    xmlFree(xdid);

    xdid = xmlMarkId(xid);
    dnum = xmlNodeGetNum(xdid, "effect");
    for (d=0; d<dnum; d++)
    {
        if (xmlNodeGetPos(xid, xdid, "effect", d) != 0) {
            fill_dsp(&obj->dsp[p++], xdid, EFFECT);
        }
    }
    xmlFree(xdid);
    obj->no_dsps = p;
}

void print_object(struct object_t *obj, enum type_t type)
{
    unsigned int d;

    if (type == FRAME)
    {
        if (!obj->no_dsps) return;
        fprintf(output, " <audioframe");
    }
    else {
        fprintf(output, " <emitter");
    }

    if (obj->mode) fprintf(output, " mode=\"%s\"", obj->mode);
    if (obj->looping) fprintf(output, " looping=\"true\"");

    if (obj->no_dsps)
    {
        fprintf(output, ">\n");

        for (d=0; d<obj->no_dsps; ++d) {
            print_dsp(&obj->dsp[d]);
        }

        if (type == EMITTER) {
            fprintf(output, " </emitter>\n\n");
        } else {
            fprintf(output, " </audioframe>\n");
        }
    }
    else {
        fprintf(output, "/>\n\n");
    }
}

struct aax_t
{
    struct instrument_t instrument;
    struct sound_t sound;
    struct object_t emitter;
    struct object_t audioframe;
};

void fill_aax(struct aax_t *aax, const char *filename, float gain)
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
            if (xtid)
            {
                fill_instrument(&aax->instrument, xtid);
                xmlFree(xtid);
            }

            xtid = xmlNodeGet(xaid, "sound");
            if (xtid)
            {
                fill_sound(&aax->sound, xtid, gain);
                xmlFree(xtid);
            }

            xtid = xmlNodeGet(xaid, "emitter");
            if (xtid)
            {
                fill_object(&aax->emitter, xtid);
                xmlFree(xtid);
            }

            xtid = xmlNodeGet(xaid, "audioframe");
            if (xtid)
            {
                fill_object(&aax->audioframe, xtid);
                xmlFree(xtid);
            }

            xmlFree(xaid);
        }
        else {
            fprintf(output, "%s does not seem to be AAXS compatible.\n", filename);
        }
        xmlClose(xid);
    }
    else {
        fprintf(output, "%s not found.\n", filename);
    }
}

void print_aax(struct aax_t *aax)
{
    struct tm* tm_info;
    time_t timer;
    char year[5];

    time(&timer);
    tm_info = localtime(&timer);
    strftime(year, 5, "%Y", tm_info);

    fprintf(output, "<?xml version=\"1.0\"?>\n\n");

    fprintf(output, "<!--\n");
    fprintf(output, " * Copyright (C) 2017-%s by Erik Hofman.\n", year);
    fprintf(output, " * Copyright (C) 2017-%s by Adalin B.V.\n", year);
    fprintf(output, " * All rights reserved.\n");
    fprintf(output, "-->\n\n");

    fprintf(output, "<aeonwave>\n");
    print_instrument(&aax->instrument);
    print_sound(&aax->sound);
    print_object(&aax->emitter, EMITTER);
    print_object(&aax->audioframe, FRAME);
    fprintf(output, "</aeonwave>\n");
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
    printf("  -i, --input <file>\t\tthe .aaxs configuration file to standardize.\n");
    printf("  -o, --output <file>\t\twrite the new .aaxs configuration to this file.\n");
    printf("  -h, --help\t\t\tprint this message and exit\n");

    printf("\nWhen no output file is specified then stdout will be used.\n");

    printf("\n");
    exit(-1);
}

int main(int argc, char **argv)
{
    char *infile, *outfile;
    aaxConfig config;

    if (argc == 1 || getCommandLineOption(argc, argv, "-h") ||
                     getCommandLineOption(argc, argv, "--help"))
    {
        help();
    }

    outfile = getOutputFile(argc, argv, NULL);
    if (outfile)
    {
        output = fopen(outfile, "w");
        testForError(output, "Output file could not be created.");
    }
    else {
        output = stdout;
    }

    config = aaxDriverOpenDefault(AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    infile = getInputFile(argc, argv, NULL);
    outfile = getOutputFile(argc, argv, NULL);
    if (infile)
    {
        struct aax_t aax;
        aaxBuffer buffer;
        float rms;

        buffer = bufferFromFile(config, infile);
        testForError(buffer, "Unable to create a buffer");

        rms = (float)aaxBufferGetSetup(buffer, AAX_AVERAGE_VALUE);
        fill_aax(&aax, infile, 838860.8f/rms);
        print_aax(&aax);
    }
    else {
        help();
    }

    aaxDriverClose(config);
    aaxDriverDestroy(config);

    if (output != stdout) {
        fclose(output);
    }

    return 0;
}
