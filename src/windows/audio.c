/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
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

