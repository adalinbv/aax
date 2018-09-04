
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <xml.h>
#include <aax/aax.h>

#include "driver.h"
#include "wavfile.h"

enum type_t
{
   WAVEFORM = 0,
   FILTER,
   EFFECT,
   EMITTER,
   FRAME
};

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
    printf(" <instrument");
    printf(" program=\"%i\"", inst->program);
    printf(" bank=\"%i\"", inst->bank);
    if (inst->name) printf(" name=\"%s\"", inst->name);
    printf(">\n");

    printf("  <note");
    if (inst->note.polyphony) printf(" polyphony=\"%i\"", inst->note.polyphony);
    if (inst->note.min) printf(" min=\"%i\"", inst->note.min);
    if (inst->note.max) printf(" max=\"%i\"", inst->note.max);
    if (inst->note.step) printf(" min=\"%i\"", inst->note.step);
    printf("/>\n");

    if (inst->position.x && inst->position.y && inst->position.z) {
        printf("  <position x=\"%3.f\" y=\"%3.1f\" z=\"%3.1f\"/>",
                  inst->position.x, inst->position.y, inst->position.z);
    };

    printf(" </instrument>\n\n");
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
      printf("  <filter type=\"%s\"", dsp->type);
   } else {
      printf("  <effect type=\"%s\"", dsp->type);
   }
   if (dsp->src) printf(" src=\"%s\"", dsp->src);
   if (dsp->repeat) printf(" repeat=\"%i\"", dsp->repeat);
   if (dsp->stereo) printf(" stereo=\"true\"");
   if (dsp->optional) printf(" optional=\"true\"");
   printf(">\n");

   for(s=0; s<dsp->no_slots; ++s)
   {
       printf("   <slot n=\"%i\">\n", s);
       for(p=0; p<4; ++p)
       {
           char buf[32];

           printf("    <param n=\"%i\"", p);
           if (dsp->slot[s].param[p].pitch) {
               printf(" pitch=\"%3.2f\"", dsp->slot[s].param[p].pitch);
           }
           if (dsp->slot[s].param[p].sustain) {
               printf(" auto-sustain=\"%3.2f\"", dsp->slot[s].param[p].sustain);
           }

           if (dsp->slot[s].param[p].value > 1.0f) {
               sprintf(buf, "%.1f", dsp->slot[s].param[p].value);
           } else {
               sprintf(buf, "%.3g", dsp->slot[s].param[p].value);
           }
           if (!strchr(buf, '.')) {
               strcat(buf, ".0");
           }
           printf(">%s</param>\n", buf);
       }
       printf("   </slot>\n");
   }

   if (dsp->dtype == FILTER) {
      printf("  </filter>\n");
   } else {
      printf("  </effect>\n");
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
    printf("  <waveform src=\"%s\"", wave->src);
    if (wave->processing) printf(" processing=\"%s\"", wave->processing);
    if (wave->ratio) printf(" ratio=\"%3.2f\"", wave->ratio);
    if (wave->pitch) printf(" pitch=\"%3.2f\"", wave->pitch);
    if (wave->voices)
    {
        printf(" voices=\"%i\"", wave->voices);
        if (wave->spread) printf(" spread=\"%2.1f\"", wave->spread);
    }
    printf("/>\n");
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

void fill_sound(struct sound_t *sound, void *xid)
{
    unsigned int p, e, emax;
    void *xeid;

    sound->gain = xmlAttributeGetDouble(xid, "gain");
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

    printf(" <sound");
    if (sound->gain) printf(" gain=\"%3.2f\"", sound->gain);
    if (sound->frequency) printf(" frequency=\"%i\"", sound->frequency);
    if (sound->duration) printf(" duration=\"%3.2f\"", sound->duration);
    if (sound->voices)
    {
        printf(" voices=\"%i\"", sound->voices);
        if (sound->spread) printf(" spread=\"%2.1f\"", sound->spread);
    }
    printf(">\n");

    for (e=0; e<sound->no_entries; ++e)
    {
        if (sound->entry[e].type == WAVEFORM) {
            print_waveform(&sound->entry[e].slot.waveform);
        } else {
            print_dsp(&sound->entry[e].slot.dsp);
        }
    }
    printf(" </sound>\n\n");
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

    if (type == EMITTER) {
        printf(" <emitter");
    } else {
        printf(" <audioframe");
    }

    if (obj->mode) printf(" mode=\"%s\"", obj->mode);
    if (obj->looping) printf(" looping=\"true\"");
    printf(">\n");

    for (d=0; d<obj->no_dsps; ++d) {
        print_dsp(&obj->dsp[d]);
    }
    if (type == EMITTER) {
        printf(" </emitter>\n\n");
    } else {
        printf(" </audioframe>\n");
    }
}

struct aax_t
{
    struct instrument_t instrument;
    struct sound_t sound;
    struct object_t emitter;
    struct object_t audioframe;
};

void fill_aax(struct aax_t *aax, const char *filename)
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
                fill_sound(&aax->sound, xtid);
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
            printf("%s does not seem to be AAXS compatible.\n", filename);
        }
        xmlClose(xid);
    }
    else {
        printf("%s not found.\n", filename);
    }
}

void print_aax(struct aax_t *aax)
{
    printf("<?xml version=\"1.0\"?>\n");
    printf("<aeonwave>\n");
    print_instrument(&aax->instrument);
    print_sound(&aax->sound);
    print_object(&aax->emitter, EMITTER);
    print_object(&aax->audioframe, FRAME);
    printf("</aeonwave>\n");
}

int main(int argc, char **argv)
{
    aaxConfig config;
    char *infile;
    int res;

    config = aaxDriverOpenDefault(AAX_MODE_WRITE_STEREO);
    testForError(config, "No default audio device available.");

    infile = getInputFile(argc, argv, NULL);
    if (infile)
    {
        struct aax_t aax;
        aaxBuffer buffer;
        float rms, peak;
        float gain;

        buffer = bufferFromFile(config, infile);
        testForError(buffer, "Unable to create a buffer");

        rms = (float)aaxBufferGetSetup(buffer, AAX_AVERAGE_VALUE)/8388608.0f;
        peak = (float)aaxBufferGetSetup(buffer, AAX_PEAK_VALUE)/8388608.0f;
        gain = 1.0f/rms;
//      printf("%s\n", infile);
//      printf("    peak: %f, rms: %f, gain=\"%3.2f\"\n", peak, rms, gain);

        fill_aax(&aax, infile);
        print_aax(&aax);
    }

    res = aaxDriverClose(config);
    res = aaxDriverDestroy(config);

    return 0;
}
