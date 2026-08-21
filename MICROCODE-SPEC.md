# The GoldenEye `gsp3D` Display-List Dialect

Reference for writing an interpreter. Derived from `rsp/graphics/gmain.s`,
`include/gbi_extension.h`, `include/PR/gbi.h`, and the game's own CPU-side
decoders in `src/game/tex.c` and `src/game/lightfixture.c`.

Confidence is marked per claim. **[CONFIRMED]** = corroborated by two or more
independent sources. **[HIGH]** = one clear assembly path plus arithmetic
self-consistency. **[MEDIUM]** = single reading of corrupted assembly.
**[UNRESOLVED]** = could not be determined; listed explicitly rather than
guessed.

---

## VALIDATED AGAINST A REAL ROM

Everything below was originally derived from a corrupted disassembly plus the
game's C source. It has since been checked against a verified NTSC-U ROM
(SHA1 `abe01e4aeb033b6c0836819f549c791b26cfde83`).

**The DMEM data blob resolved the inferences.** `bin/gspboot.data.bin` — absent
from the checkout, extracted from the ROM — is the source of most of what was
UNRESOLVED. The headline: the table at DMEM `0x2D0` reads

```
0002d0   0  10  20  30  40  50  60  70  80  90 100 110 120 130 140 150
```

exactly the `{i * 10}` values inferred arithmetically from the `<<2` / `+0x420`
code in §3.2. That was the load-bearing guess in the whole document and it is
now primary evidence.

**The interpreter was run over every room display list in the game.**
`tools/extract_display_lists.py` + `tools/dl_validate.cpp`:

| | |
|---|---|
| levels | 23 |
| display lists | 1,937 |
| commands | 120,115 |
| triangles | 192,714 |
| vertices | 342,551 |
| distinct opcodes | **13, all known** |
| unknown opcodes | **0** |
| max vertex index | **15** (cache is 16) |
| lists terminating at `G_ENDDL` | **1,937 / 1,937** |
| refs to never-loaded cache slots | 0 |
| failed address resolutions | 0 |

Three claims in this document are now empirical rather than inferred:

- **The F3D determination (§1).** The highest vertex index across 192,714 real
  triangles is 15. A 32-entry F3DEX cache would show indices above 15 somewhere
  in 23 levels. It does not, anywhere.
- **`G_TRI4` is the only GoldenEye-authored extension (§2)** — but it is not the
  only opcode an F3DEX2-based renderer will get wrong; see §2.1 on
  `G_RDPHALF_CONT`. Thirteen opcodes appear in the entire
  corpus. Every one is in the map. No F3DEX-only opcode appears anywhere.
- **`G_SETTEX` is real and must be CPU-expanded (§4).** 9,675 `0xC0` commands
  are present in raw room lists. A port that skips `texLoadFromGdl()` renders
  every surface untextured.

**Asset pipeline verified.** `make extractassets` produced 2,698 textures;
`combined.bin` hashes to `044fca472bf6ef6691fa02ff1b65ff474d86a9fa`, matching
the value documented in `assets/images/readme.md` byte for byte.

---

## 0b. The 1172 container omits trailing zeros

Not microcode, but it will bite anyone writing an asset loader, and it cost a
debugging session here.

Room data is stored as a 2-byte magic (`0x1172`) followed by a **raw deflate**
stream — no zlib or gzip header, so `wbits=-15`.

**The compressor drops trailing zero bytes.** The game inflates into an
already-zeroed buffer, so a final `G_ENDDL` — `B8 00 00 00 00 00 00 00` — is
stored as the single byte `0xB8`. About 1% of room lists therefore inflate to a
length that is not a multiple of 8 and must be zero-padded back out to whole
commands.

Skip the padding and those lists have no terminator: the walker runs off the end
into whatever follows in RDRAM and reports plausible-looking garbage. The
failure presents as an interpreter bug. It is not one.

The decomp's own `tools/1172inflate.sh` hints at this — it *filters out* gzip's
`unexpected end of file` warning rather than addressing it.

---

## 0. Caveat about the source material

**`rsp/graphics/gmain.s` is not the shipped microcode and does not build.**

- Line 10 is `#include "ginit.s"`; that file does not exist in the repo.
- It is a lossy disassembly artifact. A label injector split roughly twenty
  instructions in half, destroying the mnemonic and first operand — e.g. lines
  351–352 are `lbl_13a4:` followed by bare `, 0x10($22)`.
- Branch targets are corrupt: `j 0xlbl1264` (line 217),
  `beq $0, $0, 0xlbl107c` (line 627). Symbolic label names do not correspond to
  the numeric targets.
- Line 1544 is `addiu $0, $0, 0xbeef`, a placeholder.

The real microcode is a binary blob: `src/gspboot.s:12-19` does
`.incbin "bin/gspboot.text.bin"` / `"bin/gspboot.data.bin"`, and `bin/` is not
in the checkout. **`gspboot.data.bin` is the DMEM image holding every dispatch
table and lookup table**, so table *contents* are inferable from the arithmetic
that consumes them but are not directly readable.

Where the assembly is ambiguous, the game's own C code is used as the authority.
That turns out to cover everything that matters, because the decomp contains
independent CPU-side reimplementations of both nonstandard behaviours.

---

## 1. The baseline is F3D, not F3DEX  **[CONFIRMED]**

The single most consequential structural fact.

`F3DEX_GBI` and `F3DEX_GBI_2` are never defined — they appear only inside
`#ifdef`s in `PR/gs2dex.h` and `PR/sptask.h`, and no `-D` flag sets them in
`Makefile` or `include/Makefile.targets`. So `PR/gbi.h` resolves to the plain-F3D
branch.

Decisively, the microcode's own decode matches F3D packing. `gbi.h:1785`
(`gDma1p`) puts the parameter byte in **byte 1** of `w0`, and `gbi.h:1865`
(non-EX `gSPVertex`) encodes `((n)-1)<<4 | (v0)` there. `gmain.s:329-331,464-465`
reads exactly that:

```
329:  lbu $1, 0xfff9($27)     # w0 byte 1
331:  andi $6, $1, 0xf        # v0  = low nibble
464:  srl $1, $1, 4
465:  addi $5, $1, 0x1        # n   = high nibble + 1
```

The F3DEX encoding (`gbi.h:1861`, `((n)<<10) | (sizeof(Vtx)*(n)-1)` with
`(v0)*2` in byte 1) is incompatible with this decode.

Corroborated twice more: `G_MOVEWORD` is read as `gImmp21` layout
(`gmain.s:236-237`), and `NUML(n)` is masked as the F3D form
(`gmain.s:1442-1443`).

**Implications:** 16-entry vertex cache. F3D `G_MTX` flag encoding
(`G_MTX_PROJECTION=0x01`, `G_MTX_LOAD=0x02`, `G_MTX_PUSH=0x04` — *not* F3DEX's
inverted form). `G_TRI4` occupies the opcode slot F3DEX would use for `G_TRI2`.
**`G_MODIFYVTX`, `G_BRANCH_Z`, `G_LOAD_UCODE`, and `G_QUAD` do not exist in this
GBI.**

---

## 2. Opcode map

`G_IMMFIRST = -65` (`gbi.h:144`); byte value is `(G_IMMFIRST - k) & 0xFF`.

| Byte | Name | Note |
|---|---|---|
| `0x00` | `G_SPNOOP` | |
| `0x01` | `G_MTX` | |
| `0x03` | `G_MOVEMEM` | viewport, lights, lookat |
| `0x04` | `G_VTX` | |
| `0x06` | `G_DL` | push (byte 1 == 0) / branch |
| **`0xB1`** | **`G_TRI4`** | **the one extension** |
| `0xB3` | `G_RDPHALF_2` | |
| `0xB4` | `G_RDPHALF_1` | |
| `0xB5` | `G_LINE3D` | debug/AI-path overlays only |
| `0xB6` | `G_CLEARGEOMETRYMODE` | |
| `0xB7` | `G_SETGEOMETRYMODE` | |
| `0xB8` | `G_ENDDL` | |
| `0xB9` | `G_SETOTHERMODE_L` | |
| `0xBA` | `G_SETOTHERMODE_H` | |
| `0xBB` | `G_TEXTURE` | |
| `0xBC` | `G_MOVEWORD` | |
| `0xBD` | `G_POPMTX` | |
| `0xBE` | `G_CULLDL` | |
| `0xBF` | `G_TRI1` | |
| `0xC0` | `G_NOOP` **= `G_SETTEX`** | **CPU-side; see §4** |
| `0xC1`–`0xFF` | RDP | passthrough; segment fixup on `0xFD`/`0xFE`/`0xFF` |
| **`0xB2`** | **`G_RDPHALF_CONT`** | **stock F3D; the sky and water path — see §2.1** |
| `0xAF`, `0xB0` | — | F3DEX-only opcodes; **not present** |

### 2.1 `G_RDPHALF_CONT` (0xB2) — corrected 2026-08-21  **[CONFIRMED]**

This table previously listed `0xB2` among the "F3DEX-only opcodes, not present".
That was **wrong**, and the way it was wrong is worth recording because it is a
methodology failure rather than a transcription slip.

`G_RDPHALF_CONT` is `(G_IMMFIRST - 13)` = `0xB2`, defined in stock
`include/PR/gbi.h:164` — not in `gbi_extension.h`. It is not a GoldenEye
extension at all; it is a **stock F3D** command that F3DEX2 later reassigned.
`src/game/sky.c` emits it constantly, paired with `G_RDPHALF_1`, to stream the
extra words of long RDP commands (`sky.c:1821-1933`, `:2344+`).

**Why this survey missed it:** every validation run behind this document walked
ROOM display lists — 1,937 of them. The sky and the water are not stored per
room; `sky.c` builds their lists at runtime, every frame. A corpus that covers
100% of one path says nothing about a path it never touched, and the confident
"not present" was an inference from absence in that corpus, stated as fact.

**Why it matters beyond this document:** a renderer written against F3DEX2 —
which is what `fast3d` and RT64 target — has a different meaning for `0xB2`
entirely, so it either drops the command or decodes it as something else. That
is a candidate explanation for the black skyboxes and flat water reported by
`cblock85/GoldenEye64Recomp` (see `PRIOR-ART.md` §3.1), whose README attributes
them to "custom microcode commands RT64 doesn't implement". **Candidate, not
conclusion** — nobody has yet traced a black sky back to a dropped `0xB2`.

`src/gbi/gbi.h` and `src/gbi/gbi_interp.cpp` accept it as of this correction.

---

`G_TRI4` value: `-65 - 14 = -79`; `-79 & 0xFF = 0xB1`. **[CONFIRMED]** —
independently corroborated by `src/game/tex.c:975` (`case 0xb1:` in the
display-list scanner) and by the hand-encoded command at
`src/game/glass2.c:451` (`_g->words.w0 = 0xB1000032;`).

> **[UNRESOLVED]** The three dispatch-table base addresses read out of the
> disassembly (`0xBC` first-level, `0x76` immediate, `0xC4` DMA) mutually
> overlap, so at least one is a mis-disassembly, and their contents live in the
> absent data blob. **This does not matter for an interpreter** — dispatch on
> the opcode byte using the table above. Settled by: `gspboot.data.bin`, or a
> ROM dump of the ucode data segment.

---

## 3. `G_TRI4` (0xB1)

### 3.1 Bit layout  **[CONFIRMED]**

Per `gbi_extension.h:103-120`:

```
w0 = (0xB1 << 24) | (z4 << 12) | (z3 << 8) | (z2 << 4) | (z1 << 0)
w1 = (y4 << 28) | (x4 << 24) | (y3 << 20) | (x3 << 16)
   | (y2 << 12) | (x2 <<  8) | (y1 <<  4) | (x1 <<  0)
```

Big-endian byte view:

| byte | contents |
|---|---|
| b0 | `0xB1` |
| b1 | `0x00` |
| b2 | `z4<<4 \| z3` |
| b3 | `z2<<4 \| z1` |
| b4 | `y4<<4 \| x4` |
| b5 | `y3<<4 \| x3` |
| b6 | `y2<<4 \| x2` |
| b7 | `y1<<4 \| x1` |

Independently verified against `extract_vertex_indices_from_triangle`
(`src/game/lightfixture.c:195-227`), the decomp authors' own CPU-side decoder,
which reads the same bytes. All four sub-triangle cases cross-check.

`test_gbi.cpp` runs this comparison exhaustively — 16,368 sub-triangle decodes
across every nibble value in every position — and they agree everywhere.

Note `case 0` of that function is `G_TRI1` and divides by 10, proving `G_TRI4`
indices are **not** pre-multiplied. **[CONFIRMED]**

### 3.2 Index → DMEM address  **[HIGH]**

`gmain.s:215-218` looks each 4-bit index up in a 16-byte table at DMEM `0x2D0`,
then `gmain.s:223-229` computes `0x420 + (LUT[i] << 2)`. For that to equal
`0x420 + i*40`, the table must hold `{0, 10, 20, …, 150}` — the same `×10` form
stock `G_TRI1` carries in its command word.

So `gbi_extension.h`'s comment is right about the *display list*, but the RSP
restores the stock representation via lookup and then reuses the unmodified
`G_TRI1` triangle-setup core. **Downstream of decode, `G_TRI4` and `G_TRI1` are
the same path.**

- Per-vertex DMEM stride: **40 bytes** **[CONFIRMED]** (`gmain.s:278`, `:598`,
  and second-vertex field offsets = first + 0x28 throughout).
- Vertex cache base: DMEM **`0x420`** **[CONFIRMED]** (`gmain.s:227-229,469,277`).
- Flat-shade flag forced to 0 by `add $5, $0, $0` (`gmain.s:214`), so `G_TRI4`
  triangles always take the first vertex as the flat-shade source. **[HIGH]**

> **[UNRESOLVED, low impact]** The `0x2D0` table's contents are inferred from the
> `<<2` / `+0x420` arithmetic, not read. Settled by: the data blob.

### 3.3 The four-pass mechanism  **[HIGH]**

`G_TRI4` does not loop internally. Per `gmain.s:202-213` it consumes one
triangle, shifts `w0 >>= 4` and `w1 >>= 8`, writes the shifted words back into
the DMEM command buffer, rewinds both DL pointers by 8 and restores the byte
counter — then the command is re-fetched and re-dispatched.

Unrolled:

```c
for (int i = 0; i < 4; i++) {
    if (w1 == 0) break;                  // whole remaining w1
    emit(w1 & 0xF, (w1 >> 4) & 0xF, w0 & 0xF);
    w1 >>= 8;
    w0 >>= 4;
}
```

### 3.4 Termination is a stop, not a skip  **[HIGH]**

`gmain.s:202`: `beq $24, $0, ...` where `$24` is the **entire remaining `w1`**,
tested at the top of each pass, *before* extracting the current triangle's
nibbles.

Two consequences an interpreter must reproduce:

1. **Only `x` and `y` are tested.** A `z` nibble is never examined. A triangle
   with `x=0, y=0, z=5` still terminates the command.
2. **It is not per-triangle.** An interior `(x=0, y=0)` slot followed by a
   nonzero slot does *not* terminate — `w1` is still nonzero — so the zeroed slot
   is emitted as a degenerate triangle.

`gbi_extension.h:101`'s "triangles with all points set to 0 are not drawn" holds
only for trailing slots. Any interpreter that helpfully skips interior zero
slots silently disagrees with hardware.

**Ground truth** — `src/game/glass2.c:444-452`, a hand-encoded command from the
shipping game:

```c
gSPVertex(gdl++, arg1, 4, 0);
_g->words.w0 = 0xB1000032;
_g->words.w1 = 0x2110;
```

Decodes to triangles `(0,1,2)` and `(1,2,3)` — a quad from four vertices — then
`w1` becomes 0 and it stops. (The decomp comment on line 446 guessing
`gSPModifyVertex` is wrong: `0xB1` is `G_TRI4`, and `G_MODIFYVTX` does not exist
in a non-F3DEX GBI.) **[CONFIRMED]**

### 3.5 Vertex cache is 16 entries  **[HIGH]**

Four independent checks:

1. `G_TRI4` indices are 4 bits → 0–15.
2. `G_VTX` destination index masked to 4 bits (`gmain.s:331`).
3. `G_VTX` count is `nibble + 1` → max 16 (`gmain.s:464-465`).
4. **Arithmetic closure:** `0x420 + 16 × 40 = 0x6A0`, exactly the DL command
   buffer base (`gmain.s:48,54,1254`). No gap, no overlap.

`gbi_extension.h:100`'s "to use a higher index use gSP1Triangle" is misleading —
with a 16-entry cache there is no higher index. That comment appears to be
carried over from Perfect Dark, whose ucode has a 32-entry cache.

---

## 4. `G_SETTEX` (0xC0) — not an RSP command

**`G_SETTEX == G_NOOP == 0xC0`** (`gbi_extension.h:49` vs `gbi.h:174`).
**[CONFIRMED]**

In the F3D command space `0xC0–0xFF` is the RDP passthrough range and `0xC0` is
the RDP's no-op. The microcode forwards it; the RDP discards it. **The RSP never
interprets `texture_id`, never touches a texture table, and never emits an RDP
command for it.**

Expansion happens on the CPU in `texLoadFromGdl()`, `src/game/tex.c:779–1040`,
`case G_NOOP:` at line 817. It reads a display list and writes an expanded one.

### 4.1 Command decode (`tex.c:854-963`, matching `gbi_extension.h:193-207`)

```
type     =  w0        & 7        // TextureTypes
shiftt   = (w0 >> 10) & 0xF
shifts   = (w0 >> 14) & 0xF
offset   = (w0 >> 18) & 3        // "tile" in the macro — actually a half-texel bias
tmode    = (w0 >> 20) & 3        // "cmt"
smode    = (w0 >> 22) & 3        // "cms"
texnum   =  w1        & 0xFFF
detailid = (w1 >> 12) & 0xFFF
minlevel = (w1 >> 24) & 0xFF
```

`texnum` indexes `g_Textures[]` (a static array in the game's `.data`) and a
runtime texture pool in main RAM, via `texLoadFromTextureNum()` +
`texFindInPool()` (`tex.c:844-845`).

`smode`/`tmode` go through `texModeToGbiMode` (`tex.c:378-386`): `1 → G_TX_CLAMP`,
`2 → G_TX_MIRROR`, else `G_TX_WRAP` — a 3-value enum, **not** the raw GBI
encoding. `offset` is not a tile number; `offset == 2` adds a `+2` bias to
`uls/ult/lrs/lrt`. `minlevel` becomes the PrimColor min-LOD byte (`tex.c:429`).

### 4.2 Emitted sequences per `TextureTypes`

| type | handler | sequence |
|---|---|---|
| 0 `LOD` | `tex.c:745-753` | `LoadToTmemAddr(tex,0)` → `TileFromDefinition` → `TileLods(basetile=1)` → if `maxlod==1`, `TileLods(basetile=2)` |
| 1 `DETAIL` | `tex.c:728-742` | `LoadToTmemZero(detail)` → `TileSync` → `LoadToTmemAddr(base, sizeof detail)` → `TileFromDefinition(detail)` → `TileLods(base, basetile=1)` |
| 2 `MIPMAP` | `tex.c:717-725` | `LoadToTmemAddr(tex,0)` → `TileLods(basetile=0)` → if `maxlod==1`, `basetile=1` |
| 3 `TILE` | `tex.c:765-771` | `LoadToTmemZero` → `WriteTile(tile 0)` → `WriteTile(tile 1)` |
| 4 `TILE_PRESWAPPED` | `tex.c:756-762` | `LoadToTmemZero` → `WriteTile(tile 0)` |

Types 0 and 2 additionally emit `SetCycleType(2CYCLE)`, `SetTextureLOD(G_TL_LOD)`,
`SetTextureDetail(G_TD_DETAIL)` when a runtime override entry exists for the
texnum (`tex.c:884,940`).

> **[UNRESOLVED]** What "preswapped" actually means. The hypothesis "data is
> pre-word-swapped so LoadBlock differs" is **not supported** — the `LoadBlock`
> is byte-identical between types 3 and 4; the only difference is one tile
> descriptor versus two. The name presumably refers to build-time asset
> processing (`texSwapAltRowBytes`, `image.c:2165`), but nothing in the expansion
> branches on it. Settled by: the asset-build script.
>
> **This is inert.** We compile and run `tex.c` unmodified, so we inherit
> whatever it does without needing to understand the naming.

### 4.3 What this means for the port

`texLoadFromGdl()` is already decompiled. The port compiles and runs it as-is.
A list that has been through the pre-pass contains **no `0xC0` commands**, so the
interpreter never sees one. If one arrives, the port skipped the pre-pass — a
bug to fix, not a behaviour to emulate.

We also inherit for free the redundancy cache in
`texTrySetTileState`/`texTrySetTileSize` (`tex.c:180-232`), which omits tile
commands already in effect.

Other opcodes `texLoadFromGdl` reacts to, relevant only if you reimplement the
pre-pass: `0xE7` sets `syncEmitted`; `0xB1`/`0xBF` set `writeTexFlag`; `0xBB` is
saved for later LOD patching; `0xBA` is *dropped* when valid and its byte 2 is
16, 17, or 20 (`tex.c:1008-1024`).

---

## 5. `G_VTX` (0x04)

RDRAM struct is the standard 16-byte `Vtx_t`. **[CONFIRMED]** —
`gmain.s:467-468` loads two vertices 16 bytes apart, `:488` reads S,T at +8,
`:490` reads RGBA/normal at +12, `:501` advances by 0x20.

```
w0 = 0x04<<24 | (((n)-1)<<4 | v0)<<16 | (16*n)
w1 = segmented address
```

- `n`  = `((w0 >> 20) & 0xF) + 1`, range 1–16
- `v0` = `(w0 >> 16) & 0xF`, range 0–15
- DMEM destination = `0x420 + v0*40` (`gmain.s:469-471`)

Lighting is gated on `G_LIGHTING` (`gmain.s:494,500`). The normalization block
(`:1461-1497`) is textbook F3D `vrsqh`/`vrsql`. **No nonstandard lighting or
normal handling found.**

> **[UNRESOLVED]** Per-light DMEM field layout. Stride is clearly `0x20`, but the
> four accessed offsets (`0x1B0/0x1B8/0x1C0/0x1D0`) do not tile cleanly from the
> corrupted listing, and the ambient/directional split is uncertain. Settled by:
> the data blob, or the `G_MOVEMEM` DMEM-address table.

---

## 6. Matrices, segments, and the rest

### 6.1 `G_MTX` (0x01)  **[HIGH]**

Parameter byte in `w0` byte 1. **F3D flag encoding**:
`G_MTX_PROJECTION=0x01`, `G_MTX_LOAD=0x02`, `G_MTX_PUSH=0x04`. Mixing this up
with F3DEX's inverted form loads the projection into the modelview slot, which
presents as "the camera is stuck at the origin".

Stack base and limit are set at load time (`gmain.s:696-703`): limit = base +
`0x280`, so **10 levels** — even though the OS allocates 16
(`SP_DRAM_STACK_SIZE8`). Matches stock F3D.

**Overflow is silent** (`gmain.s:344`): the push is skipped and the load/multiply
still happens. No error, no imbalance detection. Reproduce this.

### 6.2 Segments  **[CONFIRMED]**

Table of 16 entries at DMEM `0x160`. Resolution (`gmain.s:71-86`):

```
physical = segment_table[(addr >> 24) & 0xF] + (addr & 0x00FFFFFF)
```

The ucode's `srl 22` + `andi 0x3c` means **only four bits of the segment nibble
are used** — bits 28 and above are discarded, so `0x1E000000` resolves through
segment 14, not out of range.

`SPSEGMENT_BG_VTX = 14` (`bondconstants.h:2897`), set by `bg.c:2688,2723`. The
CPU-side mirror of this exact computation is `lightFindVertexBaseForTri`
(`lightfixture.c:180-192`) — an independent confirmation of the rule.

### 6.3 Everything else is stock F3D

No GoldenEye-specific deviation found in any of:

- `G_SETGEOMETRYMODE`/`G_CLEARGEOMETRYMODE` (`gmain.s:290-302`); all observed
  geometry-mode bits are stock values.
- Fog — writes the fog factor into the **alpha byte of the vertex colour**
  (`gmain.s:575,577`), params from DMEM `0x330`. **[HIGH]**
- Clipping — trivial-reject on outcode mask `0x7030`, needs-clip on `0x4343`
  (`gmain.s:790,793`). Stock F3D masks.
- `G_CULLDL` (`gmain.s:272-281`) — walks the cache at stride 0x28 ANDing clip
  flags with `0x7030`. The `& 0x0f` and `*40` in `gbi.h:1810-1817` reconfirm the
  16-entry / 40-byte cache.
- `G_SETOTHERMODE_H`/`_L` (`gmain.s:251-271`) — length in byte 3, shift in byte
  2, standard mask-and-merge.
- RDP passthrough (`gmain.s:305-326`) — `w1` segment-fixed for `0xFD`, `0xFE`,
  `0xFF` only; everything else verbatim.
- `gSPPerspNormalize`, `gSPLookAtX`/`Y`, `gSPClipRatio`, yield/task handling —
  all stock.

**Absent:** `gSPModifyVertex`, `gSPBranchLessZ`, `G_LOAD_UCODE`, `G_QUAD`
(F3DEX-only). `gSPTextureL`'s macro exists but has zero uses in `src/`.

---

## 7. Interpreter shape

**Layer A — CPU pre-pass.** The game's own `texLoadFromGdl()`, compiled and run
unmodified. Consumes `0xC0`. Off the render path.

**Layer B — display-list walker.** A stock F3D interpreter with exactly two
deltas: opcode `0xB1` is `G_TRI4`, and the vertex cache is 16 entries.

Implemented in `src/gbi/`, tested in `tests/test_gbi.cpp`.

---

## 8. Consolidated UNRESOLVED list

| # | Item | Impact | Settled by |
|---|---|---|---|
| 1 | ~~Dispatch-table base addresses/contents~~ | **RESOLVED** — see below | read from `gspboot.data.bin` |
| 2 | ~~Contents of the `0x2D0` LUT~~ | **RESOLVED** — `{0, 10, … 150}` exactly as inferred | read from `gspboot.data.bin` |
| 3 | Light record DMEM field layout | low — lighting is reproduced on the GPU | data blob, or the `G_MOVEMEM` table |
| 4 | `G_POPMTX` handler details | low — semantics clear from `gbi.h` | clean disassembly of `gspboot.text.bin` |
| 5 | DL return-stack depth (9 or 10) | low | clean disassembly |
| 6 | `G_TRI1` flag-byte semantics | low — every call site in `src/` passes 0, and `G_TRI4` forces 0 | clean disassembly |
| 7 | `sizeof(struct image_entry)` (4 vs 8) | low — header bitfields over-subscribe a 32-bit word | compiled build or linker map |
| 8 | Meaning of "preswapped" | **none** — we run `tex.c` unmodified | asset-build script |
| 9 | `gSPTextureL` support | none — zero uses | clean disassembly |
| 10 | ~~Whether the `0x40–0x7F` dispatch group does anything~~ | **RESOLVED** — it does not | read from `gspboot.data.bin` |

None of these blocks phase 2.

### Dispatch tables, read from the data blob

First-level table at DMEM `0xBC`, four halfwords, indexed by `((cmd>>5)&6)`:

```
0xBC:  13c8  10a8  11fc  1390
       0x00  0x40  0x80  0xC0   <- command range
```

`0x10a8` is the unimplemented/skip handler — it recurs throughout the tables.
The `0x40–0x7F` group dispatches straight to it, **settling UNRESOLVED #10: that
range does nothing.**

DMA-group table at `0xC4`, indexed by `cmd*2`:

| cmd | entry | meaning |
|---|---|---|
| 0x00 | `10a8` | skip — `G_SPNOOP` |
| 0x01 | `13e0` | **`G_MTX`** |
| 0x02 | `10a8` | skip |
| 0x03 | `155c` | **`G_MOVEMEM`** |
| 0x04 | `156c` | **`G_VTX`** |
| 0x05 | `10a8` | skip |
| 0x06 | `1758` | **`G_DL`** |
| 0x07–0x09 | `10a8` | skip |

This confirms the DMA-group half of the opcode map in §2 exactly, from primary
evidence rather than inference.

> The immediate-group table base is still ambiguous in the disassembly (the
> three candidate bases overlap). It remains irrelevant: an interpreter
> dispatches on the opcode byte, and the corpus run above proves the opcode map
> is complete across the whole game.

---

## Key files

| Path | Role |
|---|---|
| `rsp/graphics/gmain.s` | corrupted partial ucode reconstruction; **not buildable** |
| `src/gspboot.s` | the real ucode — `.incbin` of `bin/gspboot.{text,data}.bin` (**absent**) |
| `include/gbi_extension.h` | `G_TRI4`, `G_SETTEX`, `gSP4Triangles`, `gsSPUseTexture` |
| `include/PR/gbi.h` | baseline: `G_IMMFIRST`:144, `G_NOOP 0xc0`:174, `gDma1p`:1785, F3D `gSPVertex`:1865 |
| `src/game/tex.c` | **the `G_SETTEX` expander** — `texLoadFromGdl` :779–1040 |
| `src/game/lightfixture.c` | independent `G_TRI4` decoder :195–227; segment-14 resolver :180–192 |
| `src/game/glass2.c` | hand-encoded `G_TRI4` :451 — ground truth |
| `src/game/image.h` | `struct tex` :39, `struct image_entry` :55 |
| `src/game/bg.c` | `gSPSegment(SPSEGMENT_BG_VTX, …)` :2688, :2723 |
| `src/game/rsp.c` | task setup, `gsp3DTextStart` / `gsp3DDataStart` |
