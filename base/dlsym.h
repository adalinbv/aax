/*
 * Copyright (C) 2005-2011 by Erik Hofman.
 * Copyright (C) 2007-2011 by Adalin B.V.
 *
 * This file is part of OpenAL-AeonWave.
 *
 *  OpenAL-AeonWave is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenAL-AeonWave is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenAL-AeonWave.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OAL_DLSYM_H
#define _OAL_DLSYM_H 1

#define DECL_FUNCTION(f)	static f##_proc p##f = 0
#define TIE_FUNCTION(f)		p##f = (f##_proc)_oalGetProcAddress(audio, #f)

char *_oalGetSymError(char *error);
void *_oalIsLibraryPresent(const char *name, const char *version);
void *_oalGetProcAddress(void *handle, const char *func);
void *_oalGetGlobalProcAddress(const char *func);

#endif /* !_OAL_DLSYM_H */

