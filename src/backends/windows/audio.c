/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
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

