/*
 * Copyright (C) 2014 by Erik Hofman.
 * Copyright (C) 2014 by Adalin B.V.
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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <aax/aax.h>

#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

void
help()
{
    printf("Usage:\n");
    printf("  aaxcvt -i <filename> -o <filename> -f <format> (-raw))\n\n");
    exit(-1);
}

int main(int argc, char **argv)
{
    enum aaxFormat format;
    char *infile, *outfile;
    int raw, rv = 0;

    if ((argc < 7) || (argc > 8))  {
        help();
    }

    raw = getCommandLineOption(argc, argv, "-r") ? 1 : 0;

    infile = getInputFile(argc, argv, NULL);
    if (!infile)
    {
        printf("Input file not specified or unable to access.\n");
        help();
        exit(-2);
    }
    outfile = getOutputFile(argc, argv, NULL);
    if (!outfile) {
       help();
    }

    format = getAudioFormat(argc, argv, AAX_FORMAT_NONE);
    if (format != AAX_FORMAT_NONE)
    {
        aaxConfig config;
        config= aaxDriverOpenByName("AeonWave Loopback", AAX_MODE_WRITE_STEREO);
        aaxBuffer buffer = bufferFromFile(config, infile);
        if (buffer)
        {
            aaxBufferSetSetup(buffer, AAX_FORMAT, format);
            if (!raw) {
               aaxBufferWriteToFile(buffer, outfile, AAX_OVERWRITE);
            }
            else
            {
               int fd = open(outfile, O_CREAT|O_WRONLY, 0644);
               if (fd >= 0)
               {
                  int size = aaxBufferGetSetup(buffer, AAX_TRACKSIZE);
                  void **data = aaxBufferGetData(buffer);
                  int res = write(fd, *data, size);
                  if (res != size) {
                     printf("Written %i bytes of the required: %i\n", res, size);
                  }
                  close(fd);
               }
               else {
                  printf("Unable to open file for writing: %s\n", outfile);
               }
            }
            aaxBufferDestroy(buffer);
        }
        aaxDriverDestroy(config);
    }
    else {
        printf("Unsupported audio format.\n");
        rv = -2;
    }

    return rv;
}

