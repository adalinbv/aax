/*
 * Copyright (C) 2018 by Erik Hofman.
 * Copyright (C) 2018 by Adalin B.V.
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

#ifndef BUFFER_MAP
#define BUFFER_MAP

template<typename T>
class buffer_map
{
public:
    buffer_map() : length(0), buf(nullptr) {}

    buffer_map(T* ptr, size_t size) : length(size), buf(ptr) {}

    buffer_map(const buffer_map& b) : buffer_map(b.buf, b.length) {}

    buffer_map(const buffer_map&& b) {
        swap(*this, b);
    }

    ~buffer_map() = default;

    void assign(T* ptr, size_t size) {
        buf = ptr; length = size;
    }

    size_t size() {
        return length;
    }

    friend void swap(buffer_map& b1, buffer_map& b2) {
        std::swap(b1.ptr, b2.ptr);
        std::swap(b1.length, b2.length);
    }

    buffer_map& operator=(buffer_map b) {
        swap(*this, b);
        return *this;
    }

    T& operator[](size_t idx) {
        if (idx < length) return buf[idx];
        throw(std::out_of_range("index beyond buffer length"));
    }

    T operator[](size_t idx) const {
        if (idx < length) return buf[idx];
        throw(std::out_of_range("index beyond buffer length"));
    }

private:
    size_t length;
    T *buf;
};

#endif
