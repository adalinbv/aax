/*
 * Copyright (C) 2015-2016 by Erik Hofman.
 * Copyright (C) 2015-2016 by Adalin B.V.
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

#ifndef AEONWAVE_MATRIX
#define AEONWAVE_MATRIX 1

#include <aax/aax.h>

namespace AAX
{

class Vector
{
public:
    Vector() {
        _v[0] = _v[1] = _v[2] = _v[3] = 0.0f;
    }

    Vector(aaxVec4f v) {
        _v[0] = v[0]; _v[1] = v[1]; _v[2] = v[2]; _v[3] = v[3];
    }

    Vector(float x, float y, float z) {
        _v[0] = x; _v[1] = y; _v[2] = z; _v[3] = 0.0f;
    }

    Vector(float w, float x, float y, float z) {
        _v[0] = w; _v[1] = x; _v[2] = y; _v[3] = z;
    }

    ~Vector() {}

    // ** support ******
    Vector& operator*=(Vector &v4) {
        aaxVec4f& v = v4.config();
        _v[0] *= v[0]; _v[1] *= v[1]; _v[2] *= v[2]; _v[3] *= v[3];
        return *this;
    }
    Vector& operator/=(Vector &v4) {
        aaxVec4f& v = v4.config();
        _v[0] /= v[0]; _v[1] /= v[1]; _v[2] /= v[2]; _v[3] /= v[3];
        return *this;
    }
    Vector& operator*=(aaxVec4f v) {
        _v[0] *= v[0]; _v[1] *= v[1]; _v[2] *= v[2]; _v[3] *= v[3];
        return *this;
    }
    Vector& operator/=(aaxVec4f v) {
        _v[0] /= v[0]; _v[1] /= v[1]; _v[2] /= v[2]; _v[3] /= v[3];
        return *this;
    }
    Vector& operator*=(float f) {
        _v[0] *= f; _v[1] *= f; _v[2] *= f; _v[3] *= f;
        return *this;
    }
    Vector& operator/=(float f) {
        _v[0] /= f; _v[1] /= f; _v[2] /= f; _v[3] /= f;
        return *this;
    }
    Vector& operator+=(Vector& v4) {
        aaxVec4f& v = v4.config();
        _v[0] += v[0]; _v[1] += v[1]; _v[2] += v[2]; _v[3] += v[3];
        return *this;
    }
    Vector& operator-=(Vector& v4) {
        aaxVec4f& v = v4.config();
        _v[0] -= v[0]; _v[1] -= v[1]; _v[2] -= v[2]; _v[3] -= v[3];
        return *this;
    }
    Vector& operator+=(aaxVec4f v) {
        _v[0] += v[0]; _v[1] += v[1]; _v[2] += v[2]; _v[3] += v[3];
        return *this;
    }
    Vector& operator-=(aaxVec4f v) {
        _v[0] -= v[0]; _v[1] -= v[1]; _v[2] -= v[2]; _v[3] -= v[3];
        return *this;
    }
    Vector& operator+=(float f) {
        _v[0] += f; _v[1] += f; _v[2] += f; _v[3] += f;
        return *this;
    }
    Vector& operator-=(float f) {
        _v[0] -= f; _v[1] -= f; _v[2] -= f; _v[3] -= f;
        return *this;
    }
    Vector& operator=(Vector &v4) {
        aaxVec4f& v = v4.config();
        _v[0] = v[0]; _v[1] = v[1]; _v[2] = v[2]; _v[3] = v[3];
        return *this;
    }
    Vector& operator=(aaxVec4f v) {
        _v[0] = v[0]; _v[1] = v[1]; _v[2] = v[2]; _v[3] = v[3];
        return *this;
    }
    Vector& operator=(float f) {
        _v[0] = f; _v[1] = f; _v[2] = f; _v[3] = f;
        return *this;
    }
    inline float& operator[](unsigned p) {
        return _v[p];
    }

    aaxVec4f& config() {
        return _v;
    }

private:
    aaxVec4f _v;
};


class Matrix
{
public:
    Matrix() {
        aaxMatrixSetIdentityMatrix(_m);
    }

    Matrix(aaxMtx4f m) {
        aaxMatrixCopyMatrix(_m,m);
    }

    ~Matrix() {}

    inline bool set(Vector& p, Vector& a) {
        return aaxMatrixSetDirection(_m,p.config(),a.config());
    }
    inline bool set(const aaxVec3f p, const aaxVec3f a) {
        return aaxMatrixSetDirection(_m,p,a);
    }
    inline bool set(Vector& p, Vector& a, Vector& u) {
        return aaxMatrixSetOrientation(_m,p.config(),a.config(),u.config());
    }
    inline bool set(const aaxVec3f p, const aaxVec3f a, const aaxVec3f u) {
        return aaxMatrixSetOrientation(_m,p,a,u);
    }
    inline bool get(aaxVec3f p, aaxVec3f a, aaxVec3f u) {
        return aaxMatrixGetOrientation(_m,p,a,u);
    }

    inline bool translate(float dx, float dy, float dz) {
        return aaxMatrixTranslate(_m,dx,dy,dz);
    }
    inline bool translate(Vector& tv) {
        aaxVec4f& t = tv.config();
        return aaxMatrixTranslate(_m,t[0],t[1],t[2]);
    }
    inline bool translate(const aaxVec3f t) {
        return aaxMatrixTranslate(_m,t[0],t[1],t[2]);
    }
    inline bool rotate(float a, float x, float y, float z) {
        return aaxMatrixRotate(_m,a,x,y,z);
    }
    inline bool multiply(Matrix& m) {
        return aaxMatrixMultiply(_m,m.config());
    }
    inline bool multiply(aaxMtx4f m) {
        return aaxMatrixMultiply(_m,m);
    }
    inline bool inverse() {
        return aaxMatrixInverse(_m);
    }

    // ** support ******
    Matrix& operator*=(Matrix &m) {
        aaxMatrixMultiply(_m,m.config());
        return *this;
    }
    Matrix& operator/=(Matrix &m) {
        aaxMtx4f im;
        aaxMatrixCopyMatrix(im,m.config());
        aaxMatrixInverse(im);
        aaxMatrixMultiply(_m,im);
        return *this;
    }
    Matrix& operator*=(aaxMtx4f& m) {
        aaxMatrixMultiply(_m,m);
    }
    Matrix& operator/=(aaxMtx4f& m) {
        aaxMtx4f im;
        aaxMatrixCopyMatrix(im, m);
        aaxMatrixInverse(im);
        aaxMatrixMultiply(_m,im);
        return *this;
    }
    Matrix& operator+=(aaxVec3f v) {
        aaxMatrixTranslate(_m,v[0],v[1],v[2]);
        return *this;
    }
    Matrix& operator-=(aaxVec3f v) {
        aaxMatrixTranslate(_m,-v[0],-v[1],-v[2]);
        return *this;
    }
    Matrix operator~() {
        aaxMtx4f im; 
        aaxMatrixCopyMatrix(im,_m);
        aaxMatrixInverse(im);
        return Matrix(im);
    }
    Matrix& operator=(Matrix &m) {
        aaxMatrixCopyMatrix(_m,m.config());
        return *this;
    }

    aaxMtx4f& config() {
        return _m;
    }

private:
    aaxMtx4f _m;
};


}

#endif /* AEONWAVE_MATRIX */

