// Copyright (C) 2026 Qore Technologies, s.r.o.
// MIT License
// Golden outputs from Qore's pre-0.8.4 xxHash copy: AOT/cache identities must not change.
#define XXH_NO_XXH3
#define XXH_STATIC_LINKING_ONLY
#include <qore/intern/xxhash/xxhash.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

struct Vector {
    size_t length;
    uint64_t seed;
    uint32_t hash32;
    uint64_t hash64;
};

static const Vector vectors[] = {
    {0, UINT64_C(0x0000000000000000), UINT32_C(0x02cc5d05), UINT64_C(0xef46db3751d8e999)},
    {0, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x4a866078), UINT64_C(0x6ec6d05f61c7e7a7)},
    {0, UINT64_C(0xffffffffffffffff), UINT32_C(0x9061da9d), UINT64_C(0x298f4c84b24f5380)},
    {1, UINT64_C(0x0000000000000000), UINT32_C(0x3fe5a29d), UINT64_C(0xf592c0c7639c4cb6)},
    {1, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x7037e2e4), UINT64_C(0x11c15bc64227a259)},
    {1, UINT64_C(0xffffffffffffffff), UINT32_C(0xbba06164), UINT64_C(0x40068883e9dd8859)},
    {3, UINT64_C(0x0000000000000000), UINT32_C(0x4021af28), UINT64_C(0x22c08528601d4f27)},
    {3, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x2f1a6469), UINT64_C(0x680fb1b072b12822)},
    {3, UINT64_C(0xffffffffffffffff), UINT32_C(0x750ca85b), UINT64_C(0x0babdacfea5c0f63)},
    {4, UINT64_C(0x0000000000000000), UINT32_C(0x1eb8f11a), UINT64_C(0xfb1e5cf2f1ae4d95)},
    {4, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x72187336), UINT64_C(0x1a24432ce8cca8ab)},
    {4, UINT64_C(0xffffffffffffffff), UINT32_C(0x487e0038), UINT64_C(0x8d1c55d11990167d)},
    {7, UINT64_C(0x0000000000000000), UINT32_C(0x02226fe7), UINT64_C(0x5613ac510496c04e)},
    {7, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x1ad9f838), UINT64_C(0xeb83e677abc7457f)},
    {7, UINT64_C(0xffffffffffffffff), UINT32_C(0x6f1f72e7), UINT64_C(0xc704098163038854)},
    {8, UINT64_C(0x0000000000000000), UINT32_C(0xe632aaca), UINT64_C(0x0df44920b59ba538)},
    {8, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x54af0651), UINT64_C(0x6d3ad13494f9c9d6)},
    {8, UINT64_C(0xffffffffffffffff), UINT32_C(0xe7c5fff2), UINT64_C(0x2913c17925934138)},
    {15, UINT64_C(0x0000000000000000), UINT32_C(0xe58a8df3), UINT64_C(0xba6143684ceb78e0)},
    {15, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x19b9950c), UINT64_C(0x43198570896acf5d)},
    {15, UINT64_C(0xffffffffffffffff), UINT32_C(0x8457966f), UINT64_C(0x6ee2784a2551df08)},
    {16, UINT64_C(0x0000000000000000), UINT32_C(0x0810fea2), UINT64_C(0x06bfde7f88bb0968)},
    {16, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x1553b236), UINT64_C(0x0b0052b67b424b95)},
    {16, UINT64_C(0xffffffffffffffff), UINT32_C(0x2f9a7411), UINT64_C(0xa0ceb4a10a4179e0)},
    {17, UINT64_C(0x0000000000000000), UINT32_C(0xcc0163da), UINT64_C(0x0547ea26c792e8c2)},
    {17, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x2fef48e3), UINT64_C(0x9df798671dd473fb)},
    {17, UINT64_C(0xffffffffffffffff), UINT32_C(0xcd1e9b33), UINT64_C(0xccf992b1dd1b0791)},
    {31, UINT64_C(0x0000000000000000), UINT32_C(0x01fda953), UINT64_C(0x1d462b23523f66f7)},
    {31, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x9a4857f9), UINT64_C(0xb2283b656cf55584)},
    {31, UINT64_C(0xffffffffffffffff), UINT32_C(0x4d07fbfa), UINT64_C(0x41f21d7506d65cab)},
    {32, UINT64_C(0x0000000000000000), UINT32_C(0xe38b444e), UINT64_C(0xf8c98e2f5a1ab60f)},
    {32, UINT64_C(0x9e3779b185ebca87), UINT32_C(0xcc4d7a40), UINT64_C(0xcebacaf298285aee)},
    {32, UINT64_C(0xffffffffffffffff), UINT32_C(0xa93283a0), UINT64_C(0xecd25673be4cbd91)},
    {33, UINT64_C(0x0000000000000000), UINT32_C(0x5103b5ba), UINT64_C(0xc01b15bc7df527b5)},
    {33, UINT64_C(0x9e3779b185ebca87), UINT32_C(0xc3c46ef6), UINT64_C(0x51dfcd1778e701dc)},
    {33, UINT64_C(0xffffffffffffffff), UINT32_C(0xe0fce298), UINT64_C(0x4a91572bdab59a85)},
    {63, UINT64_C(0x0000000000000000), UINT32_C(0x0e4e4235), UINT64_C(0x5952d19295eddf40)},
    {63, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x12a24ddd), UINT64_C(0x7a12bc49fe24dcc1)},
    {63, UINT64_C(0xffffffffffffffff), UINT32_C(0xd4ea34ef), UINT64_C(0x0399c1272d9124dd)},
    {64, UINT64_C(0x0000000000000000), UINT32_C(0xddd97ad9), UINT64_C(0x32a41aa44b5a1357)},
    {64, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x4f5bbcd7), UINT64_C(0x13c6e711ae4d5c10)},
    {64, UINT64_C(0xffffffffffffffff), UINT32_C(0x95fd1cf4), UINT64_C(0xa5f12ec69195705d)},
    {65, UINT64_C(0x0000000000000000), UINT32_C(0xbddaa395), UINT64_C(0x31026507bbf60932)},
    {65, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x56ddcdf3), UINT64_C(0xe8f4b48f0333cb11)},
    {65, UINT64_C(0xffffffffffffffff), UINT32_C(0xb8800640), UINT64_C(0x42dea0256597e91f)},
    {255, UINT64_C(0x0000000000000000), UINT32_C(0xb5bd27f3), UINT64_C(0x867e83829fb1548d)},
    {255, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x5bbb31c7), UINT64_C(0xe3b79b584d0ebd8a)},
    {255, UINT64_C(0xffffffffffffffff), UINT32_C(0x1e13c0b3), UINT64_C(0xf90592923399738c)},
    {256, UINT64_C(0x0000000000000000), UINT32_C(0x1aa38d96), UINT64_C(0x9539e32c2d2aa50e)},
    {256, UINT64_C(0x9e3779b185ebca87), UINT32_C(0x0fbf4881), UINT64_C(0x9a7755ad95626e9b)},
    {256, UINT64_C(0xffffffffffffffff), UINT32_C(0x840d92e2), UINT64_C(0x34643f4e7d9ce865)},
    {257, UINT64_C(0x0000000000000000), UINT32_C(0x44c3fa30), UINT64_C(0x611c21925636ccb0)},
    {257, UINT64_C(0x9e3779b185ebca87), UINT32_C(0xa416e508), UINT64_C(0xaf7db8e0e543941c)},
    {257, UINT64_C(0xffffffffffffffff), UINT32_C(0x4e14ac99), UINT64_C(0x6a9217c74905ea74)},
    {4096, UINT64_C(0x0000000000000000), UINT32_C(0x4d335d68), UINT64_C(0x7f0e14b29f9c7ab7)},
    {4096, UINT64_C(0x9e3779b185ebca87), UINT32_C(0xb933e414), UINT64_C(0xcac8a4e1b061a72a)},
    {4096, UINT64_C(0xffffffffffffffff), UINT32_C(0x77a4d9e9), UINT64_C(0xafacf608a50f679e)},
};

static void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int main() {
    try {
        alignas(64) std::array<unsigned char, 4104> storage{};
        for (const Vector& vector : vectors) {
            // Test every alignment relative to an eight-byte word.
            for (size_t offset = 0; offset < 8; ++offset) {
                unsigned char* data = storage.data() + offset;
                for (size_t i = 0; i < vector.length; ++i) {
                    data[i] = static_cast<unsigned char>((i * 37 + i / 7 + 11) & 255);
                }
                const uint32_t seed32 = static_cast<uint32_t>(vector.seed);
                check(XXH32(data, vector.length, seed32) == vector.hash32, "XXH32 identity changed");
                check(XXH64(data, vector.length, vector.seed) == vector.hash64, "XXH64 identity changed");
                // Exercise partial stripes, exact stripes and a large update, including empty inputs.
                for (size_t chunk : {size_t(1), size_t(7), size_t(32), size_t(129), size_t(4096)}) {
                    XXH32_state_t state32;
                    XXH64_state_t state64;
                    check(XXH32_reset(&state32, seed32) == XXH_OK, "XXH32 reset failed");
                    check(XXH64_reset(&state64, vector.seed) == XXH_OK, "XXH64 reset failed");
                    check(XXH32_update(&state32, data, 0) == XXH_OK, "XXH32 empty update failed");
                    check(XXH64_update(&state64, data, 0) == XXH_OK, "XXH64 empty update failed");
                    for (size_t pos = 0; pos < vector.length; pos += chunk) {
                        const size_t count = std::min(chunk, vector.length - pos);
                        check(XXH32_update(&state32, data + pos, count) == XXH_OK, "XXH32 update failed");
                        check(XXH64_update(&state64, data + pos, count) == XXH_OK, "XXH64 update failed");
                    }
                    check(XXH32_digest(&state32) == vector.hash32, "XXH32 streaming identity changed");
                    check(XXH64_digest(&state64) == vector.hash64, "XXH64 streaming identity changed");
                }
            }
        }
        std::puts("xxHash: 57 legacy vectors, eight alignments and five streaming chunk sizes passed");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
