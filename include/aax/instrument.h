/*
 * Copyright 2007-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef AAX_INST_INSTRUMENT_H
#define AAX_INST_INSTRUMENT_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if defined _WIN32 || defined __CYGWIN__
# define AAX_INST_APIENTRY __cdecl
# ifdef AAX_INST_BUILD_LIBRARY
#  define AAX_INST_API __declspec(dllexport)
# else
#  define AAX_INST_API __declspec(dllimport)
# endif
#else
# define AAX_INST_APIENTRY
# if __GNUC__ >= 4
#  define AAX_INST_API __attribute__((visibility("default")))
# else
#  define AAX_INST_API extern
# endif
#endif

#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export on
#endif


enum aaxInstrumentParameter
{
   AAX_NOTEPARAM_NONE = 0,

   AAX_NOTE_PITCHBEND,
   AAX_NOTE_VELOCITY,
   AAX_NOTE_AFTERTOUCH,
   AAX_NOTE_DISPLACEMENT,
   AAX_NOTE_SUSTAIN,
   AAX_NOTE_SOFTEN,

   AAX_NOTEPARAM_MAX
};

typedef void* aaxInstrument;

/*
 * Instrument support (version 3.0 and later)
 */
AAX_INST_API aaxInstrument AAX_INST_APIENTRY aaxInstrumentCeate(aaxConfig);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentDestroy(aaxInstrument);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentLoad(aaxInstrument, const char*, unsigned int, unsigned int);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentLoadByName(aaxInstrument, const char*, const char*);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentSetSetup(aaxInstrument, enum aaxSetupType, int);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentRegister(aaxInstrument);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentDeregister(aaxInstrument);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentSetParam(aaxInstrument, enum aaxInstrumentParameter, float);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentNoteOn(aaxInstrument, unsigned int);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentNoteOff(aaxInstrument, unsigned int);
AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentNoteSetParam(aaxInstrument, unsigned int, enum aaxInstrumentParameter, float);

AAX_INST_API int AAX_INST_APIENTRY aaxInstrumentGetSetup(aaxInstrument, enum aaxSetupType);
AAX_INST_API float AAX_INST_APIENTRY aaxInstrumentGetParam(aaxInstrument, enum aaxInstrumentParameter);
AAX_INST_API float AAX_INST_APIENTRY aaxInstrumentNoteGetParam(aaxInstrument, unsigned int, enum aaxInstrumentParameter);


#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export off
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

