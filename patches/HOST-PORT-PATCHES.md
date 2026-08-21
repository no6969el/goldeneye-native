# Host-build patches to `n64decomp/007`

Everything needed to compile the game's C on a host target. Measured, not
guessed: `tools/compile_census.sh` reports the number before and after.

```
baseline                      33 / 134 files   (~1,400 errors)
after the patches below      134 / 134 files   (0 errors)   GCC
                             134 / 134 files   (0 errors)   clang
```

**All of the game's C compiles on a host target.**

Apply with `git apply patches/decomp-host-port.patch` from the decomp root.

**Verified, not asserted.** The decomp is a *matching* decompilation, so `make`
must still produce a byte-identical ROM. With every patch below applied:

```
$ make && sha1sum build/u/ge007.u.z64
abe01e4aeb033b6c0836819f549c791b26cfde83   <- matches the retail US cartridge
```

Every `.o` is identical to a pristine build in `.text`, `.data` and `.rodata`;
seven differ only in `.mdebug` line tables, shifted by added comments.

That check earned its place. An earlier revision of this file claimed the
patches were "guarded or name-only, so the ROM is unaffected." **That was
wrong**, and building it proved so — see patch 5.

---

## 1. Compiler flags

```
-std=gnu99 -fms-extensions
-DVERSION_US -DLANG_US -DREFRESH_NTSC -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM
-DBUGFIX_R0 -DBYTEMATCH
```

Mirrors the decomp Makefile's US build (line 74) **minus `-DTARGET_N64`**, which
selects 32-bit N64 typedefs for `size_t` and friends and is wrong on a 64-bit
host.

`-fms-extensions` is load-bearing — see patch 4.

## 2. `hostcompat/` on the include path, first

- **`stddef.h`** — the decomp ships `include/stddef.h` as an empty stub for the
  IRIX toolchain. On a host build `PR/ultratypes.h:78` includes it, gets
  nothing, and fails on `typedef ptrdiff_t ssize_t;`. The shim provides the
  types via compiler builtins (`__PTRDIFF_TYPE__` etc.) rather than vendoring a
  licensed header.
- **`ge_gbi_compat.h`** — see patch 3.

## 3. `Gfx` typed accessors — 651 errors

`PR/gbi.h:1741` gates every typed view into a display-list command behind
`IS_BIG_ENDIAN && !IS_64_BIT`. On x86-64 they vanish and `Gfx` has only `words`;
gbi.h's own comment explains that the bitfield layouts do not match elsewhere.

The whole codebase uses **six fields across 20 sites**. Converted:

| file | sites | change |
|---|---|---|
| `src/game/bg.c` | 6 | `.dma.cmd/.par/.addr` → `GFX_CMD/GFX_DMA_PAR/GFX_DMA_ADDR` |
| `src/game/lightfixture.c` | 6 | `.dma.cmd`, `.tri.tri.v[i]` → `GFX_CMD`, `GFX_TRI_V` |
| `src/game/unk_092E50.c` | 8 | `.loadtile.sl/.tl` **writes** → `GFX_SET_LOADTILE_*` |

`unk_092E50.c` matters: those are assignment targets, not reads. A bitfield
write is a read-modify-write of the containing word, so rvalue macros cannot
serve — the setters do the masked insert explicitly.

## 4. `inherits` needs `-fms-extensions` — 654 errors

`src/bondtypes.h:43` is `#define inherits struct`, so `inherits coord16;` inside
a struct body becomes a bare `struct coord16;`. IDO treated that as an anonymous
member, giving the outer struct the inner one's fields. GCC reads it as a
forward declaration that declares nothing — hence `'StandTilePoint' has no
member named 'x'` and ~650 similar.

`-fms-extensions` restores the anonymous-member reading. **On its own it makes
things worse** (77 → 31 files) because it exposes one name collision that then
fails in a header and takes down every file including it.

## 5. `CCTVRecord::pad` → `lookpad` — unlocks patch 4, and broke the ROM

`src/bondtypes.h:3100`. `CCTVRecord` inherits `ObjectRecord`, which already has
`pad`, and declares its own. Once `inherits` is a real anonymous member the two
collide — 102 `duplicate member 'pad'` errors from **one line**. The original
comment reads `// lookpad`, so the rename matches the author's intent.

I renamed it unconditionally, reasoning that member names do not affect the
emitted binary. **The ROM build then mismatched.** Object-level diffing against
a pristine build isolated it to a single instruction in one file:

```
src/game/prop.c, setupCctv():   if (arg1->pad >= 0)

pristine   f00:  8e020080   lw  v0,128(s0)    <- CCTVRecord::pad,   s32 @ 0x80
renamed    f00:  86020006   lh  v0,6(s0)      <- ObjectRecord::pad, s16 @ 0x06
```

The name was not decoration — it was resolving the collision. With `pad` gone
from the outer struct, `arg1->pad` silently fell through to the *inherited*
member: a different field, of a different width, at a different offset. It
compiled without a warning, and on the host it would have read the wrong bytes
forever.

**Fix:** rename only on the host, and read the field through an accessor.

```c
#ifdef TARGET_N64
        s32 pad;      /* original spelling -- do not change */
#else
        s32 lookpad;
#endif

#define CCTV_PAD(p)  ((p)->pad)      /* TARGET_N64 */
#define CCTV_PAD(p)  ((p)->lookpad)  /* host */
```

Four call sites, all in `setupCctv()`. On N64 the macro expands to the original
expression, so codegen is unchanged — confirmed by rebuilding to
`abe01e4a…`.

**The general rule this cost me:** a host patch must leave the *IDO token
stream* identical, not merely be semantically equivalent. Renames, added
parentheses, reordered declarations and portable-but-different expressions all
qualify as changes until a build says otherwise. Assume nothing here; run
`make` and diff the objects.

## 6. `IS_EMPTY` pastes its argument — 340 errors

`include/CPPLib.h:256`. Full write-up in `decomp-p1-token-pasting.md`. Applied
guarded:

```c
#ifdef TARGET_N64
    /* original */
#else
    /* portable emptiness test — never pastes the argument */
#endif
```

## 7. Circular include — 200 errors

`src/bondtypes.h:31` includes `game/chrobjdata.h`, which includes `bondtypes.h`
straight back. The guard is already set so the second include is a no-op,
leaving `chrobjdata.h:20` using `struct ItemModelFileRecord` ~1,380 lines before
`bondtypes.h` defines it.

Fix: move the `chrobjdata.h` include to the end of `bondtypes.h`, after the
record structs exist.

> **Correction.** This was originally diagnosed in `PRIORITIES.md` as "missing
> generated headers" — that the structs came from `scripts/generate_chr_c.py`.
> That was wrong. They are defined in `bondtypes.h` all along; the problem was
> purely ordering.

---

## 8. `BITFLAG` was compiled out entirely — 120 errors

`src/bondconstants.h:80` gated the real macro behind `#ifdef __sgi`. Every
compiler that is not IDO got `#define BITFLAG(...)` — an empty macro producing
nothing. So every bitflag enum in the game simply did not exist on a host
build: `PLAYERFLAG`, `RUNTIMEBITFLAG`, and the rest of the family, along with
every error that cascaded from their members being undeclared.

Two changes:

- Widen the gate to `#if defined(__sgi) || defined(GE_HOST_PORT)`. This is why
  patch 6 matters — the machinery only works on GCC once `IS_EMPTY` is portable.
- Make `BITFLAG` variadic and pad. Call sites omit trailing arguments; IDO
  allowed that, standard C does not (`requires 33 arguments, but only 4 given`).
  A variadic front-end pads with 32 empties and forwards to the original
  fixed-arity body, which is left exactly as written.

**This single fix took the census from 79 to 119 files.**

## 9. `osSyncPrintf` arity — 14 errors

Five files define it as a zero-argument no-op and then call it with arguments.
Changed to `#define osSyncPrintf(...)`.

## 10. Pointers stored in `u32` tables — 59 errors

`src/game/propobj.c` holds 57 AI/monitor script tables whose cells are either a
small opcode or a **pointer to another script**:

```c
u32 monAnimRadarSub1[] = { MONRGBA(COLOR_GREEN, 20), MONJUMPTO(monAnimRadarSub3) };
/*                                                   MONJUMPTO(p) -> 0x9, p     */
```

On N64 a pointer is 32 bits, so `&monAnimRadarSub3` fits a `u32` and the linker
resolves it as a relocation. On x86-64 the same initialiser is a 64→32
truncation, which is not computable at load time.

**This is a genuine 64-bit porting problem, not a toolchain difference** — the
same family as the `OS_K0_TO_PHYSICAL` truncation risk flagged in
`src/ultra/rdram.h`.

**Decision: retype the tables to `uintptr_t`** via
`typedef uintptr_t MonScriptWord;` in `chrai.h` (31 externs, 57 definitions).
`uintptr_t` is exactly "an integer that can hold a pointer". On N64 it is `u32`,
so the matching ROM build is byte-identical; on a host it is 64-bit and the
address fits with no truncation.

Rejected alternatives:

- *Runtime fixup pass* — write placeholder zeros and patch addresses at
  startup. Works, but needs a generated fixup table and touches 57 sites for no
  benefit over a type that already means the right thing.
- *Build the game 32-bit (`-m32`)* — would make this whole class of problem
  vanish. Ruled out: RT64 is 64-bit, and you cannot link a 32-bit game with a
  64-bit renderer in one process.

---

## 11. Two more invalid-paste families

- **`DEFINED()` pastes its argument**, same defect as `IS_EMPTY`. At
  `bondaicommands.h:281`, `DEFINED(SETUPSUBROUTINES(ID))` pastes onto a literal
  `)` because `SETUPSUBROUTINES` is an optional hook that is never defined.
  Fixed by defining it to `0` for the host build — which makes the paste legal
  *and* is semantically right: no extra subroutines contributes nothing to the
  OR.
- **Redundant `##` at token boundaries.** `SKELETON( ## NAME ## )` pastes `(`
  onto an identifier and an identifier onto `)`; `& ## NAME` pastes `&` onto an
  identifier. None of these are valid, and none of them do anything — the inner
  macro already performs the real paste. Removed 8 + 9 occurrences across
  `bondconstants.h` and `PR/gbi.h`. The resulting token stream is identical, so
  both builds are unaffected.

## 12. Omitted-argument macros — the largest single family

IDO allowed call sites to pass fewer arguments than a macro declares. Standard C
does not. The same variadic-front-end-plus-padding fix applies throughout:

```c
#define M(...)   M_I(__VA_ARGS__, <N empties>)
#define M_I(a, b, ..., z, ...)   /* original body, unchanged */
```

Applied to: `BITFLAG` (33), `SWITCH` (49), `EXPAND_ARGS_STACK` (33),
`New_Vector` / `New_Coord3d` (3), `BREAK`, and 22 `TRY*` AI macros that `SWITCH`
invokes with an extra label argument.

`SWITCH`'s parameters are named `CASE0`–`CASE9` then `CASEA`–`CASEF` in **hex**,
not `CASE10`–`CASE15`. Worth knowing: a regex looking for `CASE_CONTENT15` finds
nothing and will silently patch the wrong place.

---

## 13. `SWITCH` — where the padding trick *breaks*

`SWITCH` looked like patch 12 again, and the same variadic front-end was applied
to it. It was wrong, and it was wrong silently.

Substituting `__VA_ARGS__` into another macro's argument list **macro-expands it
first**. A `SWITCH` case body is an AI command sequence:

```c
PlayAnimation(ANIM_yawning, 0, 193, ...) BREAK
```

which expands to a comma-separated byte list *before* the fixed-arity macro ever
separates its arguments — and every one of those commas then counts as a
separator. Measured: a 4-case `SWITCH` arrived as **99 arguments instead of
13**, and the case content the body finally saw was the single token `0x10`.

An object-like alias (`#define SWITCH SWITCH_I`) avoids the extra expansion —
source tokens are separated raw — but needs the compiler to tolerate omitted
trailing arguments, which IDO does and standard C does not.

**Fix: pad at the call site.** There are four `SWITCH` invocations in the whole
game, all in `chraidata.c`; each now passes all 49 arguments explicitly. Both
compilers then see one fixed-arity macro with a matching count and no front-end
is needed at all. Empty cases expand to nothing, so the emitted bytes are
unchanged.

The general lesson generalises past this macro: **the padding trick is only safe
when the arguments are inert tokens.** It is fine for `BITFLAG` (enum names) and
wrong for anything whose arguments are themselves macro calls.

## 14. `CALL` emits one comma too many on GCC — 1 error

`CALL(AI_LIST_ID)` in the generated `src/aicommands2.h` ends with a deferred
comma, and its taken branch already ends in one (`SetChrAiList()` emits it). GCC
produces `... 0x00FF , ,` — an empty array element. IDO produces a single comma
from the identical macro: its `DEFER`/`EVAL` rescan does not materialise the
deferred `COMMA` on top of the one the command already emitted.

Confirmed by preprocessing the same input through both compilers and diffing.
Host-only variant without the trailing comma; the N64 macro is untouched. The
only branch that needs the extra comma is `AI_ERR_NO_THIS`, which is a
compile-error diagnostic by design.

## 15. The last five, one at a time

Not a family — five individual cases of IDO leniency.

| file | issue | fix |
|---|---|---|
| `ob.h:51` | `fileGetIndex` declared `char *`, defined `u8 *` | aligned the declaration to the definition; the body only forwards to `strcmp()`, and a pointer's pointee type does not affect the MIPS calling convention |
| `propobj.c:9814` | `append_text_picked_up(u8*, u8*, u8*)` called with `(u8*, AMMOTYPE, u32)` | retyped to what the callers pass; both parameters are **unused**, and enum/u32/pointer are one 32-bit register argument either way |
| `vtxstore.c:162` | `modelGetNodeRwData(var_v0->chrflags, …)` missing a cast the neighbouring line already has | added `(Model *)`; a pointer cast emits nothing |
| `chraction.c:2485` | `s16 mrs[3] = metal_ricochet_SFX;` — array-from-array initializer | host aliases instead of copying (`s16 *mrs = …`); `mrs` is only ever read |
| `bg.c:230` | flexible array member initialised inside an array of structs | host spells it as the byte stream it already is — every member is `u8`, alignment 1, same 20 bytes; the only reader casts to `u8 *` and walks it byte-wise |

The last one is worth a second look: `s_specialportal` ends in a flexible array
member, so the entries are **different sizes** and `specialportalarray` is not
really an array at all. There is no standard-C struct spelling for it. Writing
the bytes out is not a workaround, it is the honest description.


---

## 16. What clang found that GCC did not

The port targets Windows as well, where clang-cl or MinGW clang is the likely
toolchain, so the census now takes `CC`. Running it under clang found **six
issues GCC had accepted silently** — the same shape of problem as patch 5, where
"it compiles" was not the same as "it is right".

**First, two flag-level differences.** Clang 16+ promoted
`-Wint-conversion` and `-Wimplicit-function-declaration` to hard errors, and
`-w` does not suppress errors. These are exactly the two leniencies the decomp's
own Makefile switches off in IDO — warnings 709 ("incompatible pointer type
assignment") and 712 ("illegal combination of pointer and integer"). The
codebase is full of them *by design*, so the census downgrades them rather than
"fixing" thousands of intentional sites.

**Then the real finding: `#pragma weak A = B`.** Three files use it to give one
object two names:

```c
s32 g_DebugPortalsInputBuffer1 = 0;
extern s32 g_DebugPortalsInputBufferSource1;
#pragma weak g_DebugPortalsInputBufferSource1 = g_DebugPortalsInputBuffer1
```

GCC accepts it. Clang creates the alias and then rejects **every use** as
`reference to 'X' is ambiguous`, because two entities with that name are now in
scope. Since the entire point is that they are the same object, the portable
spelling is a macro — placed before the `extern` declarations so those become
redeclarations of the real objects rather than new undefined symbols.

One of the three needed care: `spectrum.c` aliases an `extern u8` **scalar** to
an array object, and is assigned as `spec_keyboard_row_caps_z_x_c_v = 0xff`. The
alias names *byte 0*, not the array, so the macro is
`(spec_keyboard_buffer[0])`. The naive one-to-one macro compiled under GCC and
would have been wrong.

| file | what clang caught |
|---|---|
| `lv.c` | 4 `#pragma weak` aliases → macros |
| `spectrum.c` | scalar-aliased-to-array; macro must name byte 0 |
| `objective_status.c` | `#pragma weak` alias → macro |
| `front.c:2405` | bare `return;` from an `s32` function; the one caller discards the result |

**Worth doing again.** A second compiler is the cheapest adversarial reviewer
available: it re-checks every assumption for free, and here it caught an
aliasing bug that GCC would have carried into the Windows build silently.

---

## 17. `struct huft` doubles on a 64-bit host, and its pool is sized in bytes

`src/game/zlib.h`:

```c
struct huft { u8 e; u8 b; union { u16 n; struct huft *t; } v; };
```

The union holds a pointer, so the entry is **8 bytes on the N64 and 16 on the
host**. The Huffman pool is declared in BYTES for the 8-byte entry:

```
src/game/ob.c:39     u8  buffer[0x2100];
src/game/ob.c:77     u8  buffer[8448];
src/game/bg.c:2258   u8  buffer[0x2100];
src/game/image.c:160 u8  scratch[0x2100];
```

so the same table overruns each array by 2x. Measured rather than reasoned: the
distance table was allocated at `tl + 0x2520` — 9504 bytes into an 8448-byte
stack array.

**The symptom was a hang, not a crash**, which is why it survived so long: the
smash corrupted the Huffman tables, the decoder produced garbage, and the game's
own in-place overrun trap (`zlib.c:366`, `while(1){}`) caught it a long way from
the cause.

Scaled by `sizeof(void *) / 4`, so the ENTRY count is what stays fixed — that is
the quantity the decompressor actually bounds. `#ifdef GE_HOST_PORT` with the
original declaration verbatim in `#else`, so the IDO token stream is untouched.

**Not fixed, and worth knowing:** `src/music.c` declares `struct huft hlist;` —
a *single* entry — and passes its address as the pool. That is already writing
past it on the N64; the host just does so twice as fast.

## 18. `expand_ani_table_entries` walks an `s32[]` through an `s32**`

`src/game/initanitable.c`. `animation_table_ptrs1` is `s32[]` — 32-bit offsets —
and the function reads it as `s32**`. On the N64 a pointer is 32 bits, so a read
and a `var_v0++` both move 4 bytes and the two views agree. On a 64-bit host each
read takes 8 bytes and the cursor skips a whole entry.

The sign is the other half. Every step goes through `s32`, and these are RDRAM
addresses at 0x80000000 and up — negative as `s32`, so the value sign-extends to
`0xFFFFFFFF8xxxxxxx` on the way back into a pointer:

```
expand_ani_table_entries   base 0xffffffff807034dc   SIGSEGV
```

Same family as `mempCheckMemflagTokens`, the `MonScriptWord` tables and
`randomSetSeed` (§ above), and it fails for exactly the half of RDRAM with the
top bit set. Rewritten over `u32`, guarded.

**The family is bounded, which is the useful part:** 103 sites cast an address
through `(s32)`/`(int)`, across 14 files, and **72 of them are in
`src/game/chraction.c`**. Before working through those, a SIGSEGV handler that
recognises a faulting address shaped like `0xFFFFFFFF8xxxxxxx` and names it would
turn each into a one-line fix.

## A standing caution for 17 and 18

Both are `#ifdef GE_HOST_PORT` with the N64 line verbatim in `#else`, which is
the containment strategy §5 established. **That has not been re-verified against
`make`**: this work was done in an environment with no IDO toolchain. Before
trusting it, run the check that §5 exists for:

```bash
cd /path/to/n64decomp-007 && make && sha1sum build/u/ge007.u.z64
# must still be abe01e4aeb033b6c0836819f549c791b26cfde83
```


## 19. `ANIM_DATA_*` are offsets, not objects

The animation_data segment has VRAM base 0, so on the N64 a symbol's ADDRESS is
its offset within it, and that is the only way the game uses these:

```c
modelSetAnimation(m, &ptr_animation_table->data[(s32)&ANIM_DATA_idle], ...)
```

The segment is loaded whole and separately by `alloc_load_expand_ani_table()`,
so the CONTENT is never read through the symbol. A host link finds them
undefined and makes them BSS arrays at ~0x20000000 -- and that subscript then
runs half a gigabyte past a 0xFFFF-byte table.

`hostcompat/ge_anim_offsets.h` defines each as the byte AT its offset, so
`&NAME` is the offset and every use site works unchanged. Same technique as
`ge_segments_compat.h`; the 35 `extern` declarations in
`assets/animationtable_data.h` are guarded, because a macro and a declaration of
that name cannot coexist.

## 20. `ModelAnimation` -- a third struct whose host layout does not match its data

```c
s32 address;                        // 0x00
u16 unk04; u8 unk06; u8 unk07;
ModelAnimBitField *bitDescriptors;  // 0x08
u16 unk0C; u16 unk0E;
u8 *bitStream;                      // 0x10
```

The offsets in those comments are the contract: instances of this struct ARE
cartridge data, romCopy'd in whole. On a 64-bit host the two pointers become 8
bytes, moving `unk0C` to 0x10 and `bitStream` to 0x18, so every field past 0x08
is read from the wrong place. Pinned to `u32` on the host, with `GE_PTR()` at
the two use sites in `model.c`.

**This is the third of these** -- after `OSThread` (448 vs 472) and `struct huft`
(8 vs 16). Three is a pattern, not a coincidence: any struct with a pointer
member whose instances come from the ROM has it. Many structs in
`src/bondtypes.h` carry their N64 offsets in comments, which makes an audit
possible; see PRIORITIES.md P3h.

## 21. The sign-extension family, and the tool for it

`src/host/ge_fault.c` installs a SIGSEGV handler that recognises a faulting
address of the form `0xFFFFFFFF8xxxxxxx`, names it as an RDRAM pointer truncated
to `s32`, and prints the real address. It reports and re-raises; it deliberately
does not repair the access and continue, which would hide the very sites it
exists to enumerate.

`hostcompat/ge_addr_compat.h` provides `GE_U32()` and `GE_PTR()` for use inside
`#ifdef GE_HOST_PORT` branches only. They are not applied to the N64 path: the
rule from S5 is that the IDO token stream must be identical, and leaving the
original line verbatim in the `#else` is a fact rather than an argument.

Fixed so far: `expand_ani_table_entries` (S18) and `initactorpropstuff.c:111`.
Remaining: 103 sites across 14 files, 72 of them in `src/game/chraction.c`.

## 22. `Gwords` is two doublewords on a 64-bit host

`include/PR/gbi.h`:

```c
typedef struct { uintptr_t w0; uintptr_t w1; } Gwords;
```

`uintptr_t` is 4 bytes on the N64 and 8 here, so `sizeof(Gfx)` was 16 and every
display list had a hole in the middle of it. The game's own writes went in at
double stride; RDRAM read back as `bc000006 00000000 | 00000000 00000000`.

Guarded to `u32` under `GE_HOST_PORT`. This broke `src/debugmenu.c`, where
`gsDPLoadTextureBlock` is used in a static initialiser and the address is no
longer a compile-time constant -- fixed with a zero placeholder plus a
`GE_INIT_STMT` fixup that locates the command by SCANNING FOR ITS OPCODE rather
than by index, so it cannot drift if the macro's expansion changes.

## 23. Segments and files that arrive big-endian

Not a single family with a single answer -- a family with one answer per format,
which is the point. Each of these is converted at exactly one place, chosen so
nothing reads the data before it and nothing converts it twice:

| data | where | shape |
|---|---|---|
| `fontdl` segment | `lvInit` after `romCopy` | pure `Gfx`; blanket u32 |
| model texture table | `texLoadFromModelFileHeader` | one u32 per 12-byte entry |
| model switch table | `sub_GAME_7F075A90` | u32 per entry, before PROMOTE |
| model child records | the promote walk | per-record |
| compressed-MIDI header | `alCSeqNew` | READ big-endian, never swapped |
| ramrom demo header | `replay_recorded_ramrom_at_address` | field by field |
| BG file | `load_bg_file` | per-section, see 25 |
| `.stan` file | `stanDetermineEOF` | in the walk that already knows the sizes |
| setup lists | `proplvreset2` | per-list, before relocation |
| room vertices | `bgLoadRoomVtxData` | 6 x u16, colour bytes untouched |
| room display lists | `bgLoadRoom*Gdl` | blanket u32 |

The MIDI header is READS rather than an in-place swap because `alCSeqSetLoc`
calls `alCSeqNew` a second time on the same buffer -- an in-place swap would undo
itself.

The image table is converted in `texLoadFromModelFileHeader` and NOWHERE else,
because `texLoad` overwrites `TextureID` with a physical address on the very next
line. A swap anywhere downstream reverses an address, not a cartridge word: it
turned 0x001ACED8 back into 0xD8CE1A00 and the shadow pass dereferenced it.

## 24. `geSwapOnce` keys on the address, and addresses get reused

The "already converted" registry is a set of pointers. A file loaded over a
buffer a previous file used therefore looks like data already converted, and the
new records are left big-endian. The Nintendo logo model's root node came through
with `Next` = 0x54000005 instead of 0x05000054.

`geSwapOnceForget(base, size)` drops every record in a range, and
`fileIndexLoadToAddr` / `fileIndexLoadToBank` call it before they overwrite.
The rule: **"already swapped" is a property of the BYTES, not of the address.**

## 25. Two counts for one header, and the two bugs that follow

The BG file's section table is FIVE words. The game reads indices 0 through 4,
and section 1 -- the room list -- begins at file offset 0x14, which is where the
table ends.

The loader's first read is a 0x40-byte probe into a stack buffer, done only to
reach `roomlist[1].pPointTableBin` and learn the file size. Those 64 bytes are
the table plus the first two room records, all 32-bit, so all sixteen words must
be converted there.

Get it backwards either way and the symptom is nothing like the cause:

- sixteen words over the real file reaches 0x2C into the room list and converts
  the first rooms a second time -- rooms 0 and 1 big-endian, room 2 onward fine;
- five words over the probe leaves the size field reversed, and the loader asks
  for a 1.75 MB file that is 7 KB.

## 26. `GFX_CMD` must SIGN-EXTEND

`PR/gbi.h` declares the opcode `int cmd : 8`. Signed. So `G_ENDDL` is
`(G_IMMFIRST-7)` = -72 and `g->dma.cmd` for the byte 0xB8 is -72.

`hostcompat/ge_gbi_compat.h` returned `(w0 >> 24) & 0xFF` = 184, which equals
none of the opcode constants. Every display-list scan in the game ran forever.
It presented as the level loader hanging with a verified-correct list whose
terminator sat at command 356 of 358.

Only the opcode is signed; the other accessors read `unsigned int` fields and
stay unsigned. That asymmetry is gbi.h's, not a choice made here.

## 27. Thread stacks belong in RDRAM

The renderer puts stack objects into display lists --
`gSPMatrix(gdl++, osVirtualToPhysical(mtx))` with `mtx` a local. That works on
the N64 because the thread stacks are in RDRAM. Host stacks made
`osVirtualToPhysical` return 0, and made the whole port non-deterministic:
truncating a 64-bit stack address keeps whichever 32 bits ASLR happened to give
it, and the same binary died at frame 7490, 4960 and 4930 on three runs.

`src/ultra/fiber.cpp` allocates stacks from `kStackRegionBase` -- four megabytes
mapped above the eight the game can see, inside the same RDRAM allocation and so
inside every bounds check. The game's allocator runs to 0x807FE000, measured, so
there was no room below it.

## 28. Bitfield allocation order, and the fix that scales

Covered in PRIORITIES.md P3o. The short version: reverse the DECLARATION order
under `GE_HOST_PORT`, so LSB-first allocation lands the fields where MSB-first
put them. Every read site is then correct unchanged -- including the ones written
as shifts, which do not look like bitfield accesses and would never be found by
searching for one.

Done for `StandTileHeaderMid` and `StandTileHeaderTail`. Any other bitfield read
from cartridge data needs the same treatment and there is no audit for it yet.

## 29. Enum signedness

Covered in PRIORITIES.md P3o. `tools/force_signed_enums.py` reports; applying it
wholesale regresses the port, so the two enums that needed it are guarded by
hand. **A blanket edit to a matching decompilation needs evidence per site.**

## 30. Two strides, when the host struct cannot be pinned

`tools/pin_structs.py` makes a cartridge struct match the cartridge by turning
its pointer members into 4-byte `GE_N64PTR`s, and everything else in the setup
file is handled that way. The level-setup RECORDS cannot be: `src/game/gobjdata.c`
and its siblings are static initializer tables of `ObjectRecord` and friends, and
clang refuses an address-space-cast pointer in a static initializer -- 12 files
and 525 undefined symbols, measured. `AIListRecord` hits the same wall, from the
AI action-block tables.

So those structs are 128 bytes on the cartridge and 144 here, and there are two
different strides for one record type:

| | where it comes from | what it is |
|---|---|---|
| FILE | `sizepropdef`'s N64 value | how far to the next record in the setup file |
| MEMORY | `sizeof` on the host | how far apart they must sit to not overlap |

Both now come from one table, `hostcompat/ge_propdef_sizes.h`, read by
`geFilePropDefBytes()` in `prop.c` and by a `GE_HOST_PORT` branch at the top of
`sizepropdef()`. `tools/gen_struct_expand.py` generates a `geExpand_T(dst, src)`
per record type that moves every leaf field from its cartridge offset to its host
offset, byte-reversing on the way and widening pointers with zero extension.

**Both halves are needed and each failed on its own.** With the file stride right
and the memory stride left at Rare's constant, `PROPDEF_ARMOUR` records sat 136
bytes apart while `BodyArmourRecord` is 152 here -- so `domakedefaultobj` wrote
`shadecol` and `nextcol` over the next record's four-byte header, destroying its
type tag. The walk then read a 34-record run of `PROPDEF_NOTHING` out of the
middle of a door.

`geSwapSetupPropDefs()` finishes by walking the list it just built with the
game's own `sizepropdef()` and checking it lands exactly on the terminator. That
invariant is what a stride bug trips over, and it costs one walk per level load.

## 31. Positional puns into a word that was swapped

A byte swap fixes a word's VALUE. It does not preserve which BYTE a pun reads.

`stanMatchTileName` compares `(u16)tile->x` and `*(u8 *)&tile->y` -- a `StandTile`
reinterpreted as a `StandTilePoint`, so bytes 0-1 and byte 2 of a word that the
loader already converted. Both name the wrong halves here. Measured on Frigate's
275 pads:

| reading | result |
|---|---|
| the positional puns | 0 by name, 216 by search, 59 unresolved |
| bits 31..8, i.e. what `u32 id : 24; u8 room;` says | 0, 216, 59 |
| bits 23..0 | **270 by name, 4 by search, 1 unresolved** |

The struct is not the authority here; the data is. The 59 unresolved pads were
props whose null stan went into `walkTilesBetweenPoints` and crashed the load.

## 32. Matching hacks that are out-of-bounds accesses

`init_path_table_links` spells its loop cursor `validationGroupCursors[-3]` -- a
negative index into a one-element array, there to line the stack frame up with
IDO's. On the N64 that slot is real. Here it reads and writes whatever the host
compiler put three slots below, four times over, and the `do`/`while` never
terminated: the level load ran for five minutes without leaving its first
waygroup, with no fault to point at.

Given its own variable behind `GE_VGCURSOR` on the host path, with the original
spelling kept for the N64. **This is a class, not an instance** -- a matching
decompilation contains deliberate out-of-bounds accesses that are harmless only
because IDO's frame layout made them so, and none of them will announce
themselves. There is no audit for this yet; a hang with a plausible backtrace is
the symptom to watch for.

## 33. The intro section, and the union rule that falls out of it

The setup file has a SECOND type-tagged variable-length list, and it needs the
same two-stride treatment as 30. `SetupIntroCamera` carries two
`union { integer; char *; }` members and a `prev` pointer, so it is 40 bytes on
the cartridge and 56 here; every other intro record is a run of `s32` and is the
same size on both machines. One record changing size is enough, because the
walker strides by `sizeof`.

`GE_INTRO_TABLE` lives beside `GE_PROPDEF_TABLE` in `hostcompat/ge_propdef_sizes.h`
and the expanders come from the same generator. The tag differs -- a full `s32`
at offset 0 rather than a byte at offset 3 -- so it is read big-endian before
anything is converted.

**The rule this produced**, now the primary union policy in
`tools/gen_struct_expand.py`:

> The file cannot contain a host pointer. Every union in these records that
> changes size does so because one arm is a pointer, and that arm is always the
> runtime one -- the file holds an index or a pair of ids and the game
> overwrites it with an address once the thing it names exists. So the pointer
> arms are dropped and the remaining arms decide the conversion, by computed
> leaf shape rather than by eye.

It resolves `union { PropRecord *first; s32 Index1; }` and
`union { u16 lang_index[2]; char *lang_ptr; }` by the same reasoning, and it
still leaves `union { s8 keyID; u32 keyflags; }`, `Mtxf` and `rgba_u8` as
hand-written hooks -- so `gen_struct_swap.py` and `gen_struct_expand.py` continue
to agree about which unions are genuinely ambiguous, which is the property that
makes an undefined hook meaningful.

`fs15_16` is a new hook and a good example of the bar. It is a 15.16 fixed-point
word with four spellings, one of which splits it into two 16-bit halves, so the
arms genuinely disagree. The loader reads `.ival` and divides --
`unk04.fval = unk04.ival / 100.0f` in `bondview_r.c:336` -- so the cartridge
stores scaled integers and the word reading is not merely the live one, it is the
only one that could be right. `fval` on unconverted bytes would be a denormal.
