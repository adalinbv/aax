/*
 * SPDX-FileCopyrightText: Copyright © 2015-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2015-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 *                          WITH Universal-FOSS-exception-1.0
 */

#pragma once

// Backwards compatibility.
// Development takes place in aax/aeonwave
#include <aax/aeonwave>

#if AAX_PATCH_LEVEL > 240119
namespace aax = aeonwave;
#endif
