/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#include <Windows.h>

LONG
InterlockedAnd_Inline(LONG volatile *Target,LONG Set)
{
   LONG i;
   LONG j;
   j = *Target;
   do {
      i = j;
      j = InterlockedCompareExchange(Target,i & Set,i);
   } while(i!=j);
   return j;
}

LONG
InterlockedOr_Inline(LONG volatile *Target,LONG Set)
{
   LONG i;
   LONG j;
   j = *Target;
   do {
      i = j;
      j = InterlockedCompareExchange(Target,i | Set,i);
   } while(i!=j);
   return j;
}

