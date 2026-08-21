// room_render.cpp — render one GoldenEye room, end to end, through our own code.
//
// This is the M2 milestone in its smallest honest form: take a real room's
// display list out of the game, walk it with our interpreter, transform the
// vertices with our pipeline, and rasterise the triangles to an image.
//
// It is NOT the shipping renderer. RT64 is, and RT64 already builds and already
// speaks this dialect (see RT64-INTEGRATION.md). But RT64 is a mature renderer
// whose correctness was never the risk here — the risk was whether OUR chain
// produces correct geometry to hand it. A software rasteriser answers that
// directly, and it runs anywhere, including headless CI with no GPU.
//
// Deliberately untextured. Texturing is a later pipeline stage; flat-shaded
// geometry is what shows whether the display list, the vertex cache, the
// segment resolution, the matrix conventions and the viewport transform are all
// correct. A texture would only hide errors in those.
//
// Usage: room_render <room_pair.bin> <room-index> <out.ppm> [width] [height]

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gbi/gbi.h"
#include "gbi/gbi_interp.h"
#include "rhi/vertex_pipeline.h"
#include "ultra/rdram.h"

using namespace ge_gbi;
using namespace ge_rhi;

namespace {

struct Room {
    float origin[3] = {0, 0, 0};
    std::vector<Vtx> verts;
    std::vector<Gfx> dl;
};

bool readPairs(const char* path, std::vector<Room>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    uint32_t n = 0;
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "GERP", 4) != 0) {
        std::fclose(f);
        return false;
    }
    if (std::fread(&n, 4, 1, f) != 1) { std::fclose(f); return false; }
    for (uint32_t i = 0; i < n; ++i) {
        Room r;
        uint32_t nv = 0, dlb = 0;
        if (std::fread(r.origin, 4, 3, f) != 3) break;
        if (std::fread(&nv, 4, 1, f) != 1) break;
        r.verts.resize(nv);
        if (std::fread(r.verts.data(), sizeof(Vtx), nv, f) != nv) break;
        if (std::fread(&dlb, 4, 1, f) != 1) break;
        r.dl.resize(dlb / sizeof(Gfx));
        if (std::fread(r.dl.data(), 1, dlb, f) != dlb) break;
        out.push_back(std::move(r));
    }
    std::fclose(f);
    return !out.empty();
}

// Collects triangles as the interpreter walks the list, keeping a copy of the
// vertices each one referenced. The interpreter hands over cache slots, and the
// cache is reloaded constantly (16 entries for a whole room), so the vertices
// must be captured at draw time — reading them afterwards gets whatever the
// last G_VTX left behind.
struct TriangleSink : IDrawSink {
    struct Tri3 { Vtx v[3]; };
    std::vector<Tri3> tris;
    uint32_t skipped_invalid = 0;

    void drawTriangle(const Tri& t, const CachedVtx cache[kVertexCacheSize]) override {
        Tri3 out;
        for (int i = 0; i < 3; ++i) {
            if (!cache[t.v[i]].valid) { ++skipped_invalid; return; }
            out.v[i] = cache[t.v[i]].raw;
        }
        tris.push_back(out);
    }
    void setProjection(const float[4][4]) override {}
    void setModelview(const float[4][4]) override {}
    void setGeometryMode(uint32_t) override {}
    void setOtherModeH(uint32_t) override {}
    void setOtherModeL(uint32_t) override {}
    void setTexture(const TextureState&) override {}
    void rdpCommand(uint32_t, uint32_t) override {}
};

Mat4 perspective(float fovy_deg, float aspect, float n, float f) {
    Mat4 r{};
    const float fovy = fovy_deg * 3.1415926f / 180.0f;
    const float cot = std::cos(fovy / 2) / std::sin(fovy / 2);
    r.m[0][0] = cot / aspect;
    r.m[1][1] = cot;
    r.m[2][2] = (n + f) / (n - f);
    r.m[2][3] = -1.0f;
    r.m[3][2] = (2 * n * f) / (n - f);
    return r;
}

Mat4 lookAtOrigin(float dist, float yaw, float pitch) {
    // Orbit the room centre. Row-vector convention throughout.
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    Mat4 ry = identity();
    ry.m[0][0] = cy;  ry.m[0][2] = -sy;
    ry.m[2][0] = sy;  ry.m[2][2] = cy;
    Mat4 rx = identity();
    rx.m[1][1] = cp;  rx.m[1][2] = sp;
    rx.m[2][1] = -sp; rx.m[2][2] = cp;
    return multiply(multiply(ry, rx), translate(0.0f, 0.0f, -dist));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "usage: %s <room_pair.bin> <room-index> <out.ppm> [w] [h]\n",
                     argv[0]);
        return 2;
    }
    const int want = std::atoi(argv[2]);
    const int W = (argc > 4) ? std::atoi(argv[4]) : 640;
    const int H = (argc > 5) ? std::atoi(argv[5]) : 480;

    std::vector<Room> rooms;
    if (!readPairs(argv[1], rooms)) {
        std::fprintf(stderr, "could not read %s\n", argv[1]);
        return 2;
    }
    if (want >= int(rooms.size())) {
        std::fprintf(stderr, "room %d out of range (%zu rooms)\n", want,
                     rooms.size());
        return 2;
    }
    // Index -1 renders every room at its declared origin — the visual form of
    // what geom_validate checks numerically. If the matrix convention were
    // transposed, the rooms would pile onto one another instead of forming a
    // level.
    const bool whole_level = (want < 0);
    if (!ge_ultra::rdramInit()) return 2;
    ge_ultra::rdramResetStats();

    // --- lay the room out in RDRAM exactly as the game does -----------------
    // Vertices live at a base that segment 14 (SPSEGMENT_BG_VTX) points at, and
    // the display list references them as 0x0E00xxxx. src/game/bg.c:2688.
    constexpr uint32_t kVtxBase = 0x200000;
    constexpr uint32_t kDlBase = 0x210000;

    TriangleSink sink;
    uint32_t total_tris = 0, total_verts = 0, total_settex = 0;
    int rooms_walked = 0, walks_failed = 0;

    const int first = whole_level ? 0 : want;
    const int last = whole_level ? int(rooms.size()) - 1 : want;

    for (int ri = first; ri <= last; ++ri) {
        const Room& room = rooms[ri];
        if (room.verts.empty() || room.dl.empty()) continue;

        std::memcpy(ge_ultra::rdramBase() + kVtxBase, room.verts.data(),
                    room.verts.size() * sizeof(Vtx));
        std::memset(ge_ultra::rdramBase() + kDlBase, 0, 0x10000);
        std::memcpy(ge_ultra::rdramBase() + kDlBase, room.dl.data(),
                    room.dl.size() * sizeof(Gfx));

        // Each room is walked with its own interpreter: the vertex cache and
        // segment table are per-room state, and carrying them across rooms would
        // let one room's stale cache satisfy another's triangles.
        const size_t before = sink.tris.size();
        Interpreter interp(ge_ultra::rdramResolver(), &sink);
        interp.setSegment(14, kVtxBase);
        if (!interp.run(kDlBase)) ++walks_failed;

        // Move this room's triangles into world space by its declared origin.
        if (whole_level)
            for (size_t i = before; i < sink.tris.size(); ++i)
                for (int k = 0; k < 3; ++k) {
                    sink.tris[i].v[k].x = int16_t(sink.tris[i].v[k].x + room.origin[0]);
                    sink.tris[i].v[k].y = int16_t(sink.tris[i].v[k].y + room.origin[1]);
                    sink.tris[i].v[k].z = int16_t(sink.tris[i].v[k].z + room.origin[2]);
                }

        total_tris += interp.stats().triangles;
        total_verts += interp.stats().vertices_loaded;
        total_settex += interp.stats().settex_seen;
        ++rooms_walked;
    }

    if (whole_level)
        std::printf("whole level: %d rooms walked\n", rooms_walked);
    else
        std::printf("room %d: %zu verts, %zu commands\n", want,
                    rooms[want].verts.size(), rooms[want].dl.size());
    std::printf("  %u triangles, %u vertices loaded, %d walks bailed\n",
                total_tris, total_verts, walks_failed);
    std::printf("  G_SETTEX seen: %u (expected - CPU-expanded by tex.c)\n",
                total_settex);
    if (sink.skipped_invalid)
        std::printf("  WARNING: %u triangles referenced unloaded slots\n",
                    sink.skipped_invalid);
    if (sink.tris.empty()) {
        std::fprintf(stderr, "no triangles produced\n");
        return 1;
    }

    // --- frame the camera on the room's own bounds --------------------------
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (const auto& t : sink.tris)
        for (int i = 0; i < 3; ++i) {
            const float p[3] = {float(t.v[i].x), float(t.v[i].y), float(t.v[i].z)};
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], p[k]);
                hi[k] = std::max(hi[k], p[k]);
            }
        }
    const float radius = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]}) * 0.5f;
    const float dist = std::max(radius * 2.6f, 40.0f);
    std::printf("  bounds %.0f x %.0f x %.0f, camera at %.0f units\n",
                hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], dist);

    // Inspection camera, NOT the game's projection. The game clips at
    // znear=10 / zfar=300 (src/game/bondview2.c:8423) while rooms average ~576
    // units across, so a game-accurate frustum cannot contain a whole room —
    // that short draw distance is why GoldenEye is so foggy. For verifying
    // geometry we want to see all of it at once.
    const Mat4 proj = perspective(45.0f, float(W) / float(H), 1.0f, dist * 4.0f);
    const Mat4 view = lookAtOrigin(dist, 0.7f, 0.45f);
    const Mat4 centre = translate(-(lo[0] + hi[0]) * 0.5f, -(lo[1] + hi[1]) * 0.5f,
                                  -(lo[2] + hi[2]) * 0.5f);
    const Mat4 mvp = multiply(multiply(centre, view), proj);

    Viewport vp;
    vp.vscale[0] = int16_t(W * 2);
    vp.vscale[1] = int16_t(-H * 2);   // flip Y: screen rows go down
    vp.vscale[2] = 511;
    vp.vtrans[0] = int16_t(W * 2);
    vp.vtrans[1] = int16_t(H * 2);
    vp.vtrans[2] = 511;

    // --- rasterise ----------------------------------------------------------
    std::vector<uint8_t> rgb(size_t(W) * H * 3, 24);
    std::vector<float> depth(size_t(W) * H, 1e30f);
    int drawn = 0, clipped = 0;

    for (const auto& t : sink.tris) {
        TransformedVtx tv[3];
        bool usable = true;
        for (int i = 0; i < 3; ++i) {
            tv[i] = transformVertex(t.v[i], mvp, vp, 0xFFFF);
            if (tv[i].behind_near) usable = false;
        }
        // No near-plane clipping here — a vertex behind the eye would project to
        // nonsense. Dropping the triangle is correct for an inspection render
        // and keeps the rasteriser honest about what it does not do.
        if (!usable) { ++clipped; continue; }
        if (triangleRejected(tv[0], tv[1], tv[2])) { ++clipped; continue; }

        const float x0 = tv[0].screen[0], y0 = tv[0].screen[1];
        const float x1 = tv[1].screen[0], y1 = tv[1].screen[1];
        const float x2 = tv[2].screen[0], y2 = tv[2].screen[1];
        const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
        if (std::fabs(area) < 1e-6f) { ++clipped; continue; }

        // Flat shade from the triangle's own vertex colours, darkened by facing
        // so structure reads. The game stores either colours or normals in these
        // bytes depending on G_LIGHTING; either way the value varies per surface,
        // which is all this needs.
        int cr = 0, cg = 0, cb = 0;
        for (int i = 0; i < 3; ++i) {
            cr += t.v[i].color.r; cg += t.v[i].color.g; cb += t.v[i].color.b;
        }
        const float facing = 0.35f + 0.65f * std::fabs(area) /
                                          (std::fabs(area) + 4000.0f);
        cr = int(cr / 3 * facing); cg = int(cg / 3 * facing); cb = int(cb / 3 * facing);
        cr = std::clamp(cr, 12, 255);
        cg = std::clamp(cg, 12, 255);
        cb = std::clamp(cb, 12, 255);

        const int minx = std::max(0, int(std::floor(std::min({x0, x1, x2}))));
        const int maxx = std::min(W - 1, int(std::ceil(std::max({x0, x1, x2}))));
        const int miny = std::max(0, int(std::floor(std::min({y0, y1, y2}))));
        const int maxy = std::min(H - 1, int(std::ceil(std::max({y0, y1, y2}))));
        const float inv_area = 1.0f / area;

        for (int y = miny; y <= maxy; ++y)
            for (int x = minx; x <= maxx; ++x) {
                const float px = float(x) + 0.5f, py = float(y) + 0.5f;
                float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * inv_area;
                float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * inv_area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                const float z = w0 * tv[0].screen[2] + w1 * tv[1].screen[2] +
                                w2 * tv[2].screen[2];
                const size_t idx = size_t(y) * W + x;
                if (z >= depth[idx]) continue;
                depth[idx] = z;
                rgb[idx * 3 + 0] = uint8_t(cr);
                rgb[idx * 3 + 1] = uint8_t(cg);
                rgb[idx * 3 + 2] = uint8_t(cb);
            }
        ++drawn;
    }

    std::printf("  rasterised %d triangles (%d clipped/rejected)\n", drawn, clipped);
    std::printf("  failed address resolutions: %llu\n",
                (unsigned long long)ge_ultra::rdramBadResolveCount());

    FILE* out = std::fopen(argv[3], "wb");
    if (!out) { std::fprintf(stderr, "cannot write %s\n", argv[3]); return 2; }
    std::fprintf(out, "P6\n%d %d\n255\n", W, H);
    std::fwrite(rgb.data(), 1, rgb.size(), out);
    std::fclose(out);
    std::printf("  wrote %s\n", argv[3]);

    ge_ultra::rdramShutdown();
    return drawn > 0 ? 0 : 1;
}
