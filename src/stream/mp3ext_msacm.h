/*
 * Copyright 2012-2015 by Erik Hofman.
 * Copyright 2012-2015 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef __FILE_EXT_MSACM_H
#define __FILE_EXT_MSACM_H 1

#ifdef WINXP
#include <Windows.h>
#include <mmreg.h>
#include <msacm.h>

#define MP3_BLOCK_SIZE                          522
#ifndef MPEGLAYER3_WFX_EXTRA_BYTES
# define MPEGLAYER3_WFX_EXTRA_BYTES             12
#endif
#ifndef MPEGLAYER3_ID_MPEG
# define MPEGLAYER3_ID_MPEG                     1
#endif
#ifndef MPEGLAYER3_FLAG_PADDING_OFF
# define MPEGLAYER3_FLAG_PADDING_OFF            0x00000002
#endif
#ifndef ACM_STREAMSIZEF_SOURCE
# define ACM_STREAMSIZEF_SOURCE                 0x00000000L
#endif
#ifndef ACMDRIVERDETAILS_SUPPORTF_CODEC
# define ACMDRIVERDETAILS_SUPPORTF_CODEC        0x00000001L
#endif
#ifndef ACM_FORMATTAGDETAILSF_INDEX
# define ACM_FORMATTAGDETAILSF_INDEX            0x00000000L
#endif
#ifndef ACMERR_BASE
# define ACMERR_BASE                            (512)
# define ACMERR_NOTPOSSIBLE                     (ACMERR_BASE + 0)
# define ACMERR_BUSY                            (ACMERR_BASE + 1)
# define ACMERR_UNPREPARED                      (ACMERR_BASE + 2)
# define ACMERR_CANCELED                        (ACMERR_BASE + 3)
#endif
#ifndef ACM_STREAMCONVERTF_BLOCKALIGN
# define ACM_STREAMCONVERTF_BLOCKALIGN          0x00000004
# define ACM_STREAMCONVERTF_START               0x00000010
# define ACM_STREAMCONVERTF_END                 0x00000020
#endif

typedef MMRESULT (DLL_API *acmDriverOpen_proc)(LPHACMDRIVER, HACMDRIVERID, DWORD);
typedef MMRESULT (DLL_API *acmDriverClose_proc)(HACMDRIVER, DWORD);
typedef MMRESULT (DLL_API *acmDriverEnum_proc)(ACMDRIVERENUMCB, DWORD_PTR, DWORD);
typedef MMRESULT (DLL_API *acmDriverDetailsA_proc)(HACMDRIVERID, LPACMDRIVERDETAILSA, DWORD);
typedef MMRESULT (DLL_API *acmStreamOpen_proc)(LPHACMSTREAM, HACMDRIVER, LPWAVEFORMATEX, LPWAVEFORMATEX, LPWAVEFILTER, DWORD_PTR, DWORD_PTR, DWORD);
typedef MMRESULT (DLL_API *acmStreamClose_proc)(HACMSTREAM, DWORD);
typedef MMRESULT (DLL_API *acmStreamSize_proc)(HACMSTREAM, DWORD, LPDWORD, DWORD);
typedef MMRESULT (DLL_API *acmStreamConvert_proc)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
typedef MMRESULT (DLL_API *acmStreamPrepareHeader_proc)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
typedef MMRESULT (DLL_API *acmStreamUnprepareHeader_proc)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
typedef MMRESULT (DLL_API *acmFormatTagDetailsA_proc)(HACMDRIVER, LPACMFORMATTAGDETAILSA, DWORD);
#endif

#endif /* __FILE_EXT_MSACM_H */


