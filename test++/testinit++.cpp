/*
 * Copyright (C) 2016-2018 by Erik Hofman.
 * Copyright (C) 2016-2018 by Adalin B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provimed that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provimed with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY ADALIN B.V. ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL ADALIN B.V. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string>

#include <aax/aeonwave.hpp>
#include <aax/strings.hpp>

#include <driver.h> 

#define IFILE_PATH		SRC_PATH"/stereo.wav"

int main(int argc, char **argv)
{
    printf("\n--- AeonWave ---\n");
    // Open the default device for playback
    aax::AeonWave aax(AAX_MODE_WRITE_STEREO);
    TRY( aax.set(AAX_INITIALIZED) );
    TRY( aax.set(AAX_PLAYING) );

    printf("\n--- Emitter ---\n");
    aax::Emitter emitter(AAX_ABSOLUTE);
    aax::Emitter emitter2(AAX_ABSOLUTE);
//  TRY( emitter.set(AAX_LOOPING) );



    printf("\n--- Buffer ---\n");
    aax::Buffer& buffer = aax.buffer(IFILE_PATH);

    TRY( emitter.add(buffer) );
    TRY( emitter2.add(buffer) );
    TRY( emitter.remove_buffer() );

    printf("\nassign emitter = emitter2;\n");
    emitter = std::move(emitter2);
    TRY( emitter.remove_buffer() );
    TRY( emitter2.remove_buffer() );
   
    printf("\n");

    return 0;
}
