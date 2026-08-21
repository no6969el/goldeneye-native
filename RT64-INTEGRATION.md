# RT64 integration

Status: **RT64 builds clean and already speaks GoldenEye's dialect.** This is a
much better starting position than the plan assumed.

---

## Build

Verified building on Linux from a clean clone (`github.com/rt64/rt64`, submodules
recursive), Vulkan backend + SDL2 window:

```bash
sudo apt-get install -y libsdl2-dev libvulkan-dev libgtk-3-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DRT64_STATIC=ON -DRT64_SDL_WINDOW_VULKAN=ON
cmake --build build -j
# -> build/rt64.a  (~13 MB, 340 targets, 0 errors)
```

`libgtk-3-dev` is not obvious from the README — it comes in via
`nativefiledialog-extended`, and configure fails on it with a `gtk+-3.0` message
that doesn't name RT64 at all.

---

## The big finding: F3DGOLDEN already exists

`src/gbi/rt64_gbi_f3dgolden.cpp` implements GoldenEye's microcode dialect, and
`F3DGOLDEN_G_TRIX` is defined as **`0xB1`** — exactly the opcode value derived in
`MICROCODE-SPEC.md`.

Their `triX` and our `decodeTri4` agree in every detail: same nibble
extraction (`v0 = w1 & 0xF`, `v1` next nibble, `v2 = w0 & 0xF`), same
`w1 >>= 8` / `w0 >>= 4` per triangle, and the same termination — `while (w1 != 0)`
tested at the top of the loop, before extracting indices, never looking at the
`z` nibbles.

That makes **three independent implementations in agreement**: ours, the game's
own CPU-side decoder (`src/game/lightfixture.c:195-227`), and RT64's. The
`G_TRI4` semantics are settled.

RT64 has no 4-iteration cap; it doesn't need one, since `w1` is 32 bits and
shifts 8 per pass.

---

## Two things to fix before it will run

### 1. Ucode auto-detection will not fire on this ROM

RT64 identifies the microcode by XXH3-64 hashing the ucode text and data in
RDRAM (`src/gbi/rt64_gbi.cpp:403-430`) against a table of known segments. The
GoldenEye entries are:

| line | hash length | expected hash |
|---|---|---|
| `rt64_gbi.cpp:176` | `0x1420` (5152) | `0xEDA47A4C2B7E69F8` |
| `rt64_gbi.cpp:276` | `0x800` (2048) | `0xB2152361A81ED3B0` |

Both **lengths match our ROM exactly** — `bin/gspboot.text.bin` is 5152 bytes and
`bin/gspboot.data.bin` is 2048. But neither hash matches. Computed from a
verified NTSC-U ROM (SHA1 `abe01e4a…`):

| segment | length | actual XXH3-64 |
|---|---|---|
| `gspboot.text.bin` | `0x1420` | `0xC09B06D35BC306D9` |
| `gspboot.data.bin` | `0x800`  | `0xDEEF4CB184A0CAB8` |

Right shape, different build. RT64's own entries carry a `// Needs confirmation.`
comment and their instance string is `"SW Version: 2.0G, 09-30-96 (GE007)"`, so
they were probably taken from a different revision or a prototype.

Two ways forward, in order of preference:

- **Add a `GBISegment` entry** for these hashes pointing at `&F3D_GOLDEN`. One
  line each, and worth upstreaming — it fixes detection for the retail NTSC-U
  ROM and removes a "needs confirmation".
- **Bypass detection** and force `GBIUCode::F3DGOLDEN`. Fine for a native port,
  where the microcode is never in doubt, and avoids depending on the blobs being
  present at all.

If you keep detection, note the consequence: **the port must place the real
microcode blobs in RDRAM even though nothing ever executes them**, purely so the
hash can be computed. That is a non-obvious requirement for a native port, where
the RSP does not exist.

### 2. `G_MOVEWORD` at `0xBD`

`rt64_gbi_f3dgolden.h` defines `F3DGOLDEN_G_MOVEWORD 0xBD` and maps it to
`GBI_F3D::moveWord` *on top of* the F3D base setup, which has already mapped
`0xBC`. The game's own headers put `G_MOVEWORD` at `0xBC` and `G_POPMTX` at
`0xBD` (`G_IMMFIRST - 3` and `- 2`).

This is not a contradiction — it's defensive, and it's safe here because
**GoldenEye never pops matrices**: zero uses of `gSPPopMatrix`/`gsSPPopMatrix`
anywhere in the decomp. `0xBD` is dead opcode space in this game, so mapping it
costs nothing.

Our interpreter follows the game's headers (`0xBC` = MOVEWORD, `0xBD` = POPMTX)
and reaches the same behaviour, since POPMTX never occurs. No `0xBD` appears
anywhere in the 1,937-list room corpus. Left as-is; noted here so the difference
isn't mistaken for a bug later.

---

## Integration shape

RT64 is **not** fed a digested command stream. Per its README it does deferred
RSP *and* texture decode in compute shaders, so it wants raw memory and display
lists and runs its own interpreter:

```cpp
Application::setup(threadId);
Application::processDisplayLists(uint8_t *memory,      // our flat RDRAM
                                 uint32_t dlStart,
                                 uint32_t dlEnd,
                                 bool isHLE /* = true */);
Application::updateScreen();
```

`Application::Core` (`src/hle/rt64_application.h`) wants `RDRAM`, `DMEM`, `IMEM`
and the DPC/VI register set. The port synthesises the registers and hands over
its RDRAM pointer directly.

**This is why the flat 8 MB RDRAM decision matters.** RT64 takes a `uint8_t*` and
32-bit addresses into it — exactly the contract `src/ultra/rdram.h` was built
around. Had the game been given malloc'd blocks, this interface would be
unusable.

### What that means for `IDrawSink`

`ge_gbi::IDrawSink` is not the RT64 feed path, and should not be bent into one.
It stays useful for what it already does — corpus validation (`dl_validate`),
`G_CULLDL` outcodes, and the screen-space read-back that `bondview` and the HUD
need. RT64 gets RDRAM and addresses; the interpreter stays the reference model.

That is a real simplification: no adapter layer to write, and no risk of the two
disagreeing.

---

## Wiring: done, and what it took

`src/rhi/rt64_backend.{h,cpp}` constructs `Application::Core` from our RDRAM and
a synthesised register block, and wraps setup / processDisplayLists /
updateScreen. `tools/rt64_probe.cpp` drives the whole path.

**It compiles and links.** `ge_rt64` builds clean against RT64's headers — which
is the real check, because it verifies every `Core` field, method signature and
type in the handoff. `rt64_probe` links to an 8.3 MB executable against
`rt64.a` and its contrib archives.

Four things cost time and are worth knowing:

1. **`HLSL_CPU` is load-bearing.** RT64's `src/shared/*.h` are shared between
   HLSL shaders and C++. Without that define they compile as shader source, and
   you get a wall of `'float3' does not name a type` — plus a genuinely
   confusing collision where `rt64_hlsl.h` line 271 is parsed as a call to POSIX
   `select()`. Mirror RT64's own set: `HLSL_CPU`, `FFX_GCC`,
   `IMGUI_IMPL_VULKAN_NO_PROTOTYPES`, and on Linux
   `PLUME_SDL_VULKAN_ENABLED` + `RT64_SDL_WINDOW_VULKAN`.
2. **`file(GLOB_RECURSE)`, not `file(GLOB)`.** The contrib archives sit several
   levels down (`build/src/contrib/plume/libplume.a` and friends). `**` is not
   recursive in a plain `GLOB`, so the link fails on symbols that are in fact
   present.
3. **Repeat `rt64.a` after the contrib set.** rt64 and plume reference each
   other; a single pass leaves undefined symbols.
4. **System deps:** SDL2, vulkan, X11, zstd, and GTK3. zstd and GTK3 are not
   obvious — the first comes in via RT64's zip filesystem, the second via
   `nativefiledialog-extended`.

### Where it stops here

The probe runs and reaches Vulkan surface creation:

```
SDL_Vulkan_CreateSurface failed with error Invalid window.
```

This container is headless. Vulkan itself is present (llvmpipe software
rasteriser), but RT64 with `RT64_SDL_WINDOW_VULKAN` needs a real `SDL_Window`
and there is no offscreen path — `SDL_VIDEODRIVER=offscreen` and `=dummy` both
fail to produce a surface. **That is an environment limit, not a wiring
problem.** On a machine with a display, the next thing to run is exactly this
probe.

---

## Next steps

1. **Run `rt64_probe` on a machine with a display.** Everything up to surface
   creation is already exercised:

   ```bash
   cmake -S . -B build -DRT64_DIR=/path/to/rt64
   cmake --build build --target rt64_probe -j
   ./build/rt64_probe /tmp/room_pair.bin /path/to/decomp/bin
   ```

2. **Apply `patches/rt64-goldeneye-hashes.patch`** (three lines) or force
   `GBIUCode::F3DGOLDEN`. Without it, detection prints *"Unable to find a
   matching GBI in the current database. This game is not supported in HLE."*
   even though the handler for it is present and correct.

3. Decide whether the port ships the microcode blobs purely for hash detection,
   or forces the dialect. Forcing is cleaner for a native port — the RSP does
   not exist there, so carrying its microcode only to hash it is ceremony.

4. Keep `room_render` regardless. It needs no GPU, runs in CI, and is the
   fastest way to tell whether a change broke geometry rather than shading.
