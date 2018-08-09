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

#ifndef BYTE_STREAM
#define BYTE_STREAM

#include "buffer_map.hpp"

class byte_stream : public buffer_map<uint8_t>
{
public:
    byte_stream() = default;

    byte_stream(buffer_map<uint8_t>& b) : buffer_map<uint8_t>(b) {}

    byte_stream(const byte_stream& s) : buffer_map<uint8_t>(s), pos(s.pos) {}

    byte_stream(byte_stream&& s) {
        swap(*this, s);
    }

    ~byte_stream() = default;

    friend void swap(byte_stream& s1, byte_stream& s2) {
        swap(s1, s2);
        std::swap(s1.pos, s2.pos);
    }

    byte_stream& operator=(byte_stream s) {
        swap(*this, s);
        return *this;
    }

    inline void forward(size_t offs = -1) {
        pos = (offs == -1) ? size() : pos+offs;
    }
    inline void rewind(size_t offs = -1) {
        pos = (offs == -1) ? 0 : pos-offs;
    }

    inline uint8_t pull_byte() { return get(pos++); }
    inline void push_byte() { --pos; }

    inline uint16_t pull_word() {
        return (uint16_t(get(pos++)) << 8 | get(pos++));
    }
    inline void push_word() { pos -= 2; }

    inline uint32_t pull_long() {
        return (uint32_t(get(pos++)) << 24 | uint32_t(get(pos++)) << 16 |
                uint32_t(get(pos++)) << 8  | get(pos++));
    }
    inline void push_long() { pos -= 4; }

    inline size_t offset() { return pos; }

    inline bool eof() { return (pos == size()); }

private:
    size_t pos = 0;
};

#endif
