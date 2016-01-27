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

#include <cstring>
#include <iostream>
#include <iomanip>

#include <aax/aax.h>

namespace AAX
{


template <typename T>
class VecBase
{
public:
    VecBase() : _v4(true) {
        _v[0] = _v[1] = _v[2] = _v[3] = 0;
    }

    VecBase(T v[4]) : _v4(true) {
        _v[0] = v[0]; _v[1] = v[1]; _v[2] = v[2]; _v[3] = v[3];
    }

    VecBase(T x, T y, T z) : _v4(false) {
        _v[0] = x; _v[1] = y; _v[2] = z; _v[3] = 0;
    }

    VecBase(T w, T x, T y, T z) : _v4(true) {
        _v[0] = w; _v[1] = x; _v[2] = y; _v[3] = z;
    }

    VecBase(T f) : _v4(true) {
        _v[0] = f; _v[1] = f; _v[2] = f; _v[3] = f;
    }

    ~VecBase() {}

    // ** support ******
    VecBase& operator=(VecBase<T>& v4) {
        T (&v)[4] = v4.config();
        _v[0] = v[0]; _v[1] = v[1]; _v[2] = v[2]; _v[3] = v[3];
        _v4 = v4.is_v4();
        return *this;
    }

    VecBase& operator*=(VecBase<T>& v4) {
        T (&v)[4] = v4.config();
        _v[0] *= v[0]; _v[1] *= v[1]; _v[2] *= v[2];
        if (_v4) _v[3] *= v[3];
        return *this;
    }
    VecBase& operator/=(VecBase<T>& v4) {
        T (&v)[4] = v4.config();
        _v[0] /= v[0]; _v[1] /= v[1]; _v[2] /= v[2];
        if (_v4) _v[3] /= v[3];
        return *this;
    }
    VecBase& operator*=(T v[4]) {
        _v[0] *= v[0]; _v[1] *= v[1]; _v[2] *= v[2];
        if (_v4) _v[3] *= v[3];
        return *this;
    }
    VecBase& operator/=(T v[4]) {
        _v[0] /= v[0]; _v[1] /= v[1]; _v[2] /= v[2];
        if (_v4) _v[3] /= v[3];
        return *this;
    }
    VecBase& operator*=(T f) {
        _v[0] *= f; _v[1] *= f; _v[2] *= f;
        if (_v4) _v[3] *= f;
        return *this;
    }
    VecBase& operator/=(T f) {
        T fi = 1/f;
        _v[0] *= fi; _v[1] *= fi; _v[2] *= fi;
        if (_v4) _v[3] *= fi;
        return *this;
    }
    VecBase& operator+=(VecBase<T>& v4) {
        T (&v)[4] = v4.config();
        _v[0] += v[0]; _v[1] += v[1]; _v[2] += v[2];
        if (_v4) _v[3] += v[3];
        return *this;
    }
    VecBase& operator-=(VecBase<T>& v4) {
        T (&v)[4] = v4.config();
        _v[0] -= v[0]; _v[1] -= v[1]; _v[2] -= v[2];
        if (_v4) _v[3] -= v[3];
        return *this;
    }
    VecBase& operator+=(T v[4]) {
        _v[0] += v[0]; _v[1] += v[1]; _v[2] += v[2];
        if (_v4) _v[3] += v[3];
        return *this;
    }
    VecBase& operator-=(T v[4]) {
        _v[0] -= v[0]; _v[1] -= v[1]; _v[2] -= v[2];
        if (_v4) _v[3] -= v[3];
        return *this;
    }
    VecBase& operator+=(T f) {
        _v[0] += f; _v[1] += f; _v[2] += f;
        if (_v4) _v[3] += f;
        return *this;
    }
    VecBase& operator-=(T f) {
        _v[0] -= f; _v[1] -= f; _v[2] -= f;
        if (_v4) _v[3] -= f;
        return *this;
    }
    VecBase<T> operator-() {
        VecBase<T> v4;
        v4[0] = -_v[0]; v4[1] = -_v[1]; v4[2] = -_v[2];
        if (_v4) v4[3] = -_v[3];
        return v4;
    }
    inline T& operator[](unsigned p) {
        return _v[p];
    }

    T (&config())[4] {
        return _v;
    }

    inline bool is_v4() {
        return _v4;
    }

private:
    T _v[4];
    bool _v4;
};
typedef VecBase<float> Vector;
typedef VecBase<double> Vector64;


template <typename T>
class MtxBase
{
public:
    MtxBase() {}
    ~MtxBase() {}

    T (&config())[4][4] {
        return _m;
    }

    // ** support ******
    bool operator==(MtxBase<T>& m) {
        return (memcmp(_m, m.config(), sizeof(T[4][4])) == 0);
    }
    bool operator==(T (&m)[4][4]) {
        return (memcmp(_m, m, sizeof(T[4][4])) == 0);
    }
    bool operator!=(MtxBase<T>& m) {
        return (memcmp(_m, m.config(), sizeof(T[4][4])) != 0);
    }
    bool operator!=(T (&m)[4][4]) {
        return (memcmp(_m, m, sizeof(T[4][4])) != 0);
    }

    friend std::ostream& operator<<(std::ostream& s, MtxBase<T>& mtx) {
        T (&m)[4][4] = mtx.config();
        s << std::fixed << std::showpoint << std::setprecision(4);
        for (int j = 0; j<4; ++j) {
            s << "[";
            for (int i=0; i<4; ++i) {
                s << " " << std::setw(7) << m[i][j];
            }
            s << " ]" << std::endl;
        }
    }

protected:
    T _m[4][4];
};


class Matrix : public MtxBase<float>
{
public:
    Matrix() {
        aaxMatrixSetIdentityMatrix(_m);
    }

    Matrix(aaxMtx4f& m) {
        aaxMatrixCopyMatrix(_m,m);
    }

    Matrix(aaxMtx4d& m) {
        aaxMatrix64ToMatrix(_m, m);
    }

    Matrix(MtxBase<float>& m) {
        aaxMatrixCopyMatrix(_m, m.config());
    }

    Matrix(MtxBase<double>& m) {
        aaxMatrix64ToMatrix(_m, m.config());
    }

    ~Matrix() {}

    inline bool set(VecBase<float>& p, VecBase<float>& a) {
        return aaxMatrixSetDirection(_m,p.config(),a.config());
    }
    inline bool set(const aaxVec3f& p, const aaxVec3f& a) {
        return aaxMatrixSetDirection(_m,p,a);
    }
    inline bool set(VecBase<float>& p, VecBase<float>& a, VecBase<float>& u) {
        return aaxMatrixSetOrientation(_m,p.config(),a.config(),u.config());
    }
    inline bool set(const aaxVec3f& p, const aaxVec3f& a, const aaxVec3f& u) {
        return aaxMatrixSetOrientation(_m,p,a,u);
    }
    inline bool get(aaxVec3f& p, aaxVec3f& a, aaxVec3f& u) {
        return aaxMatrixGetOrientation(_m,p,a,u);
    }

    inline bool translate(float dx, float dy, float dz) {
        return aaxMatrixTranslate(_m,dx,dy,dz);
    }
    bool translate(VecBase<float>& tv) {
        aaxVec4f& t = tv.config();
        return aaxMatrixTranslate(_m,t[0],t[1],t[2]);
    }
    inline bool translate(const aaxVec3f& t) {
        return aaxMatrixTranslate(_m,t[0],t[1],t[2]);
    }
    inline bool rotate(float a, float x, float y, float z) {
        return aaxMatrixRotate(_m,a,x,y,z);
    }
    inline bool multiply(Matrix& m) {
        return aaxMatrixMultiply(_m,m.config());
    }
    inline bool multiply(aaxMtx4f& m) {
        return aaxMatrixMultiply(_m,m);
    }
    inline bool inverse() {
        return aaxMatrixInverse(_m);
    }

    // ** support ******
    Matrix& operator*=(Matrix& m) {
        aaxMatrixMultiply(_m,m.config());
        return *this;
    }
    Matrix& operator*=(aaxMtx4f& m) {
        aaxMatrixMultiply(_m,m);
        return *this;
    }
    Matrix& operator/=(Matrix& m) {
        aaxMtx4f im;
        aaxMatrixCopyMatrix(im,m.config());
        aaxMatrixInverse(im);
        aaxMatrixMultiply(_m,im);
        return *this;
    }
    Matrix& operator/=(aaxMtx4f& m) {
        aaxMtx4f im;
        aaxMatrixCopyMatrix(im, m);
        aaxMatrixInverse(im);
        aaxMatrixMultiply(_m,im);
        return *this;
    }
    Matrix& operator+=(Vector& v) {
        aaxMatrixTranslate(_m,v[0],v[1],v[2]);
        return *this;
    }
    Matrix& operator+=(aaxVec3f& v) {
        aaxMatrixTranslate(_m,v[0],v[1],v[2]);
        return *this;
    }
    Matrix& operator-=(Vector& v) {
        aaxMatrixTranslate(_m,-v[0],-v[1],-v[2]);
        return *this;
    }
    Matrix& operator-=(aaxVec3f& v) {
        aaxMatrixTranslate(_m,-v[0],-v[1],-v[2]);
        return *this;
    }
    aaxMtx4f& operator~() {
        aaxMtx4f im;
        aaxMatrixCopyMatrix(im,_m);
        aaxMatrixInverse(im);
        return im;
    }
    Matrix& operator=(aaxMtx4f m) {
        aaxMatrixCopyMatrix(_m,m);
        return *this;
    }
    Matrix& operator=(MtxBase<double>& m) {
        aaxMatrix64ToMatrix(_m, m.config());
        return *this;
    }
    Matrix& operator=(aaxMtx4d m) {
        aaxMatrix64ToMatrix(_m, m);
        return *this;
    }
    friend Matrix operator*(Matrix m1, Matrix& m2) {
        return m1 *= m2;
    }
};


class Matrix64 : public MtxBase<double>
{
public:
    Matrix64() {
        aaxMatrix64SetIdentityMatrix(_m);
    }

    Matrix64(aaxMtx4d& m) {
        aaxMatrix64CopyMatrix64(_m,m);
    }

    Matrix64(MtxBase<double>& m) {
        aaxMatrix64CopyMatrix64(_m, m.config());
    }

    ~Matrix64() {}

    inline bool translate(double dx, double dy, double dz) {
        return aaxMatrix64Translate(_m,dx,dy,dz);
    }
    bool translate(VecBase<double>& tv) {
        double (&t)[4] = tv.config();
        return aaxMatrix64Translate(_m,t[0],t[1],t[2]);
    }
    inline bool rotate(double a, double x, double y, double z) {
        return aaxMatrix64Rotate(_m,a,x,y,z);
    }
    inline bool multiply(Matrix64& m) {
        return aaxMatrix64Multiply(_m,m.config());
    }
    inline bool multiply(aaxMtx4d& m) {
        return aaxMatrix64Multiply(_m,m);
    }
    inline bool inverse() {
        return aaxMatrix64Inverse(_m);
    }

    // ** support ******
    Matrix64& operator*=(Matrix64& m) {
        aaxMatrix64Multiply(_m,m.config());
        return *this;
    }
    Matrix64& operator/=(Matrix64& m) {
        aaxMtx4d im;
        aaxMatrix64CopyMatrix64(im,m.config());
        aaxMatrix64Inverse(im);
        aaxMatrix64Multiply(_m,im);
        return *this;
    }
    Matrix64& operator*=(aaxMtx4d& m) {
        aaxMatrix64Multiply(_m,m);
    }
    Matrix64& operator/=(aaxMtx4d& m) {
        aaxMtx4d im;
        aaxMatrix64CopyMatrix64(im, m);
        aaxMatrix64Inverse(im);
        aaxMatrix64Multiply(_m,im);
        return *this;
    }
    Matrix64 operator~() {
        aaxMtx4d im;
        aaxMatrix64CopyMatrix64(im,_m);
        aaxMatrix64Inverse(im);
        return Matrix64(im);
    }
    Matrix64& operator=(Matrix64& m) {
        aaxMatrix64CopyMatrix64(_m,m.config());
        return *this;
    }
};

}

#endif /* AEONWAVE_MATRIX */

