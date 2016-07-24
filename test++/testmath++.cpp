/*
 * Copyright (C) 2016 by Erik Hofman.
 * Copyright (C) 2016 by Adalin B.V.
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

#include <ostream>

#include <aax/aeonwave.hpp>

#define LOG(s,a,b,c) \
    if (((a)==(b))==(c)) printf((c)?"M1 == M2, succes":"M1 != M2, succes"); \
    else { printf(c?"M1 != M2, failed":"M1 == M2, failed"); \
           std::cout << "m1:\n" << (a); std::cout << "m2:\n" << (b); }\
    printf(": %s\n",(s));

#define PVEC(c,v1,v2,v3) \
    printf("Vector(%2.1f, %2.1f, %2.1f) %c Vector(%2.1f, %2.1f, %2.1f) = Vector(%2.1f, %2.1f, %2.1f)\n", v1[0], v1[1], v1[2], c, v2[0], v2[1], v2[2], v3[0], v3[1], v3[2]);

aaxVec3f at  = { 2.0f, 0.0f, -1.0f };
aaxVec3f up  = { 2.0f, 1.0f,  0.0f };
aaxVec3f pos = { 1.0f, 3.0f, -2.5f };

int main(int argc, char **argv)
{
    aax::Vector v1(at);
    aax::Vector v2(pos);
    aax::Vector v3;

    v3 = v1+v2; PVEC('+',v1,v2,v3);
    v3 = v1-v2; PVEC('-',v1,v2,v3);
    v3 = v1*v2; PVEC('*',v1,v2,v3);
    v3 = v1/v2; PVEC('/',v1,v2,v3);

    v3 = v2.normalized();
    printf("Normalized Vector(%2.1f, %2.1f, %2.1f) = Vector(%2.1f, %2.1f, %2.1f)\n", v2[0], v2[1], v2[2], v3[0], v3[1], v3[2]);

    aax::Matrix64 im64, m64;
    aax::Matrix im, m1, m2;

    LOG("Initializing",m1,m2,true);

    m1.rotate(0.13f, 1.0f, 0.0f, 0.0f);
    LOG("Rotating",m1,m2,false);

    m1 = m64;
    LOG("Convert from 64-bit to 32-bit",m1,m2,true);

    m1.rotate(-0.26f, 1.0f, 0.0f, 0.0f);
    m64.rotate(-0.26, 1.0, 0.0, 0.0);
    aax::Matrix m3 = m64;
    LOG("Rotating M1 and M64",m1,m3,true);

    m1 += pos;
    m3 = m1 * im;
    LOG("Multiplying identity matrix by m1",m3,m1,true);

    m1.set(pos, at, up); m1.rotate(0.13f, 1.0f, 0.0f, 0.0f);
    m64.set(pos, at, up); m64.rotate(0.13, 1.0, 0.0, 0.0);
    m3 = m64;
    LOG("Testing orientation",m1,m3,true);

    m2 = ~m1;
    m64.inverse();
    m3 = m64;
    LOG("Inverse matrix",m2,m3,true);

    return 0;
}
