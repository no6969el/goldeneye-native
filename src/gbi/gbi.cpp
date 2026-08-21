#include "gbi.h"

#include <cstring>

namespace ge_gbi {

void mtxToFloat(const Mtx& in, float out[4][4]) {
    // libultra's guMtxL2F. The N64 stores all 16 integer parts first, then all
    // 16 fractional parts — NOT interleaved per element. Getting this wrong
    // produces geometry that is subtly, beautifully wrong.
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const int k = i * 4 + j;
            // Assemble in UNSIGNED space, then reinterpret. The obvious form —
            // `(int32_t(int16_t(intpart)) << 16) | fracpart` — left-shifts a
            // negative value, which is undefined behaviour before C++20 and
            // which UBSan flags. It happens to produce the right answer on every
            // compiler anyone will use, and it is still worth not writing.
            const uint32_t bits =
                (uint32_t(in.intpart[k]) << 16) | uint32_t(in.fracpart[k]);
            int32_t fixed;
            std::memcpy(&fixed, &bits, sizeof(fixed));
            out[i][j] = float(fixed) / 65536.0f;
        }
    }
}

int decodeTri4(uint32_t w0, uint32_t w1, Tri out[4]) {
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        // The microcode's test: `beq $24, $0` on the ENTIRE remaining w1, before
        // any nibble extraction. Not a per-triangle "are all three indices zero"
        // check — see the long note in gbi.h.
        if (w1 == 0) break;

        out[n].v[0] = uint8_t((w1 >> 0) & 0xF);   // x
        out[n].v[1] = uint8_t((w1 >> 4) & 0xF);   // y
        out[n].v[2] = uint8_t((w0 >> 0) & 0xF);   // z
        ++n;

        w1 >>= 8;
        w0 >>= 4;
    }
    return n;
}

Tri decodeTri1(uint32_t w0, uint32_t w1) {
    // Stock F3D: gSP1Triangle packs (v)*10 into bytes 2,1,0 of w1... but note
    // GoldenEye builds against plain F3D where gSP1Triangle uses gImmp1 with the
    // indices in w1. The /10 matches the game's own CPU-side decoder in
    // src/game/lightfixture.c:199-201 (case 0), which is the authority here.
    (void)w0;
    Tri t{};
    t.v[0] = uint8_t(((w1 >> 16) & 0xFF) / 10);
    t.v[1] = uint8_t(((w1 >> 8) & 0xFF) / 10);
    t.v[2] = uint8_t(((w1 >> 0) & 0xFF) / 10);
    return t;
}

}  // namespace ge_gbi
