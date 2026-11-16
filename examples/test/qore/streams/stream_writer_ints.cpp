/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Checks that every integer writer of the native StreamWriter writes the whole value in the byte order of its name
 * (writei1/2/4/8, writei2/4/8LSB, writeu1/2/4; issue #6324: writeu2() and writeu4() wrote only the low byte), and that
 * the matching StreamReader readers (readi*, readu*, *LSB) read each value back; run by stream-writer.qtest
 */
#include <qore/Qore.h>
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/BinaryInputStream.h"
#include "qore/intern/BinaryOutputStream.h"
#include "qore/intern/StreamReader.h"
#include "qore/intern/StreamWriter.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

static int checks = 0;

static void require(bool valid, const std::string& message) {
    ++checks;
    if (!valid) {
        throw std::runtime_error(message);
    }
}

static std::string hex(const unsigned char* p, size_t len) {
    std::string rv;
    char buf[4];
    for (size_t i = 0; i < len; ++i) {
        snprintf(buf, sizeof(buf), "%02x", p[i]);
        rv += buf;
    }
    return rv;
}

//! the bytes of value in big-endian (MSB first) or little-endian order, width bytes
static std::string expected_bytes(uint64_t value, int width, bool msb) {
    unsigned char b[8];
    for (int i = 0; i < width; ++i) {
        int shift = msb ? (width - 1 - i) * 8 : i * 8;
        b[i] = static_cast<unsigned char>(value >> shift);
    }
    return hex(b, width);
}

//! writes one value with a writer and checks the bytes, then reads it back with the reader and checks the value
static void check(const char* name, int width, bool msb, uint64_t bits, int64 expected_value,
        const std::function<int(StreamWriter&, ExceptionSink*)>& write,
        const std::function<int64(StreamReader&, ExceptionSink*)>& read) {
    ExceptionSink xsink;
    BinaryOutputStream* os = new BinaryOutputStream;
    ReferenceHolder<BinaryOutputStream> os_holder(os, &xsink);
    os->ref();
    {
        ReferenceHolder<StreamWriter> writer(new StreamWriter(&xsink, os), &xsink);
        require(!xsink, std::string(name) + ": constructor");
        require(!write(**writer, &xsink) && !xsink, std::string(name) + ": write");
    }
    SimpleRefHolder<BinaryNode> data(os->getData(&xsink));
    require(!xsink && data, std::string(name) + ": data");
    std::string got = hex(static_cast<const unsigned char*>(data->getPtr()), data->size());
    std::string exp = expected_bytes(bits, width, msb);
    require(got == exp, std::string(name) + ": wrote " + got + ", expected " + exp);

    ReferenceHolder<StreamReader> reader(new StreamReader(&xsink, new BinaryInputStream(*data)), &xsink);
    require(!xsink, std::string(name) + ": reader");
    int64 v = read(**reader, &xsink);
    require(!xsink && v == expected_value, std::string(name) + ": read back " + std::to_string(v) + ", expected "
        + std::to_string(expected_value));
}

int main() {
    qore_init(QL_MIT);
    try {
        for (uint64_t v : {0x0ull, 0x1ull, 0x7full, 0x80ull, 0xffull, 0x100ull, 0x1234ull, 0x7fffull, 0x8000ull,
                0xffffull}) {
            check("writeu2", 2, true, v, (int64)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writeu2((uint16_t)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readu2(x); });
            check("writei2", 2, true, v, (int16_t)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei2((int16_t)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readi2(x); });
            check("writei2LSB", 2, false, v, (int16_t)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei2LSB((int16_t)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readi2LSB(x); });
            check("writei2LSB/readu2LSB", 2, false, v, (int64)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei2LSB((int16_t)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readu2LSB(x); });
        }
        for (uint64_t v : {0x0ull, 0xffull, 0x1234ull, 0x12345678ull, 0x7fffffffull, 0x80000000ull,
                0xdeadbeefull, 0xffffffffull}) {
            check("writeu4", 4, true, v, (int64)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writeu4((uint32_t)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readu4(x); });
            check("writei4", 4, true, v, (int32_t)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei4((int32_t)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readi4(x); });
            check("writei4LSB", 4, false, v, (int32_t)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei4LSB((int32_t)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readi4LSB(x); });
            check("writei4LSB/readu4LSB", 4, false, v, (int64)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei4LSB((int32_t)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readu4LSB(x); });
        }
        for (uint64_t v : {0x0ull, 0xffull, 0x123456789abcdef0ull, 0x7fffffffffffffffull, 0x8000000000000000ull,
                0xffffffffffffffffull}) {
            check("writei8", 8, true, v, (int64)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei8((int64)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readi8(x); });
            check("writei8LSB", 8, false, v, (int64)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei8LSB((int64)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readi8LSB(x); });
        }
        for (uint64_t v : {0x0ull, 0x7full, 0x80ull, 0xffull}) {
            check("writeu1", 1, true, v, (int64)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writeu1((unsigned char)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readu1(x); });
            check("writei1", 1, true, v, (signed char)v,
                [v](StreamWriter& w, ExceptionSink* x) { return w.writei1((signed char)v, x); },
                [](StreamReader& r, ExceptionSink* x) { return r.readi1(x); });
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "FAIL: %s\n", e.what());
        qore_cleanup();
        return 1;
    }
    qore_cleanup();
    printf("PASS: %d stream integer checks\n", checks);
    return 0;
}
