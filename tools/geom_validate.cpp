// geom_validate.cpp — run real GoldenEye room geometry through the vertex
// pipeline and check it behaves.
//
// The display-list corpus proved the interpreter reads the right OPCODES. This
// proves the transform puts geometry in the right PLACE, which is a different
// question and the one that matters for actually rendering a level.
//
// Four checks, in increasing order of what they'd catch:
//
//   1. Room-local vertices are centred near zero and room-sized. Catches wrong
//      vertex stride or endianness.
//   2. origin + local assembles into one coherent level. Catches a transposed
//      translate — which produces geometry that still looks like geometry, just
//      scattered.
//   3. The pipeline produces finite screen coordinates for a camera placed in
//      the level. Catches divide-by-zero and behind-eye handling.
//   4. Culling DISCRIMINATES: a room in front of the camera survives, a room
//      behind it is rejected. This is the check that unblocks restoring
//      G_CULLDL, which is currently a deliberate never-cull.
//
// Check 2 is run twice — once correctly, once with a deliberately transposed
// translate — to demonstrate the test can actually fail. A validation that
// passes either way proves nothing.
//
// Usage: geom_validate <room_geom.bin>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gbi/gbi.h"
#include "rhi/vertex_pipeline.h"

using namespace ge_rhi;

namespace {

struct Room {
    float origin[3] = {0, 0, 0};
    std::vector<ge_gbi::Vtx> verts;
};
struct Level {
    std::string name;
    std::vector<Room> rooms;
};

bool readGeom(const char* path, std::vector<Level>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    uint32_t nlevels = 0;
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "GERG", 4) != 0) {
        std::fclose(f);
        return false;
    }
    if (std::fread(&nlevels, 4, 1, f) != 1) { std::fclose(f); return false; }

    for (uint32_t i = 0; i < nlevels; ++i) {
        Level lv;
        uint32_t nlen = 0, nrooms = 0;
        if (std::fread(&nlen, 4, 1, f) != 1) break;
        lv.name.resize(nlen);
        if (std::fread(lv.name.data(), 1, nlen, f) != nlen) break;
        if (std::fread(&nrooms, 4, 1, f) != 1) break;
        for (uint32_t r = 0; r < nrooms; ++r) {
            Room room;
            uint32_t nv = 0;
            if (std::fread(room.origin, 4, 3, f) != 3) break;
            if (std::fread(&nv, 4, 1, f) != 1) break;
            room.verts.resize(nv);
            if (std::fread(room.verts.data(), sizeof(ge_gbi::Vtx), nv, f) != nv) break;
            lv.rooms.push_back(std::move(room));
        }
        out.push_back(std::move(lv));
    }
    std::fclose(f);
    return !out.empty();
}

struct Bounds {
    float lo[3] = {1e30f, 1e30f, 1e30f};
    float hi[3] = {-1e30f, -1e30f, -1e30f};
    void add(float x, float y, float z) {
        const float p[3] = {x, y, z};
        for (int i = 0; i < 3; ++i) {
            lo[i] = std::min(lo[i], p[i]);
            hi[i] = std::max(hi[i], p[i]);
        }
    }
    float extent(int i) const { return hi[i] - lo[i]; }
    float maxExtent() const {
        return std::max({extent(0), extent(1), extent(2)});
    }
};

// A symmetric perspective in libultra layout, matching guPerspectiveF and the
// game's own near/far (src/game/bondview2.c:8423).
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

int g_failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL: %s\n", what); ++g_failures; }
    else std::printf("  ok:   %s\n", what);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <room_geom.bin>\n", argv[0]);
        return 2;
    }
    std::vector<Level> levels;
    if (!readGeom(argv[1], levels)) {
        std::fprintf(stderr, "could not read %s\n", argv[1]);
        return 2;
    }

    size_t total_rooms = 0, total_verts = 0;
    for (const auto& l : levels) {
        total_rooms += l.rooms.size();
        for (const auto& r : l.rooms) total_verts += r.verts.size();
    }
    std::printf("=== room geometry: %zu levels, %zu rooms, %zu vertices ===\n\n",
                levels.size(), total_rooms, total_verts);

    // --- 1. local vertices are room-centred and room-sized -------------------
    std::printf("[1] room-local vertices\n");
    int off_centre = 0, oversized = 0, empty = 0;
    double sum_extent = 0.0;
    size_t counted = 0;
    for (const auto& l : levels)
        for (const auto& r : l.rooms) {
            if (r.verts.empty()) { ++empty; continue; }
            Bounds b;
            for (const auto& v : r.verts) b.add(v.x, v.y, v.z);
            const float cx = (b.lo[0] + b.hi[0]) * 0.5f;
            const float cz = (b.lo[2] + b.hi[2]) * 0.5f;
            const float ext = std::max(b.extent(0), b.extent(2));
            // Centre should sit near zero relative to the room's own size.
            if (ext > 1.0f && (std::fabs(cx) > ext || std::fabs(cz) > ext))
                ++off_centre;
            // A room-local extent as large as a whole level means the stride or
            // endianness is wrong.
            if (b.maxExtent() > 20000.0f) ++oversized;
            sum_extent += ext;
            ++counted;
        }
    std::printf("  mean room extent: %.0f units\n", sum_extent / double(counted));
    check(off_centre == 0, "every room's vertices are centred on its own origin");
    check(oversized == 0, "no room-local extent exceeds a plausible room size");
    std::printf("  (%d rooms with no vertices)\n", empty);

    // --- 2. rooms assemble into a coherent level ----------------------------
    std::printf("\n[2] level assembly (origin + local)\n");
    double sum_level_extent = 0.0;
    int implausible = 0;
    for (const auto& l : levels) {
        Bounds lb;
        for (const auto& r : l.rooms) {
            const Mat4 m = translate(r.origin[0], r.origin[1], r.origin[2]);
            for (const auto& v : r.verts) {
                const float p[4] = {float(v.x), float(v.y), float(v.z), 1.0f};
                float w[3];
                for (int j = 0; j < 3; ++j) {
                    float s = 0.0f;
                    for (int k = 0; k < 4; ++k) s += p[k] * m.m[k][j];
                    w[j] = s;
                }
                lb.add(w[0], w[1], w[2]);
            }
        }
        sum_level_extent += lb.maxExtent();
        // A GoldenEye level is a few thousand units across. Tens of thousands
        // means the rooms did not assemble.
        if (lb.maxExtent() > 60000.0f) ++implausible;
    }
    std::printf("  mean level extent: %.0f units\n",
                sum_level_extent / double(levels.size()));
    check(implausible == 0, "every level assembles into a plausible bounding box");

    // Control: transpose the translate and confirm the check FAILS. A test that
    // passes either way is not a test.
    {
        int scattered = 0;
        for (const auto& l : levels) {
            Bounds lb;
            for (const auto& r : l.rooms) {
                Mat4 m = identity();
                m.m[0][3] = r.origin[0];   // column instead of row: wrong
                m.m[1][3] = r.origin[1];
                m.m[2][3] = r.origin[2];
                for (const auto& v : r.verts) {
                    const float p[4] = {float(v.x), float(v.y), float(v.z), 1.0f};
                    float w[3];
                    for (int j = 0; j < 3; ++j) {
                        float s = 0.0f;
                        for (int k = 0; k < 4; ++k) s += p[k] * m.m[k][j];
                        w[j] = s;
                    }
                    lb.add(w[0], w[1], w[2]);
                }
            }
            // With the translate in the wrong place every room collapses onto
            // the same origin, so the level shrinks to one room's size.
            if (lb.maxExtent() < 4000.0f) ++scattered;
        }
        check(scattered > int(levels.size()) / 2,
              "control: a transposed translate visibly breaks assembly "
              "(so check 2 has discriminating power)");
    }

    // --- 3. pipeline produces finite screen coordinates ---------------------
    std::printf("\n[3] vertex pipeline\n");
    const Mat4 proj = perspective(60.0f, 1.4005603f, 10.0f, 300.0f);
    Viewport vp;
    size_t nonfinite = 0, behind = 0, transformed = 0;
    for (const auto& l : levels)
        for (const auto& r : l.rooms) {
            // Camera INSIDE the room, 50 units back from its centre.
            //
            // Not 500: the game's projection is znear=10 / zfar=300
            // (src/game/bondview2.c:8423) while the mean room is ~576 units
            // across, so a camera outside a room has the whole room beyond the
            // far plane. That short draw distance is why GoldenEye is so foggy,
            // and it is why the camera has to be placed in the room to test
            // anything meaningful.
            //
            // Room-local vertices are already centred on zero, so the room's
            // own origin cancels out of the modelview entirely — applying it
            // here would move the room away from the camera, not toward it.
            const Mat4 mv = translate(0.0f, 0.0f, -50.0f);
            const Mat4 mvp = multiply(mv, proj);
            for (const auto& v : r.verts) {
                const TransformedVtx t = transformVertex(v, mvp, vp, 0xFFFF);
                ++transformed;
                if (t.behind_near) { ++behind; continue; }
                for (int i = 0; i < 3; ++i)
                    if (!std::isfinite(t.screen[i])) ++nonfinite;
            }
        }
    std::printf("  transformed %zu vertices (%zu behind the near plane)\n",
                transformed, behind);
    check(nonfinite == 0, "no non-finite screen coordinates");

    // --- 4. culling discriminates ------------------------------------------
    std::printf("\n[4] cull discrimination (unblocks G_CULLDL)\n");
    int in_front_kept = 0, behind_culled = 0, rooms_tested = 0;
    for (const auto& l : levels)
        for (const auto& r : l.rooms) {
            if (r.verts.size() < 3) continue;
            ++rooms_tested;

            std::vector<TransformedVtx> tv(r.verts.size());

            // Camera standing in the room: must NOT be culled.
            {
                const Mat4 mv = translate(0.0f, 0.0f, -50.0f);
                const Mat4 mvp = multiply(mv, proj);
                for (size_t i = 0; i < r.verts.size(); ++i)
                    tv[i] = transformVertex(r.verts[i], mvp, vp, 0xFFFF);
                if (!shouldCull(tv.data(), int(tv.size()))) ++in_front_kept;
            }

            // Room displaced far to one side: must cull. Every vertex then
            // shares the same off-screen outcode bit, which is exactly the
            // condition G_CULLDL tests.
            {
                const Mat4 mv = translate(100000.0f, 0.0f, -50.0f);
                const Mat4 mvp = multiply(mv, proj);
                for (size_t i = 0; i < r.verts.size(); ++i)
                    tv[i] = transformVertex(r.verts[i], mvp, vp, 0xFFFF);
                if (shouldCull(tv.data(), int(tv.size()))) ++behind_culled;
            }
        }
    std::printf("  rooms tested: %d\n", rooms_tested);
    std::printf("  visible rooms kept:   %d / %d\n", in_front_kept, rooms_tested);
    std::printf("  off-screen rooms cut: %d / %d\n", behind_culled, rooms_tested);
    check(in_front_kept > rooms_tested * 9 / 10,
          "rooms the camera is looking at survive culling");
    check(behind_culled > rooms_tested * 9 / 10,
          "rooms far off-screen are culled");

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nGEOMETRY VALIDATION PASSED\n");
    return 0;
}
