// rt64_probe.cpp — bring RT64 up against our RDRAM and report how far it gets.
//
// This is the wiring test. It exercises the real integration path:
//
//   ge_ultra::rdramInit()                  our flat 8 MB buffer
//     -> Rt64Backend::loadMicrocode()      blobs placed for GBI hash detection
//       -> Rt64Backend::init()             Application::setup()
//         -> setFramebuffer()              VI_ORIGIN, from our VI shim
//           -> processDisplayList()        a real room's list
//             -> present()
//
// On a machine with a display it should render. Headless it will stop at
// GraphicsDeviceNotFound or window creation — which is an environment limit, not
// a code one, and the probe says so rather than pretending otherwise.
//
// Usage: rt64_probe [room_pair.bin] [ucode-dir]

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "rhi/rt64_backend.h"
#include "ultra/rdram.h"

#include <SDL.h>

namespace {

std::vector<uint8_t> readFile(const std::string& path) {
    std::vector<uint8_t> out;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        out.resize(size_t(n));
        if (std::fread(out.data(), 1, out.size(), f) != out.size()) out.clear();
    }
    std::fclose(f);
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string pair_path = (argc > 1) ? argv[1] : "";
    const std::string ucode_dir = (argc > 2) ? argv[2] : "";

    std::printf("=== RT64 wiring probe ===\n");

    if (!ge_ultra::rdramInit()) {
        std::fprintf(stderr, "RDRAM allocation failed\n");
        return 2;
    }
    std::printf("[1] RDRAM: %u bytes at %p\n", ge_ultra::rdramSize(),
                static_cast<void*>(ge_ultra::rdramBase()));

    ge_rhi::Rt64Backend backend;

    // --- microcode, for GBI dialect detection -------------------------------
    if (!ucode_dir.empty()) {
        const auto text = readFile(ucode_dir + "/gspboot.text.bin");
        const auto data = readFile(ucode_dir + "/gspboot.data.bin");
        if (text.empty() || data.empty()) {
            std::printf("[2] microcode: NOT FOUND in %s\n", ucode_dir.c_str());
            std::printf("    (run the decomp's asset extraction; RT64 hashes\n"
                        "     these to pick the GBI dialect)\n");
        } else {
            const bool ok = backend.loadMicrocode(text.data(), uint32_t(text.size()),
                                                  data.data(), uint32_t(data.size()));
            std::printf("[2] microcode: %s (text %zu, data %zu bytes)\n",
                        ok ? "placed in RDRAM" : "FAILED to place",
                        text.size(), data.size());
            std::printf("    RT64's table has no entry for the retail NTSC-U\n"
                        "    hashes; see patches/rt64-goldeneye-hashes.patch\n");
        }
    } else {
        std::printf("[2] microcode: skipped (no ucode dir given)\n");
    }

    // --- window -------------------------------------------------------------
    SDL_Window* window = nullptr;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("[3] SDL video init failed: %s\n", SDL_GetError());
        std::printf("    Headless environment. RT64 needs a window and a Vulkan\n"
                    "    surface; there is no offscreen path. This is an\n"
                    "    environment limit, not a wiring problem.\n");
    } else {
        window = SDL_CreateWindow("GoldenEye (RT64)", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, 640, 480,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        std::printf("[3] SDL window: %s\n",
                    window ? "created" : SDL_GetError());
    }

    // --- RT64 setup ---------------------------------------------------------
    const ge_rhi::Rt64InitResult init = backend.init(window, "ge007-xr");
    std::printf("[4] RT64 setup: %s\n", init.message.c_str());

    if (!init.ok) {
        std::printf("\nProbe stopped before rendering. Everything above this\n"
                    "point is the integration path and it is exercised.\n");
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        ge_ultra::rdramShutdown();
        return 1;
    }

    // --- feed a real display list -------------------------------------------
    if (!pair_path.empty()) {
        const auto pairs = readFile(pair_path);
        if (pairs.size() > 8 && std::memcmp(pairs.data(), "GERP", 4) == 0) {
            // Layout per tools/extract_room_pair.py: origin[3], vtx_count,
            // verts, dl_bytes, dl. Take the first room.
            size_t o = 8;
            o += 12;
            uint32_t nv = 0;
            std::memcpy(&nv, pairs.data() + o, 4);
            o += 4;
            const uint8_t* verts = pairs.data() + o;
            o += size_t(nv) * 16;
            uint32_t dlb = 0;
            std::memcpy(&dlb, pairs.data() + o, 4);
            o += 4;
            const uint8_t* dl = pairs.data() + o;

            constexpr uint32_t kVtxBase = 0x200000;
            constexpr uint32_t kDlBase = 0x210000;
            std::memcpy(ge_ultra::rdramBase() + kVtxBase, verts, size_t(nv) * 16);
            std::memcpy(ge_ultra::rdramBase() + kDlBase, dl, dlb);

            constexpr uint32_t kFramebuffer = 0x300000;
            backend.setFramebuffer(kFramebuffer, 320, 240);
            std::printf("[5] feeding room 0: %u verts, %u bytes of display list\n",
                        nv, dlb);
            backend.processDisplayList(kDlBase, kDlBase + dlb);
            backend.present();
            std::printf("[6] frame submitted\n");
        } else {
            std::printf("[5] no usable room pair file\n");
        }
    }

    backend.shutdown();
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    ge_ultra::rdramShutdown();
    std::printf("\nprobe complete\n");
    return 0;
}
