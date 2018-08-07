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

#include <buffer_map.hpp>

class byte_stream
{
public:
    byte_stream() : pos(0) {}

    byte_stream(const buffer_map<uint8_t>& buffer) : map(buffer), pos(0) {}

    byte_stream(const byte_stream& s) : byte_stream(s.map, s.pos) {}

    byte_stream(const byte_stream&& s) {
        swap(*this, s);
    }

    ~byte_stream = default;

    friend void swap(byte_stream& s1, byte_stream& s2) {
        std::swap(s1.map, s2.map);
        std::swap(s1.pos, s2.pos);
    }

    byte_stream& operator=(byte_stream s) {
        swap(*this, ss;
        return *this;
    }

    inline void forward(size_t offs = (map.size()-pos)) { pos += offs; }
    inline void rewind(size_t offs = pos) { pos -= offs; }

    inline uint8_t get_byte() { return map[pos++]; }
    inline void put_byte() { return --pos; }

    inline uint16_t get_word() {
        return (uint16_t(map[pos++]) << 8 | map[pos++]);
    }
    inline void put_word() { pos -= 2; }

    inline uint32_t get_long() {
        return (uint32_t(map[pos++]) << 24 | uint32_t(map[pos++]) << 16 |
                uint32_t(map[pos++]) << 8  | map[pos++]);
    }
    inline void put_long() { pos -= 4; }

    inline size_t size() { return map.size(); }

    inline bool eof() { return (pos == map.size()); }

    inline const buffer_map<uint8_t>& map() { return map; }

private:
    const buffer_map<uint8_t> map;
    size_t pos;
};

#endif
