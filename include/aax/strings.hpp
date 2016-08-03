/*
 * Copyright (C) 2015-2016 by Erik Hofman.
 * Copyright (C) 2015-2016 by Adalin B.V.
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
 *    3. Neither the name of Adalin B.V. nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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

#ifndef AAX_STRINGS_HPP
#define AAX_STRINGS_HPP 1

#include <aax/aax.h>

namespace aax
{

inline std::string to_string(enum aaxHandleType handle)
{
    switch(handle)
    {
    case AAX_CONFIG: return "audio driver";
    case AAX_BUFFER: return "audio buffer";
    case AAX_EMITTER: return "sound emitter";
    case AAX_AUDIOFRAME: return "audio frame";
    case AAX_FILTER: return "sound filter";
    case AAX_EFFECT: return "sound effect";
    case AAX_CONFIG_HD: return "HD audio driver";
    }
}

inline std::string to_string(enum aaxFormat fmt)
{
    switch(fmt)
    {
    case AAX_PCM8S: return "signed, 8-bits per sample";
    case AAX_PCM16S: return "signed, 16-bits per sample";
    case AAX_PCM24S: return "signed, 24-bits per sample 32-bit encoded";
    case AAX_PCM32S: return "signed, 32-bits per sample";
    case AAX_FLOAT: return "32-bit floating point, -1.0 to 1.0";
    case AAX_DOUBLE: return "64-bit floating point, -1.0 to 1.0";
    case AAX_MULAW: return "16-bit compressed to 8-bit µ-law";
    case AAX_ALAW: return "16-bit compresed to 8-bit A-law";
    case AAX_IMA4_ADPCM: return "16-bit compressed to 4-bit";
    case AAX_PCM8U: return "unsigned, 8-bits per sample";
    case AAX_PCM16U: return "unsigned, 16-bits per sample";
    case AAX_PCM24U: return "unsigned, 24-bits per sample 32-bit encoded";
    case AAX_PCM32U: return "unsigned, 32-bits per sample";
    case AAX_PCM16S_LE: return "signed, 16-bits per sample little-endian";
    case AAX_PCM24S_LE: return "signed, 24-bits per sample little-endian 32-bit encoded";
    case AAX_PCM32S_LE: return "signed, 32-bits per sample little-endian";
    case AAX_FLOAT_LE: return "32-bit floating point, -1.0 to 1.0 little-endian";
    case AAX_DOUBLE_LE: return "64-bit floating point, -1.0 to 1.0 little-endian";
    case AAX_PCM16U_LE: return "unsigned, 16-bits per sample little-endian";
    case AAX_PCM24U_LE: return "unsigned, 24-bits per sample little-endian 32-bit encoded";
    case AAX_PCM32U_LE: return "unsigned, 32-bits per sample little-endian";
    case AAX_PCM16S_BE: return "signed, 16-bits per sample big-endian";
    case AAX_PCM24S_BE: return "signed, 24-bits per sample big-endian 32-bit encoded";
    case AAX_PCM32S_BE: return "signed, 32-bits per sample big-endian";
    case AAX_FLOAT_BE: return "32-bit floating point, -1.0 to 1.0 big-endian";
    case AAX_DOUBLE_BE: return "64-bit floating point, -1.0 to 1.0 big-endian";
    case AAX_PCM16U_BE: return "unsigned, 16-bits per sample big-endian";
    case AAX_PCM24U_BE: return "unsigned, 24-bits per sample big-endian 32-bit encoded";
    case AAX_PCM32U_BE: return "unsigned, 32-bits per sample big-endian";
    case AAX_AAXS16S: return "16-bit XML synthesized waveform";
    case AAX_AAXS24S: return "24-bit XML synthesized waveform 32-bit encoded";
    }
}

inline std::string to_string(enum aaxType type)
{
    switch(type)
    {
    case AAX_LINEAR: return "linear";
    case AAX_DECIBEL: return "Decibels";
    case AAX_RADIANS: return "radians";
    case AAX_DEGREES: return "degrees";
    case AAX_BYTES: return "bytes";
    case AAX_FRAMES: return "frames";
    case AAX_SAMPLES: return "samples";
    case AAX_MICROSECONDS: return "µ-seconds";
    }
}

inline std::string to_string(enum aaxModeType mode)
{
    switch(mode)
    {
    case AAX_POSITION: return "position";
    case AAX_LOOPING: return "looping";
    case AAX_BUFFER_TRACK: return "buffer track";
    }
}

inline std::string to_string(enum aaxTrackType type)
{
    switch(type)
    {
    case AAX_TRACK_MIX: return "mix tracks";
    case AAX_TRACK_ALL: return "all tracks";
    case AAX_TRACK_FRONT_LEFT: return "front left";
    case AAX_TRACK_FRONT_RIGHT: return "front right";
    case AAX_TRACK_REAR_LEFT: return "rear left";
    case AAX_TRACK_REAR_RIGHT: return "rear right";
    case AAX_TRACK_CENTER_FRONT: return "center front";
    case AAX_TRACK_SUBWOOFER: return "sub-woofer (low frequency emitter)";
    case AAX_TRACK_SIDE_LEFT: return "side left";
    case AAX_TRACK_SIDE_RIGHT: return "side right";
    }
}

inline std::string to_string(enum aaxSetupType type)
{
    switch(type)
    {
    case AAX_DRIVER_STRING: return "driver name string";
    case AAX_VERSION_STRING: return "version string";
    case AAX_RENDERER_STRING: return "renderer string";
    case AAX_VENDOR_STRING: return "vendor string";
    case AAX_FREQUENCY: return "frequency";
    case AAX_TRACKS: return "number of tracks";
    case AAX_FORMAT: return "audio format";
    case AAX_REFRESHRATE: return "audio refresh rate";
    case AAX_TRACKSIZE: return "track size";
    case AAX_NO_SAMPLES: return "number of samples";
    case AAX_LOOP_START: return "loop start point";
    case AAX_LOOP_END: return "loop end point";
    case AAX_MONO_SOURCES: return "number of mono emitters";
    case AAX_STEREO_SOURCES: return "number of multi-track emitters";
    case AAX_BLOCK_ALIGNMENT: return "block alignment";
    case AAX_AUDIO_FRAMES: return "number of audio frames";
    case AAX_UPDATERATE: return "4D parameter update rate";
    case AAX_LATENCY: return "audio latency";
    case AAX_TRACK_LAYOUT: return "track layout";
    case AAX_BITRATE: return" bit rate";
    case AAX_FRAME_TIMING: return "frame timing";
    case AAX_PEAK_VALUE: return "track peak value";
    case AAX_AVERAGE_VALUE: return "track average value";
    case AAX_COMPRESSION_VALUE: return "track compression value";
    case AAX_GATE_ENABLED: return "noise gate enabled";
    case AAX_SHARED_MODE: return "driver shared mode";
    case AAX_TIMER_MODE: return "driver timer driven mode";
    case AAX_BATCHED_MODE: return "driver non-realtime batched mode";
    case AAX_SEEKABLE_SUPPORT: return "driver seekable support";
    case AAX_TRACKS_MIN: return "minimum number of supported playback channels";
    case AAX_TRACKS_MAX: return "maximum number of supported playback channels";
    case AAX_PERIODS_MIN: return "minimum number of supported driver periods";
    case AAX_PERIODS_MAX: return "maximum number of supported driver periods";
    case AAX_FREQUENCY_MIN: return "minimum supported playback frequency";
    case AAX_FREQUENCY_MAX: return "maximum supported playback frequency";
    case AAX_SAMPLES_MAX: return "maximum number of capture samples";
    case AAX_COVER_IMAGE_DATA: return "album cover art";
    case AAX_MUSIC_PERFORMER_STRING: return "music performer string";
    case AAX_MUSIC_GENRE_STRING: return "music genre string";
    case AAX_TRACK_TITLE_STRING: return "song title string";
    case AAX_TRACK_NUMBER_STRING: return "song track number string";
    case AAX_ALBUM_NAME_STRING: return "album name string";
    case AAX_RELEASE_DATE_STRING: return "song release date string";
    case AAX_SONG_COPYRIGHT_STRING: return "song copyright string";
    case AAX_SONG_COMPOSER_STRING: return "song composer string";
    case AAX_SONG_COMMENT_STRING: return "song comment string";
    case AAX_ORIGINAL_PERFORMER_STRING: return "song original performer string";
    case AAX_WEBSITE_STRING: return "artist website string";
    case AAX_MUSIC_PERFORMER_UPDATE: return "updated music performer string";
    case AAX_TRACK_TITLE_UPDATE: return "updated song title string";
    case AAX_RELEASE_MODE: return "release mode";
    }
}

inline std::string to_string(enum aaxErrorType type) {
    return aaxGetErrorString(type);
}

inline std::string to_string(enum aaxEmitterMode mode)
{
    switch(mode)
    {
    case AAX_STEREO: return "multi track stereo";
    case AAX_ABSOLUTE: return "absolute position";
    case AAX_RELATIVE: return "relative to the sensor position";
    }
}

inline std::string to_string(enum aaxState state)
{
    switch(state)
    {
    case AAX_INITIALIZED: return "initialized";
    case AAX_PLAYING: return "playing";
    case AAX_STOPPED: return "stopped";
    case AAX_SUSPENDED: return "suspended";
    case AAX_CAPTURING: return "capturing";
    case AAX_PROCESSED: return "processed";
    case AAX_STANDBY: return "stand-by";
    case AAX_UPDATE: return "update";
    case AAX_MAXIMUM: return "maximum number of buffers";
    }
}

inline std::string to_string(enum aaxRenderMode mode)
{
    switch(mode)
    {
    case AAX_MODE_WRITE_STEREO: return "stereo playback";
    case AAX_MODE_WRITE_SPATIAL: return "spatialized stereo playback";
    case AAX_MODE_WRITE_SURROUND: return "surround sound playback";
    case AAX_MODE_WRITE_HRTF: return "binaural headphone playback";
    }
}

inline std::string to_string(enum aaxDistanceModel model)
{
    switch(model)
    {
    case AAX_DISTANCE_MODEL_NONE: return "no distance decay";
    case AAX_EXPONENTIAL_DISTANCE: return "natural exponential distance decay";
    case AAX_EXPONENTIAL_DISTANCE_DELAY: return "natural exponential distance decay with distance delay";
    case AAX_AL_INVERSE_DISTANCE: return "AL inverse distance decay";
    case AAX_AL_INVERSE_DISTANCE_CLAMPED: return "AL clamped inverse distance decay";
    case AAX_AL_LINEAR_DISTANCE: return "AL linear distance decay";
    case AAX_AL_LINEAR_DISTANCE_CLAMPED: return "AL clamped linear distance decay";
    case AAX_AL_EXPONENT_DISTANCE: return "AL exponential distance decay";
    case AAX_AL_EXPONENT_DISTANCE_CLAMPED: return "AL clamped exponential distance decay";
    }
}

inline std::string to_string(enum aaxFilterType type) {
    return aaxFilterGetNameByType(0, type);
}

inline std::string to_string(enum aaxEffectType type) {
    return aaxEffectGetNameByType(0, type);
}

inline std::string to_string(enum aaxWaveformType type)
{
    if (!type) return "{}";
    std::string result;
    if (type & AAX_INVERSE) result += "inverse ";
    if (type & AAX_CONSTANT_VALUE) result += "constant | ";
    if (type & AAX_TRIANGLE_WAVE) result += "triangle | ";
    if (type & AAX_SINE_WAVE) result += "sine | ";
    if (type & AAX_SQUARE_WAVE) result += "square | ";
    if (type & AAX_SAWTOOTH_WAVE) result += " sawtooth | ";
    if (type & AAX_IMPULSE_WAVE) result += " impulse | ";
    if (type & AAX_WHITE_NOISE) result += "white noise | ";
    if (type & AAX_PINK_NOISE) result += "pink noise | ";
    if (type & AAX_BROWNIAN_NOISE) result += "brownian noise | ";
    if (type & AAX_ENVELOPE_FOLLOW) result += "envelope follow | ";
    return "{" + result.substr(0, result.size() - 3) + "}";
}

inline std::string to_string(enum aaxFrequencyFilterType type)
{
    std::string result;
    if (type & AAX_6DB_OCT) result += "6dB/oct ";
    else if (type & AAX_12DB_OCT) result += "12dB/oct ";
    else if (type & AAX_24DB_OCT) result += "24dB/oct ";
    else if (type & AAX_36DB_OCT) result += "36dB/oct ";
    else if (type & AAX_48DB_OCT) result += "48dB/oct ";
    if (type & AAX_BESSEL) result += "bessel";
    else result += "butterworth";
    return result;
}

inline std::string to_string(enum aaxProcessingType type)
{
    switch(type)
    {
    case AAX_OVERWRITE: return "overwrite";
    case AAX_ADD: return "add";
    case AAX_MIX: return "mix";
    case AAX_RINGMODULATE: return "ring-modulate";
    case AAX_APPEND: return "append";
    }
}

} // namespace aax

#endif

