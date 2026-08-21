// test_gbi.cpp — differential tests for the gsp3D display-list interpreter.
//
// The interesting tests here are DIFFERENTIAL: the game itself contains a
// CPU-side G_TRI4 decoder (extract_vertex_indices_from_triangle,
// src/game/lightfixture.c:195-227) written by the decomp authors from the real
// microcode. That function is an independent oracle. It is transcribed verbatim
// below, and the interpreter is checked against it over exhaustive inputs.
//
// This matters because a G_TRI4 nibble-order bug does not crash — it renders
// triangles with permuted winding, which looks like backface-culling problems
// and gets misdiagnosed for days.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gbi/gbi.h"
#include "gbi/gbi_interp.h"

using namespace ge_gbi;

static int g_failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) { std::printf("  FAIL: %s\n", what); ++g_failures; }
}

static void checkQuiet(bool cond, const char* what) {
    if (!cond) { std::printf("  FAIL: %s\n", what); ++g_failures; }
    else std::printf("  ok:   %s\n", what);
}

// ---------------------------------------------------------------------------
// ORACLE: verbatim transcription of the game's own decoder.
// src/game/lightfixture.c:195-227. Byte/word indexing preserved exactly,
// including the odd-looking casts, because the whole point is to reproduce its
// behaviour rather than a tidied-up version of it.
// ---------------------------------------------------------------------------
static void oracle_extract(const Gfx* gfx, uint32_t tri_type,
                           int32_t* idx1, int32_t* idx2, int32_t* idx3) {
    // Byte-order note: the game runs big-endian. The Gfx words are stored
    // big-endian in RDRAM, so byte 0 of the struct is the top byte of w0. We
    // build a big-endian byte image to index through, matching hardware.
    uint8_t b[8];
    b[0] = uint8_t(gfx->w0 >> 24); b[1] = uint8_t(gfx->w0 >> 16);
    b[2] = uint8_t(gfx->w0 >> 8);  b[3] = uint8_t(gfx->w0 >> 0);
    b[4] = uint8_t(gfx->w1 >> 24); b[5] = uint8_t(gfx->w1 >> 16);
    b[6] = uint8_t(gfx->w1 >> 8);  b[7] = uint8_t(gfx->w1 >> 0);

    auto u16at = [&](int i) -> uint16_t {  // ((u16*)gfx)[i], big-endian
        return uint16_t((b[i * 2] << 8) | b[i * 2 + 1]);
    };
    const uint32_t w0 = gfx->w0, w1 = gfx->w1;

    switch (tri_type) {
        case 1:  // sub-triangle 1
            *idx1 = int32_t(w1 & 0xF);
            *idx2 = int32_t(b[7] >> 4);
            *idx3 = int32_t(w0 & 0xF);
            break;
        case 2:
            *idx1 = int32_t(b[6] & 0xF);
            *idx2 = int32_t(u16at(3) >> 12);
            *idx3 = int32_t(b[3] >> 4);
            break;
        case 3:
            *idx1 = int32_t(u16at(2) & 0xF);
            *idx2 = int32_t(b[5] >> 4);
            *idx3 = int32_t(b[2] & 0xF);
            break;
        case 4:
            *idx1 = int32_t(b[4] & 0xF);
            *idx2 = int32_t(w1 >> 28);
            *idx3 = int32_t(u16at(1) >> 12);
            break;
        default:
            *idx1 = *idx2 = *idx3 = -1;
            break;
    }
}

// The gSP4Triangles macro from include/gbi_extension.h:103-120, transcribed.
static Gfx encodeTri4(int x1, int y1, int z1, int x2, int y2, int z2,
                      int x3, int y3, int z3, int x4, int y4, int z4) {
    Gfx g;
    g.w0 = (uint32_t(G_TRI4) << 24) | (uint32_t(z4) << 12) | (uint32_t(z3) << 8) |
           (uint32_t(z2) << 4) | (uint32_t(z1) << 0);
    g.w1 = (uint32_t(y4) << 28) | (uint32_t(x4) << 24) | (uint32_t(y3) << 20) |
           (uint32_t(x3) << 16) | (uint32_t(y2) << 12) | (uint32_t(x2) << 8) |
           (uint32_t(y1) << 4) | (uint32_t(x1) << 0);
    return g;
}

// ---------------------------------------------------------------------------
// 1. Exhaustive differential test against the game's own decoder.
// ---------------------------------------------------------------------------
static void testTri4Differential() {
    std::printf("[G_TRI4 vs the game's own CPU decoder]\n");

    int compared = 0, mismatched = 0;

    // Sweep every nibble value in every position for each slot independently,
    // with the other slots held at values that keep w1 nonzero so the
    // terminator does not cut the command short.
    for (int slot = 0; slot < 4; ++slot) {
        for (int x = 0; x < 16; ++x) {
            for (int y = 0; y < 16; ++y) {
                for (int z = 0; z < 16; ++z) {
                    int X[4] = {1, 1, 1, 1}, Y[4] = {1, 1, 1, 1}, Z[4] = {1, 1, 1, 1};
                    X[slot] = x; Y[slot] = y; Z[slot] = z;

                    const Gfx g = encodeTri4(X[0], Y[0], Z[0], X[1], Y[1], Z[1],
                                             X[2], Y[2], Z[2], X[3], Y[3], Z[3]);
                    Tri out[4];
                    const int n = decodeTri4(g.w0, g.w1, out);
                    if (n <= slot) continue;  // terminated before this slot

                    int32_t a = 0, b = 0, c = 0;
                    oracle_extract(&g, uint32_t(slot + 1), &a, &b, &c);

                    ++compared;
                    if (out[slot].v[0] != a || out[slot].v[1] != b ||
                        out[slot].v[2] != c) {
                        if (++mismatched <= 5) {
                            std::printf("    slot %d (x=%d y=%d z=%d): got (%u,%u,%u) "
                                        "oracle (%d,%d,%d)\n",
                                        slot, x, y, z, out[slot].v[0], out[slot].v[1],
                                        out[slot].v[2], a, b, c);
                        }
                    }
                }
            }
        }
    }

    std::printf("  compared %d sub-triangle decodes\n", compared);
    check(compared > 10000, "differential sweep actually ran");
    checkQuiet(mismatched == 0, "interpreter agrees with the game's decoder everywhere");
}

// ---------------------------------------------------------------------------
// 2. The real hand-encoded command from src/game/glass2.c:449-452.
//
// This is ground truth from the shipping game: a 4-vertex window rendered as a
// quad. If our decode disagrees with this, we are wrong.
// ---------------------------------------------------------------------------
static void testGlass2RealCommand() {
    std::printf("[real G_TRI4 from glass2.c: w0=0xB1000032 w1=0x2110]\n");

    Gfx g{0xB1000032u, 0x00002110u};
    Tri out[4];
    const int n = decodeTri4(g.w0, g.w1, out);

    checkQuiet(n == 2, "decodes to exactly 2 triangles (a quad)");
    if (n >= 1) {
        checkQuiet(out[0].v[0] == 0 && out[0].v[1] == 1 && out[0].v[2] == 2,
                   "triangle 0 == (0,1,2)");
    }
    if (n >= 2) {
        checkQuiet(out[1].v[0] == 1 && out[1].v[1] == 2 && out[1].v[2] == 3,
                   "triangle 1 == (1,2,3)");
    }
    // Preceded by gSPVertex(gdl++, arg1, 4, 0) — 4 vertices, so every index
    // must be in range.
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < 3; ++k)
            check(out[i].v[k] < 4, "index within the 4 loaded vertices");
}

// ---------------------------------------------------------------------------
// 3. Termination semantics — the subtle one.
// ---------------------------------------------------------------------------
static void testTri4Termination() {
    std::printf("[G_TRI4 termination semantics]\n");

    // Trailing zero slots terminate.
    {
        Gfx g = encodeTri4(1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0);
        Tri out[4];
        checkQuiet(decodeTri4(g.w0, g.w1, out) == 2, "trailing zero slots terminate");
    }
    // Four full triangles.
    {
        Gfx g = encodeTri4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        Tri out[4];
        checkQuiet(decodeTri4(g.w0, g.w1, out) == 4, "four triangles decode");
    }
    // Entirely empty.
    {
        Gfx g = encodeTri4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        Tri out[4];
        checkQuiet(decodeTri4(g.w0, g.w1, out) == 0, "all-zero command draws nothing");
    }
    // The trap: an interior (x=0,y=0) slot followed by a NONZERO slot does NOT
    // terminate, because the microcode tests the whole remaining w1. The zero
    // slot is emitted as a degenerate triangle. An interpreter that "helpfully"
    // skips it disagrees with hardware.
    {
        Gfx g = encodeTri4(0, 0, 5, 3, 4, 6, 0, 0, 0, 0, 0, 0);
        Tri out[4];
        const int n = decodeTri4(g.w0, g.w1, out);
        checkQuiet(n == 2, "interior zero slot does NOT terminate early");
        if (n >= 1)
            checkQuiet(out[0].v[0] == 0 && out[0].v[1] == 0 && out[0].v[2] == 5,
                       "interior zero slot emitted as degenerate (0,0,5)");
    }
    // z nibbles are never examined by the terminator.
    {
        Gfx g = encodeTri4(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        Tri out[4];
        checkQuiet(decodeTri4(g.w0, g.w1, out) == 1, "z=0 alone does not terminate");
    }
}

// ---------------------------------------------------------------------------
// 4. Matrix conversion — the non-interleaved fixed-point layout.
// ---------------------------------------------------------------------------
static void testMtxConversion() {
    std::printf("[N64 fixed-point matrix -> float]\n");

    Mtx m{};
    // Identity in N64 form: integer part 1 on the diagonal, fraction all zero.
    for (int i = 0; i < 4; ++i) m.intpart[i * 4 + i] = 1;

    float f[4][4];
    mtxToFloat(m, f);
    bool ident = true;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (f[i][j] != (i == j ? 1.0f : 0.0f)) ident = false;
    checkQuiet(ident, "identity round-trips");

    // A negative value with a fractional part: -1.5 => int -2, frac 0x8000.
    Mtx n{};
    n.intpart[0] = uint16_t(int16_t(-2));
    n.fracpart[0] = 0x8000;
    mtxToFloat(n, f);
    checkQuiet(f[0][0] == -1.5f, "negative fractional value (-1.5) converts");

    // Confirm the layout is NOT interleaved: writing fracpart[1] must affect
    // element [0][1], not element [0][0].
    Mtx p{};
    p.intpart[1] = 3;
    mtxToFloat(p, f);
    checkQuiet(f[0][1] == 3.0f && f[0][0] == 0.0f,
               "int/frac blocks are separate, not interleaved");
}

// ---------------------------------------------------------------------------
// 5. Interpreter end-to-end over a synthetic display list.
// ---------------------------------------------------------------------------
namespace {

struct RecordingSink : IDrawSink {
    std::vector<Tri> tris;
    std::vector<Vtx> tri_verts;
    int projections = 0, modelviews = 0, rdp = 0;
    uint32_t geom = 0;

    void drawTriangle(const Tri& t, const CachedVtx cache[kVertexCacheSize]) override {
        tris.push_back(t);
        for (int i = 0; i < 3; ++i) tri_verts.push_back(cache[t.v[i]].raw);
    }
    void setProjection(const float[4][4]) override { ++projections; }
    void setModelview(const float[4][4]) override { ++modelviews; }
    void setGeometryMode(uint32_t m) override { geom = m; }
    void setOtherModeH(uint32_t) override {}
    void setOtherModeL(uint32_t) override {}
    void setTexture(const TextureState&) override {}
    void rdpCommand(uint32_t, uint32_t) override { ++rdp; }
};

// A flat 1 MiB "RDRAM".
struct FakeRdram {
    std::vector<uint8_t> mem = std::vector<uint8_t>(1u << 20, 0);

    template <typename T>
    void write(uint32_t addr, const T& v) {
        std::memcpy(mem.data() + addr, &v, sizeof(T));
    }
    void writeGfx(uint32_t addr, uint32_t w0, uint32_t w1) {
        Gfx g{w0, w1};
        write(addr, g);
    }
    RdramResolver resolver() {
        return [this](uint32_t a, size_t len) -> const void* {
            if (a + len > mem.size()) return nullptr;
            return mem.data() + a;
        };
    }
};

}  // namespace

static void testInterpreterEndToEnd() {
    std::printf("[interpreter over a synthetic display list]\n");

    FakeRdram ram;
    RecordingSink sink;
    Interpreter interp(ram.resolver(), &sink);

    // Four vertices at 0x1000, tagged so we can identify them.
    for (int i = 0; i < 4; ++i) {
        Vtx v{};
        v.x = int16_t(100 + i);
        v.y = int16_t(200 + i);
        v.z = int16_t(300 + i);
        ram.write(0x1000 + i * 16, v);
    }

    // Display list at 0x2000. Vertices are referenced through SEGMENT 14
    // (SPSEGMENT_BG_VTX), the way room geometry actually does it — this
    // exercises the segment resolver on the path that matters.
    uint32_t p = 0x2000;
    // gSPSegment(14, 0x1000): G_MOVEWORD, index G_MW_SEGMENT, offset 14*4.
    ram.writeGfx(p, (uint32_t(G_MOVEWORD) << 24) | (uint32_t(14 * 4) << 8) | G_MW_SEGMENT,
                 0x1000);
    p += 8;
    // gSPVertex(seg14 + 0, n=4, v0=0)
    ram.writeGfx(p, (uint32_t(G_VTX) << 24) | (uint32_t(((4 - 1) << 4) | 0) << 16) | (16 * 4),
                 0x0E000000);
    p += 8;
    // The real glass2.c quad.
    ram.writeGfx(p, 0xB1000032u, 0x00002110u);
    p += 8;
    // An RDP command with a segmented image address, to check the fixup.
    ram.writeGfx(p, uint32_t(G_SETTIMG) << 24, 0x0E000040);
    p += 8;
    ram.writeGfx(p, uint32_t(G_ENDDL) << 24, 0);

    const bool ok = interp.run(0x2000);
    checkQuiet(ok, "list executed to G_ENDDL without bailing");
    checkQuiet(sink.tris.size() == 2, "quad produced 2 triangles");
    checkQuiet(interp.stats().vertices_loaded == 4, "4 vertices loaded");
    checkQuiet(interp.segmentedToPhysical(0x0E000040) == 0x1040,
               "segment 14 resolves (0x0E000040 -> 0x1040)");
    checkQuiet(sink.rdp == 1, "RDP command forwarded");

    // The vertices the triangles reference must be the ones we wrote — this is
    // what proves the segment resolution and the cache indexing agree.
    bool verts_ok = (sink.tri_verts.size() == 6);
    if (verts_ok) {
        const uint8_t expect[6] = {0, 1, 2, 1, 2, 3};
        for (int i = 0; i < 6; ++i)
            if (sink.tri_verts[i].x != int16_t(100 + expect[i])) verts_ok = false;
    }
    checkQuiet(verts_ok, "triangles reference the correct cached vertices");
}

// ---------------------------------------------------------------------------
// 6. Guards: runaway lists and nesting.
// ---------------------------------------------------------------------------
static void testGuards() {
    std::printf("[robustness guards]\n");

    FakeRdram ram;
    RecordingSink sink;
    Interpreter interp(ram.resolver(), &sink);

    // A display list that calls itself. On hardware this overflows the RSP's DL
    // stack; here it must terminate rather than blow the host stack.
    ram.writeGfx(0x3000, uint32_t(G_DL) << 24, 0x3000);
    ram.writeGfx(0x3008, uint32_t(G_ENDDL) << 24, 0);
    interp.reset();
    interp.run(0x3000);
    checkQuiet(interp.stats().dl_depth_max <= kDlStackDepth,
               "self-recursive list bounded by the DL stack depth");

    // Unterminated list running off the end of RDRAM must fail cleanly, not
    // read out of bounds.
    FakeRdram ram2;
    RecordingSink sink2;
    Interpreter interp2(ram2.resolver(), &sink2);
    std::memset(ram2.mem.data(), 0, ram2.mem.size());  // all G_SPNOOP
    checkQuiet(!interp2.run(0x0), "unterminated list bails instead of overrunning");
}

int main() {
    testTri4Differential();
    testGlass2RealCommand();
    testTri4Termination();
    testMtxConversion();
    testInterpreterEndToEnd();
    testGuards();

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nall GBI tests passed\n");
    return 0;
}
