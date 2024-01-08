/*
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 *                          WITH Universal-FOSS-exception-1.0
 */

#pragma once

#include <map>
#include <cmath>
#include <random>
#include <memory>
#include <utility>
#include <algorithm>

#include <aax/aeonwave.hpp>

namespace aax
{

namespace note
{
static float volume = 0.5f;
static float pan_levels = 128.0f;
static float distance = 2.0f;
}; // namespace aax

namespace math
{
inline float note2freq(float note_no, float base_freq=440.0f) {
    return base_freq*powf(2.0f, (note_no-69.0f)/12.0f);
}
inline int freq2note(float freq, float base_freq=440.0f) {
   return rintf(12*(logf(freq/base_freq)/log(2))+57);
}

}; // namespace math


class Panning
{
public:
    Panning() = default;

    virtual ~Panning() = default;

    Panning(const Panning&) = delete;
    Panning(Panning&&) = delete;

    Panning& operator=(const Panning&) = delete;
    Panning& operator=(Panning&&) = delete;

    float get() { return pan; }
    void set(float p, bool init=false) {
        pan = p;
        panned = !init;

        if (!init && abs(wide) > 0) {
            p = floorf(pan*wide)/abs(wide);
        }
        if (p != 0.0f) {
            p *= spread;
            int idx = floorf(p * note::pan_levels);
            auto it = matrices.find(idx);
            if (it != matrices.end()) {
                mtx = it->second;
            } else {
                Matrix64 m;
                m.rotate(-1.57*p, 0.0, 1.0, 0.0);
                m.multiply(mtx_panned);
                matrices[idx] = m;
                mtx = m;
            }
        } else {
            mtx = mtx_panned;
        }
    }

public:
    std::map<int,Matrix64> matrices;
    Vector at = Vector(0.0f, 0.0f, -1.0f);
    Vector up = Vector(0.0f, 1.0f, 0.0f);
    Vector64 pos = Vector64(0.0, 1.0, -note::distance);
    Matrix64 mtx_panned = Matrix64(pos, at, up);
    Matrix64 mtx = mtx::identity;
    float spread = 1.0f;
    float pan = 0.0f;
    int wide = 0;
    bool panned = false;
};

class Note : public Emitter
{
public:
    Note(float f, float p, Panning& pan)
        : Emitter(pan.wide ? AAX_ABSOLUTE : AAX_RELATIVE),
          frequency(f), pitch(p)
    {
        pitch = p; set_pitch();
        tie(pitch_param, AAX_PITCH_EFFECT, AAX_PITCH);

        tie(volume_param, AAX_VOLUME_FILTER, AAX_GAIN);
        if (pan.wide) {
            // pitch*frequency ranges from: 8 - 12544 Hz,
            // log(20) = 1.3, log(12544) = 4.1
            // p = 0.0f .. 1.0f
            float p = (math::lin2log(pitch*frequency) - 1.3f)/2.8f;
            p = floorf(-2.0f*(p-0.5f) * note::pan_levels)/note::pan_levels;
            if (p != pan_prev) {
                pan.set(p, true);
                Emitter::matrix(pan.mtx);
                pan_prev = p;
            }
        } else {
            Emitter::matrix(mtx::identity);
        }

        aax::dsp dsp = Emitter::get(AAX_VOLUME_FILTER);
        dsp.set(AAX_MAX_GAIN, 2.56f);
        Emitter::set(dsp);
    }

    Note() = delete;

    virtual ~Note() = default;

    Note(const Note&) = delete;
    Note(Note&&) = delete;

    Note& operator=(const Note&) = delete;
    Note& operator=(Note&&) = delete;

    void matrix(Matrix64& m) {
        Emitter::set(AAX_POSITION, AAX_ABSOLUTE);
        Emitter::matrix(m);
    }

    bool play(float velocity, float start_pitch=1.0f, float time=0.0f) {
        hold = false;
        if (time > 0.0f && start_pitch != pitch) {
           aax::dsp dsp = Emitter::get(AAX_PITCH_EFFECT);
           dsp.set(AAX_PITCH_START, start_pitch);
           dsp.set(AAX_PITCH_RATE, time);
           dsp.set(true|AAX_ENVELOPE_FOLLOW);
           Emitter::set(dsp);
        }
        Emitter::set(AAX_INITIALIZED);
        Emitter::set(AAX_MIDI_ATTACK_VELOCITY_FACTOR, 127.0f*velocity);
        if (!playing) playing = Emitter::set(AAX_PLAYING);
        return playing;
    }

    bool stop(float velocity=1.0f) {
        playing = false;
        Emitter::set(AAX_MIDI_RELEASE_VELOCITY_FACTOR, 127.0f*velocity);
        return hold ? true : Emitter::set(AAX_STOPPED);
    }

    bool finish(void) {
        hold = false; playing = false;
        return Emitter::set(AAX_STOPPED);
    }

    bool finished(void) {
        aaxState s = Emitter::state();
        return (s == AAX_PROCESSED || s == AAX_INITIALIZED);
    }

    bool stopped(void) {
        aaxState s = Emitter::state();
        return (s != AAX_PLAYING);
    }

    // current_notes hold until hold becomes false, even after a stop message.
    // already stopped current_notes can be caught by hold again.
    void set_hold(bool h) {
        if (!h && hold) Emitter::set(AAX_STOPPED);
        else if (h && Emitter::state() == AAX_STOPPED) {
            Emitter::set(AAX_PLAYING);
        }
        hold = h;
    }

    // only current_notes started before this command should hold until stop arrives
    void set_sustain(bool s) { hold = s; }

    void set_soft(float soft) {
        Emitter::set(AAX_MIDI_SOFT_FACTOR, 127.0f*soft);
    }
    void set_pressure(float pressure) {
        Emitter::set(AAX_MIDI_PRESSURE_FACTOR, 127.0f*pressure);
    }
    void set_pitch(float b) {
        pitch_bend = b; set_pitch();
    }

    // envelope control
    void set_attack_time(int t) { set(AAX_MIDI_ATTACK_FACTOR, t); }
    void set_release_time(int t) { set(AAX_MIDI_RELEASE_FACTOR,t); }
    void set_decay_time(int t) { set(AAX_MIDI_DECAY_FACTOR, t); }

    bool buffer(Buffer& buffer) {
        Emitter::remove_buffer();
        return Emitter::add(buffer);
    }

private:
    void set_pitch() {
        pitch_param = pitch*pitch_bend;
    }

    Param volume_param = note::volume;
    Param pitch_param = 1.0f;

    bool playing = false;
    bool hold = true;

    float pan_prev = -1000.0f;
    float pitch_bend = 1.0f;

    float frequency;
    float pitch;
};


class Instrument : public Mixer
{
private:
    using note_t = std::shared_ptr<Note>;

public:
    Instrument(AeonWave& ptr, Buffer& buf, bool drums=false, int wide=0, bool panned=true)
        : Mixer(ptr), aax(ptr), buffer(buf), is_drum_channel(drums)
    {
        pan.wide = wide;

        tie(volume, AAX_VOLUME_FILTER, AAX_GAIN);

        tie(vibrato_freq, AAX_DYNAMIC_PITCH_EFFECT, AAX_LFO_FREQUENCY);
        tie(vibrato_depth, AAX_DYNAMIC_PITCH_EFFECT, AAX_LFO_DEPTH);
        tie(vibrato_state, AAX_DYNAMIC_PITCH_EFFECT);

        tie(tremolo_freq, AAX_DYNAMIC_GAIN_FILTER, AAX_LFO_FREQUENCY);
        tie(tremolo_depth, AAX_DYNAMIC_GAIN_FILTER, AAX_LFO_DEPTH);
        tie(tremolo_offset, AAX_DYNAMIC_GAIN_FILTER, AAX_LFO_OFFSET);
        tie(tremolo_state, AAX_DYNAMIC_GAIN_FILTER);

        tie(freqfilter_cutoff, AAX_FREQUENCY_FILTER, AAX_CUTOFF_FREQUENCY);
        tie(freqfilter_resonance, AAX_FREQUENCY_FILTER, AAX_RESONANCE);
        tie(freqfilter_state, AAX_FREQUENCY_FILTER);

        tie(chorus_level, AAX_CHORUS_EFFECT, AAX_DELAY_GAIN);
        tie(chorus_depth, AAX_CHORUS_EFFECT, AAX_LFO_OFFSET);
        tie(chorus_rate, AAX_CHORUS_EFFECT, AAX_LFO_FREQUENCY);
        tie(chorus_feedback, AAX_CHORUS_EFFECT, AAX_FEEDBACK_GAIN);
        tie(chorus_cutoff, AAX_CHORUS_EFFECT, AAX_DELAY_CUTOFF_FREQUENCY);
        tie(chorus_state, AAX_CHORUS_EFFECT);

        tie(delay_level, AAX_DELAY_EFFECT, AAX_DELAY_GAIN);
        tie(delay_depth, AAX_DELAY_EFFECT, AAX_LFO_OFFSET);
        tie(delay_rate, AAX_DELAY_EFFECT, AAX_LFO_FREQUENCY);
        tie(delay_feedback, AAX_DELAY_EFFECT, AAX_FEEDBACK_GAIN);
        tie(delay_cutoff, AAX_DELAY_EFFECT, AAX_DELAY_CUTOFF_FREQUENCY);
        tie(delay_state, AAX_DELAY_EFFECT);

        tie(reverb_level, AAX_REVERB_EFFECT, AAX_DECAY_LEVEL);
        tie(reverb_delay_depth, AAX_REVERB_EFFECT, AAX_DELAY_DEPTH);
        tie(reverb_decay_level, AAX_REVERB_EFFECT, AAX_DECAY_LEVEL);
        tie(reverb_decay_depth, AAX_REVERB_EFFECT, AAX_DECAY_DEPTH);
        tie(reverb_cutoff, AAX_REVERB_EFFECT, AAX_CUTOFF_FREQUENCY);
        tie(reverb_state, AAX_REVERB_EFFECT);

        if (panned) {
            Mixer::matrix(pan.mtx_panned);
        } else {
            Mixer::matrix(mtx::identity);
        }
        Mixer::set(AAX_POSITION, AAX_RELATIVE);
        Mixer::set(AAX_PLAYING);
        if (is_drum_channel) {
            Mixer::set(AAX_MONO_EMITTERS, 10);
        }
    }

    Instrument() = delete;

    virtual ~Instrument() = default;

    Instrument(const Instrument&) = delete;
    Instrument(Instrument&&) = delete;

    Instrument& operator=(const Instrument&) = delete;
    Instrument& operator=(Instrument&&) = delete;

    void finish(void) { note_finish(); }
    bool finished(void) { return note_finished(); }

    // It's tempting to store the instrument buffer as a class parameter
    // but drums require a different buffer for every note_no
    void play(int note_no, float velocity, Buffer& buffer, float pitch=1.0f) {
        note_no = get_note(note_no);
        if (!is_drum_channel) {
            pitch *= buffer.get_pitch(aax::math::note2freq(note_no));
        }
        if (monophonic || legato) {
            auto it = note.find(note_prev);
            if (it != note.end()) it->second->stop();
            note_prev = note_no;
        }
        note_t current_note;
        auto it = note.find(note_no);
        if (it != note.end()) {
            current_note = it->second;
            if (request_note_finish && !current_note->finished()) {
                current_note->finish();
                note_stopped[note_no] = std::move(note.at(note_no));
                note.erase(note_no);
                it = note.end();
            }
        }
        if (it == note.end()) {
            float buffer_frequency = buffer.get(AAX_BASE_FREQUENCY);
            Note *n = new Note(buffer_frequency,pitch,pan);
            auto ret = note.insert({note_no,
                               note_t(n, [this](Note *n) { Mixer::remove(*n); })
                                  });
            current_note = ret.first->second;
            if (!playing && !is_drum_channel) {
                Mixer::add(buffer);
                playing = true;
            }
            if (is_drum_channel && !pan.panned) current_note->matrix(pan.mtx_panned);
            else if (pan.panned && abs(pan.wide) > 1) current_note->matrix(pan.mtx);
            current_note->buffer(buffer);
        }

        Mixer::add(*current_note);
        current_note->set_soft(soft);
        current_note->set_attack_time(attack_time);
        current_note->set_release_time(release_time);
        current_note->set_decay_time(decay_time);
        current_note->play(velocity, pitch_start, slide_state ? transition_time : 0.0f);
        pitch_start = pitch;
        for (auto it = note_stopped.begin(), next = it; it != note_stopped.end();
             it = next)
        {
            ++next;
            if (it->second->finished()) {
                note_stopped.erase(it);
            }
        }
    }

    // So support both
    void play(int note_no, float velocity, float pitch=1.0f) {
        note_play(note_no, velocity, pitch);
    }

    void stop(int note_no, float velocity=0) {
       note_stop(get_note(note_no), velocity);
    }

    void set_pitch(float pitch) {
        note_set_pitch(pitch);
    }
    void set_pitch(int note_no, float pitch) {
        note_set_pitch(get_note(note_no), pitch);
    }

    void set_pressure(float p) { note_set_pressure(p); }
    void set_pressure(int note_no, float p) {
        note_set_pressure(get_note(note_no), p);
    }

    void set_soft(float s) { note_set_soft(s); }
    void set_pan(float p) { note_set_pan(p); }
    void set_pos(Matrix64& m) { note_set_pos(m); }
    void set_hold(bool h) { note_set_hold(h); }
    void set_hold(int note_no, bool h) { note_set_hold(get_note(note_no), h); }
    void set_sustain(bool s) { note_set_sustain(s); }
    void set_attack_time(unsigned t) { note_set_attack_time(t); }
    void set_release_time(unsigned t) { note_set_release_time(t); }
    void set_decay_time(unsigned t) { note_set_decay_time(t); }

    void set_note_finish(bool finish) { request_note_finish = finish; }
    void set_monophonic(bool m) { if (!is_drum_channel) monophonic = m; }
    void set_gain(float v) {
        gain = v; set_volume();
    }
    void set_expression(float e) {
        expression = e; set_volume();
    }
    void set_modulation(float m) {
        if (!is_drum_channel) {
            bool enabled = (m != 0.0f);
            vibrato_depth = m; tremolo_depth = m; tremolo_offset - 1.0f - m;
            if (enabled)
            {
                if (!vibrato_state) {
                    vibrato_state = tremolo_state = AAX_SINE;
                }
            } else if (vibrato_state) {
                vibrato_state = tremolo_state = false;
            }
        }
    }

    void set_pitch_start(float p) {
        if (!is_drum_channel) { pitch_start = p; }
    }
    void set_pitch_slide_state(bool s) {
        if (!is_drum_channel) { slide_state = s; }
    }
    void set_pitch_transition_time(float t) {
        if (!is_drum_channel) { transition_time = t; }
    }

    void set_pitch_depth(float s) { pitch_depth = s; }
    float get_pitch_depth() { return pitch_depth; }

    void set_tuning_coarse(float s) { note_tuning_coarse(s); }
    float get_tuning_coarse() { return coarse_tuning; }

    void set_tuning_fine(float s) { note_tuning_fine(s); }
    float get_tuning_fine() { return fine_tuning; }

    void set_celeste_depth(float level) { note_celeste_depth(level); }
    float get_celeste_depth() { return detune; }

    void set_modulation_depth(float d) { note_modulation_depth(d); }
    float get_modulation_depth() { return modulation_range; }

    bool get_pressure_volume_bend() { return pressure_volume_bend; }
    bool get_pressure_pitch_bend() { return pressure_pitch_bend; }
    float get_aftertouch_sensitivity() { return pressure_sensitivity; }

    void set_vibrato_rate(float r) {}
    void set_vibrato_depth(float d) {}
    void set_vibrato_delay(float d) {}

    void set_tremolo_depth(float d) {}
    void set_legato(bool l) { legato = l; }

    void set_spread(float s = 1.0f) { pan.spread = s; }
    void set_wide(int s = 1) { pan.wide = s; }
    void set_drums(bool d = true) { is_drum_channel = d; }

    // The whole device must have one chorus effect and one reverb effect.
    // Each Channel must have its own adjustable send levels to the chorus
    // and the reverb. A connection from chorus to reverb must be provided.
    void set_chorus(Buffer& buf) {
        Mixer::add(buf);
    }
    void set_chorus_level(float lvl) {
        if ((chorus_level = lvl) > 0) {
            if (!chorus_state) chorus_state = true;
        } else if (chorus_state) chorus_state = false;
    }

    void set_chorus_depth(float depth) { chorus_depth = depth; }
    void set_chorus_rate(float rate) { chorus_rate = rate; }
    void set_chorus_feedback(float fb) { chorus_feedback = fb; }
    void set_chorus_cutoff(float fc) { chorus_cutoff = fc; }
    void set_phaser_depth(float d) {}

    void set_delay(Buffer& buf) {
        Mixer::add(buf);
    }
    void set_delay_level(float lvl) {
        if ((delay_level = lvl) > 0) {
            if (!delay_state) delay_state = AAX_EFFECT_1ST_ORDER;
        } else if (delay_state) delay_state = false;
    }

    void set_delay_depth(float depth) { delay_depth = depth; }
    void set_delay_rate(float rate) { delay_rate = rate; }
    void set_delay_feedback(float fb) { delay_feedback = fb; }
    void set_delay_cutoff(float fc) { delay_cutoff = fc; }

    void set_reverb(Buffer& buf) {
        Mixer::add(buf);
        aax::dsp dsp = Mixer::get(AAX_REVERB_EFFECT);
        reverb_decay_level = dsp.get(AAX_DECAY_LEVEL);
    }
    void set_reverb_level(float lvl) {
        if (lvl > 1e-5f) {
            reverb_level = lvl*reverb_decay_level;
            if (!reverb_state) reverb_state = AAX_EFFECT_1ST_ORDER;
        } else if (reverb_state) reverb_state = false;
    }
    void set_reverb_cutoff(float fc) { reverb_cutoff = fc; }
    void set_reverb_delay_depth(float v) { reverb_delay_depth = v; }
    void set_reverb_decay_level(float v) { reverb_decay_level = v; }
    void set_reverb_decay_depth(float v) { reverb_decay_depth = v; }
    void set_reverb_time_rt60(float v) {
        reverb_decay_level = powf(math::level_60dB, 0.5f*reverb_decay_depth/v);
    }

    void set_filter_cutoff(float dfc) {
        cutoff = dfc; set_filter_cutoff();
        if (!freqfilter_state) freqfilter_state = true;
    }

    void set_filter_resonance(float dQ) {
        freqfilter_resonance = Q + dQ;
        if (!freqfilter_state) freqfilter_state = true;
    }

    float get_gain() { return gain; }
    float get_spread(void) { return pan.spread; }
    int get_wide(void) { return pan.wide; }
    bool is_drums() { return is_drum_channel; }

    float get_chorus_level() { return chorus_level; }
    float get_delay_level() { return delay_level; }
    float get_reverb_level() { return reverb_level/reverb_decay_level; }

    Buffer& get_buffer() { return buffer; }

protected:
    virtual void note_finish(void) {
        for (auto& it : note) it.second->stop();
    }

    virtual bool note_finished(void) {
        for (auto& it : note) {
            if (!it.second->finished()) return false;
        }
        return true;
    }

    virtual void note_play(int note_no, float velocity, float pitch) {
        play(note_no, velocity, buffer, pitch);
    }

    virtual void note_stop(int note_no, float velocity=0) {
        if (!legato) {
            auto it = note.find(note_no);
            if (it != note.end()) {
                it->second->stop(velocity);
            }
        }
    }

    virtual void note_set_pitch(float pitch) {
        for (auto& it : note) it.second->set_pitch(pitch);
    }

    virtual void note_set_pitch(int note_no, float pitch) {
        auto it = note.find(note_no);
        if (it != note.end()) {
            it->second->set_pitch(pitch);
        }
    }

    virtual void note_tuning_coarse(float s) { coarse_tuning = s; }
    virtual void note_tuning_fine(float s) { fine_tuning = s; }
    virtual void note_celeste_depth(float level) { detune = level; }
    virtual void note_modulation_depth(float d) { modulation_range = d; }

    virtual void note_set_soft(float s) {
        // sitch between 1.0f (non-soft) and 0.707f (soft)
        soft = (!is_drum_channel) ? 1.0f - 0.293f*s : 1.0f;
        set_filter_cutoff();
        for (auto& it : note) it.second->set_soft(soft);
    }

    virtual void note_set_pressure(float p) {
        for (auto& it : note) it.second->set_pressure(p);
    }

    virtual void note_set_pressure(int note_no, float p) {
        auto it = note.find(note_no);
        if (it != note.end()) {
            it->second->set_pressure(p);
        }
    }

    virtual void note_set_pan(float p) {
        p = floorf(p * note::pan_levels)/note::pan_levels;
        if (p != pan_prev) {
            pan.set(p);
            note_set_pos(pan.mtx);
            pan_prev = p;
        }
    }

    virtual void note_set_pos(Matrix64& m) {
        pan.mtx = m;
        if (is_drum_channel || pan.wide) {
            for (auto& it : note) it.second->matrix(pan.mtx);
        } else {
            Mixer::matrix(pan.mtx);
        }
    }

    virtual void note_set_hold(int note_no, bool h) {
       auto it = note.find(note_no);
       if (it != note.end()) {
            it->second->set_hold(h);
        }
    }

    virtual void note_set_hold(bool h) {
        for (auto& it : note) it.second->set_hold(h);
    }

    virtual void note_set_sustain(bool s) {
        if (!is_drum_channel) {
            for (auto& it : note) it.second->set_sustain(s);
        }
    }

    virtual void note_set_attack_time(unsigned t) {
        if (!is_drum_channel) { attack_time = t;
            for (auto& it : note) it.second->set_attack_time(t);
        }
    }
    virtual void note_set_release_time(unsigned t) {
        if (!is_drum_channel) { release_time = t;
            for (auto& it : note) it.second->set_release_time(t);
        }
    }
    virtual void note_set_decay_time(unsigned t) {
        if (!is_drum_channel) { decay_time = t;
            for (auto& it : note) it.second->set_decay_time(t);
        }
    }

    void set_filter_cutoff() {
        freqfilter_cutoff = soft * math::log2lin(cutoff*fc);
    }
    void set_volume() {
        volume = gain*expression;
    }

private:
    Param volume = 1.0f;

    Param vibrato_freq = 5.0f;
    Param vibrato_depth = 0.0f;
    Status vibrato_state = false;

    Param tremolo_freq = 5.0f;
    Param tremolo_depth = 0.0f;
    Param tremolo_offset = 1.0f;
    Status tremolo_state = false;

    Param freqfilter_cutoff = 2048.0f;
    Param freqfilter_resonance = 1.0f;
    Param freqfilter_state = false;

    Param chorus_rate = 0.0f;
    Param chorus_level = 0.0f;
    Param chorus_feedback = 0.0f;
    Param chorus_cutoff = 22050.0f;
    Param chorus_depth = Param(1900.0f, AAX_MICROSECONDS);
    Status chorus_state = false;

    Param delay_rate = 0.0f;
    Param delay_level = 0.0f;
    Param delay_feedback = 0.0f;
    Param delay_cutoff = 22050.0f;
    Param delay_depth = Param(1900.0f, AAX_MICROSECONDS);
    Status delay_state = false;

    Param reverb_level = 40.0f/127.0f;
    Param reverb_delay_depth = 0.035f;
    Param reverb_decay_level = 0.0f;
    Param reverb_decay_depth = 0.0f;
    Param reverb_cutoff = 22000.0f;
    Status reverb_state = false;

protected:
    int get_note(int note_no) {
        if (is_drum_channel) return note_no;
        return note_no + coarse_tuning;
    }

    AeonWave& aax;
    Buffer& buffer;

    Panning pan;

    unsigned attack_time = 64;
    unsigned release_time = 64;
    unsigned decay_time = 64;

//  float modulation_freq = 1.5f;
//  float modulation_range = 1.0f;

    float cutoff = 1.0f;
    float fc = math::lin2log(float(freqfilter_cutoff));
    float Q = float(freqfilter_resonance);

    float soft = 1.0f;
    float gain = 1.0f;
    float expression = 1.0f;

    float pan_prev = 0.0f;

    float detune = 0.0f;
    float pitch_depth = 2.0f;
    float coarse_tuning = 0.0f;
    float fine_tuning = 0.0f;
    float modulation_range = 2.0f;
    float pressure_sensitivity = 1.0f;

    float transition_time = 0.0f;
    float pitch_start = 1.0f;
    int note_prev = 0;

    bool is_drum_channel = false;
    bool monophonic = false;
    bool playing = false;
    bool slide_state = false;
    bool legato = false;

    bool pressure_volume_bend = true;
    bool pressure_pitch_bend = false;

    bool request_note_finish = false;
    std::map<uint32_t,note_t> note_stopped;
    std::map<uint32_t,note_t> note;
};


/*
 * An Essemble represents a group of instruments playing in unison.
 * This could be, for example, a string quartet or a brass section.
 *
 * The audio-frame related filters and effects like the volume, chorus, delay,
 * frequency filter, and reverb, as well as panning, are handled by this class
 * instead of being handled by every instrument individually.
 */
class Ensemble : public Instrument
{
private:
    struct member_t {
        member_t(Ensemble* e, Instrument *i, float p, float g, int n=0, int m=128)
          : ensemble(e), instrument(std::unique_ptr<Instrument>(i)),
            min_note(n), max_note(m), pitch(p), gain(g)
        {
            ensemble->add(*instrument);
        }

        ~member_t() {
            ensemble->remove(*instrument);
        }

        Ensemble* ensemble;
        std::unique_ptr<Instrument> instrument;
        int min_note;
        int max_note;
        float pitch;
        float gain;
    };

    Ensemble(const Ensemble&) = delete;
    Ensemble& operator=(const Ensemble&) = delete;

public:
    Ensemble(AeonWave& ptr, Buffer& buf, bool drums=false, int wide=0)
        : Instrument(ptr, buf, drums, wide), m_mt((std::random_device())())
    {}

    Ensemble(AeonWave& ptr, bool drums=false, int wide=0) :
        Ensemble(ptr, aax::nullBuffer, drums, wide)
    {}

    Ensemble() = delete;

    virtual ~Ensemble() = default;

    Ensemble(Ensemble&&) = default;
    Ensemble& operator=(Ensemble&&) = default;

    void add_member(Buffer& buf, float pitch, float gain, int min, int max)
    {
        std::uniform_real_distribution<> dis(0.995f*pitch, pitch);
        if (member.size()) pitch = dis(m_mt);
        Instrument *i = new Instrument(aax, buf, is_drum_channel, pan.wide, false);
        member_t *m = new member_t(this, i, pitch, gain, min, max);
        member.emplace_back(m);
    }

    void add_member(Buffer& buf, float pitch, float gain) {
        add_member(buf, pitch, gain, 0, 128);
    }
    void add_member(Buffer& buf, float pitch, int min=0, int max=128) {
        add_member(buf, pitch, 1.0f, min, max);
    }
    void add_member(Buffer& buf, int min=0, int max=128) {
        add_member(buf, 1.0f, 1.0f, min, max);
    }

    int no_members() { return member.size(); }

private:
    std::vector<std::unique_ptr<member_t>> member;
    std::mt19937 m_mt;

    void note_play(int note_no, float velocity, float pitch)
    {
        for(int i=0; i<member.size(); ++i) {
            auto& m = member[i];
            if (note_no >= m->min_note && note_no < m->max_note) {
                m->instrument->play(note_no, velocity, pitch*m->pitch);
            }
        }
    }

    void note_stop(int note_no, float velocity) {
        if (!member.size()) {
            Instrument::note_stop(note_no, velocity);
        } else if (!legato) {
            for(int i=0; i<member.size(); ++i) {
                member[i]->instrument->stop(note_no, velocity);
            }
        }
    }

    void note_finish(void) {
        if (!member.size()) {
            Instrument::note_finish();
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->finish();
        }
    }

    bool note_finished(void) {
        if (!member.size()) {
            return Instrument::note_finished();
        }
        for(int i=0; i<member.size(); ++i) {
            if (!member[i]->instrument->finished()) return false;
        }
        return true;
    }

    void note_set_pitch(float pitch) {
        if (!member.size()) {
            Instrument::note_set_pitch(pitch);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_pitch(pitch);
        }
    }

    void note_set_pitch(int note_no, float pitch) {
        if (!member.size()) {
            Instrument::note_set_pitch(note_no, pitch);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_pitch(note_no, pitch);
        }
    }

    void note_tuning_coarse(float s) {
        if (!member.size()) {
            Instrument::note_tuning_coarse(s);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_tuning_coarse(s);
        }
    }
    void note_tuning_fine(float s) {
        if (!member.size()) {
            Instrument::note_tuning_fine(s);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_tuning_fine(s);
        }
    }
    void note_celeste_depth(float level) {
        if (!member.size()) {
            Instrument::note_celeste_depth(level);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_celeste_depth(level);
        }
    }
    void note_modulation_depth(float d) {
        if (!member.size()) {
            Instrument::note_modulation_depth(d);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_modulation_depth(d);
        }
     }

    void note_set_soft(float s) {
        if (!member.size()) {
            Instrument::note_set_soft(s);
        } else {
            // sitch between 1.0f (non-soft) and 0.707f (soft)
            soft = (!is_drum_channel) ? 1.0f - 0.293f*s : 1.0f;
            set_filter_cutoff();
            for(int i=0; i<member.size(); ++i) {
                member[i]->instrument->set_soft(s);
            }
        }
    }

    void note_set_pressure(float p) {
        if (!member.size()) {
            Instrument::note_set_pressure(p);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_pressure(p);
        }
    }

    void note_set_pressure(int note_no, float p) {
        if (!member.size()) {
            Instrument::note_set_pressure(note_no, p);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_pressure(note_no, p);
        }
    }

    void note_set_pan(float p) {
        if (!member.size()) {
            Instrument::note_set_pan(p);
        } else {
            p = floorf(p * note::pan_levels)/note::pan_levels;
            if (p != pan_prev) {
                pan.set(p);
                if (is_drum_channel || pan.wide) {
                    for(int i=0; i<member.size(); ++i) {
                        member[i]->instrument->set_pos(pan.mtx);
                    }
                } else {
                    Mixer::matrix(pan.mtx);
                }
                pan_prev = p;
            }
        }
    }

    void note_set_hold(int note_no, bool h) {
        if (!member.size()) {
            Instrument::note_set_hold(note_no, h);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_hold(note_no, h);
        }
    }

    void note_set_hold(bool h) {
        if (!member.size()) {
            Instrument::note_set_hold(h);
        } else for(int i=0; i<member.size(); ++i) {
            member[i]->instrument->set_hold(h);
        }
    }

    void note_set_sustain(bool s) {
        if (!member.size()) {
            Instrument::note_set_sustain(s);
        } else if (!is_drum_channel) {
            for(int i=0; i<member.size(); ++i) {
                member[i]->instrument->set_sustain(s);
            }
        }
    }

    void note_set_attack_time(unsigned t) {
        if (!member.size()) {
            Instrument::note_set_attack_time(t);
        } else if (!is_drum_channel) { attack_time = t;
            for(int i=0; i<member.size(); ++i) {
                member[i]->instrument->set_attack_time(t);
            }
        }
    }
    void note_set_release_time(unsigned t) {
        if (!member.size()) {
            Instrument::note_set_release_time(t);
        } else if (!is_drum_channel) { release_time = t;
            for(int i=0; i<member.size(); ++i) {
                member[i]->instrument->set_release_time(t);
            }
        }
    }
    void note_set_decay_time(unsigned t) {
        if (!member.size()) {
            Instrument::note_set_decay_time(t);
        } else if (!is_drum_channel) { decay_time = t;
            for(int i=0; i<member.size(); ++i) {
                member[i]->instrument->set_decay_time(t);
            }
        }
    }
};

} // namespace aax


