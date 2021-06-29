/*
 * Written by Erik Hofman, 2018.
 *
 * Public Domain (www.unlicense.org)
 *
 * This is free and unencumbered software released into the public domain.

 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute
 * this software, either in source code form or as a compiled binary, for any
 * purpose, commercial or non-commercial, and by any means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors of this
 * software dedicate any and all copyright interest in the software to the
 * public domain. We make this dedication for the benefit of the public at
 * large and to the detriment of our heirs and successors. We intend this
 * dedication to be an overt act of relinquishment in perpetuity of all present
 * and future rights to this software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef BUFFER_MAP
#define BUFFER_MAP

#include <algorithm>
#include <iostream>

template<typename T>
class buffer_map
{
public:
    buffer_map() = default;

    buffer_map(T* ptr, size_t size) : length(size), buf(ptr) {}

    buffer_map(const buffer_map& b, size_t size = 0)
        : length(size ? size : b.length), buf(b.buf) {}

    buffer_map(buffer_map&& b) = default;

    virtual ~buffer_map() = default;

    friend void swap(buffer_map& b1, buffer_map& b2) noexcept {
        std::swap(b1.length, b2.length);
        std::swap(b1.buf, b2.buf);
    }

    buffer_map& operator=(const buffer_map& b) = default;

    void assign(T* ptr, size_t size) {
        length = size;
        buf = ptr;
    }

    const T& operator[](size_t idx) const {
        if (idx < length) return buf[idx];
        std::cerr << "index beyond buffer length" << std::endl;
        return _error;
    }

    T operator[](size_t idx) {
        if (idx < length) return buf[idx];
        std::cerr << "index beyond buffer length" << std::endl;
        return _error;
    }

    inline T& get(size_t idx) const {
        if (idx < length) return buf[idx];
        std::cerr << "index beyond buffer length" << std::endl;
        static unsigned char error = 0;
        return error;
    }

    operator T*() const {
        return buf;
    }

    size_t size() {
        return length;
    }

private:
    static const unsigned char _error = 0;
    size_t length = 0;
    T *buf = nullptr;
};

#endif
