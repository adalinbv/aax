/*
 * Copyright 2007-2017 by Adalin B.V.
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


enum aaxControllerParameter
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

typedef void* aaxController;

/*
 * Instrument support (version 3.0 and later)
 */
AAX_INST_API aaxController AAX_INST_APIENTRY aaxControllerCeate(aaxConfig);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerDestroy(aaxController);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerLoad(aaxController, const char*, unsigned int, unsigned int);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerLoadByName(aaxController, const char*, const char*);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerSetSetup(aaxController, enum aaxSetupType, int);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerRegister(aaxController);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerDeregister(aaxController);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerSetParam(aaxController, enum aaxControllerParameter, float);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerNoteOn(aaxController, unsigned int);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerNoteOff(aaxController, unsigned int);
AAX_INST_API int AAX_INST_APIENTRY aaxControllerNoteSetParam(aaxController, unsigned int, enum aaxControllerParameter, float);

AAX_INST_API int AAX_INST_APIENTRY aaxControllerGetSetup(aaxController, enum aaxSetupType);
AAX_INST_API float AAX_INST_APIENTRY aaxControllerGetParam(aaxController, enum aaxControllerParameter);
AAX_INST_API float AAX_INST_APIENTRY aaxControllerNoteGetParam(aaxController, unsigned int, enum aaxControllerParameter);


#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
# pragma export off
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

