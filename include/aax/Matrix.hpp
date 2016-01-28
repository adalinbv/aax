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

#include <stdio.h>

#include <cmath>
#include <cfloat>
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

    VecBase(float v[4]) : _v4(true) {
        _v[0] = v[0]; _v[1] = v[1]; _v[2] = v[2]; _v[3] = v[3];
    }

    VecBase(double v[4]) : _v4(true) {
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

    T magnitude() {
        T m = _v[0]*_v[0] + _v[1]*_v[1] + _v[2]*_v[2];
        if (_v4) m += _v[3]*_v[3];
        return sqrt(m);
    }

    T normalize() {
        float m = magnitude();
        if (m) {
            T fi = 1/m;
            _v[0] *= fi; _v[1] *= fi; _v[2] *= fi;
            if (_v4) _v[3] *= fi;
        } else {
            _v[0] = _v[1] = _v[2] = _v[3] = 0;
        }
        return m;
    }

    VecBase<T> normalized() {
        VecBase<T> r;
        float m = magnitude();
        if (m) {
            T fi = 1/m;
            r[0] = _v[0]*fi; r[1] = _v[1]*fi; r[2] = _v[2]*fi;
            if (_v4) r[3] = _v[3]*fi;
        }
        return r;
    }

    T dot(VecBase<T>& v) {
        T d = _v[0]*v[0] + _v[1]*v[1] + _v[2]*v[2];
        if (_v4) d += _v[3]*v[3];
        return d;
    }

    VecBase<T> cross(VecBase<T>& v2) {
        VecBase<T> r;
        if (_v4 == false) {
            r[0] = _v[1]*v2[2] - _v[2]*v2[1];
            r[1] = _v[2]*v2[0] - _v[0]*v2[2];
            r[2] = _v[0]*v2[1] - _v[1]*v2[0];
        }
        return r;
    }

    // ** support ******
    VecBase& operator=(VecBase<T>& v4) {
        T (&v)[4] = v4.config();
        _v[0] = v[0]; _v[1] = v[1]; _v[2] = v[2]; _v[3] = v[3];
        _v4 = v4.is_v4();
        return *this;
    }
    VecBase& operator=(T f) {
        _v[0] = f; _v[1] = f; _v[2] = f; _v[3] = f;
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
        return mtxcmp(m.config());
    }
    bool operator==(T (&m)[4][4]) {
        return mtxcmp(m);
    }
    bool operator!=(MtxBase<T>& m) {
        return ~mtxcmp(m.config());
    }
    bool operator!=(T (&m)[4][4]) {
        return ~mtxcmp(m);
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

private:
    bool mtxcmp(T (&m)[4][4]) {
        for(unsigned i=0; i<4; ++i) {
            for(unsigned j=0; j<4; ++j) {
                if (fabs(_m[i][j]-m[i][j])>(FLT_EPSILON)) return false;
            }
        }
        return true;
    }
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

    bool set(Vector64& pos, Vector64& at) {
        Vector64 up(0.0, 1.0, 0.0);
        if (fabs(at[0])<DBL_EPSILON && fabs(at[2])<DBL_EPSILON) {
            up[1] = 0.0f; up[2] = (at[2] < 0.0f) ? -1.0f : 1.0f;
        }
        return set(pos, at, up);
    }

    bool set(Vector64& p, Vector64& a, Vector64& u) {
        Vector64 pos(p[0], p[1], p[2], -1.0);
        Vector64 at(a[0], a[1], a[2]);
        Vector64 up(u[0], u[1], u[2]);
        Vector64 side = at.cross(up);
        set(0, side.normalized());
        set(1, up.normalized());
        set(2, -at.normalized());
        set(3, -pos);
        return true;
    }

    bool set(double (&p)[3], double (&a)[3], double (&u)[3]) {
        Vector64 pos = p, at = a, up = u;
        return set(pos, at, up);
    }
    bool set(float (&p)[3], float (&a)[3], float (&u)[3]) {
        Vector64 pos(p), at(a), up(u);
        return set(pos, at, up);
    }

    inline bool translate(double dx, double dy, double dz) {
        return aaxMatrix64Translate(_m,dx,dy,dz);
    }
    bool translate(Vector64& tv) {
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

private:
    void set(unsigned p, Vector64 v) {
        _m[p][0] = v[0]; _m[p][1] = v[1]; _m[p][2] = v[2];
        if (v.is_v4()) _m[p][3] = v[3];
    }
};

} // namespace AAX

#endif /* AEONWAVE_MATRIX */

