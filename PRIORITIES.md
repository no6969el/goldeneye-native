# Priority list

Ordered by what unblocks the most, with sizes grounded in measurement rather
than guesswork. Updated as findings change the picture.

**Rule used to order this:** a task ranks high if it is on the critical path to a
*running game*, or if leaving it undone means other work is being validated
against assumptions instead of reality.

---

## The measurement that reordered everything

Until now, every part of this project was validated against synthetic tests or
extracted data. **The game's own C had never been compiled.** That was the
largest unexamined assumption in the project, so I measured it:

```
134 files in src/game/
 33 compile clean against the shim  (25%)   <- baseline
134 compile clean after P0-P3      (100%)   <- now, 0 errors
```

Error count fell from ~1,400 to zero. All applied fixes are documented in
`patches/HOST-PORT-PATCHES.md`; the patch itself is
`patches/decomp-host-port.patch`.

Roughly 1,400 errors, but they collapse into a small number of root causes. The
biggest one is not a porting nuisance — it is a structural finding (P0 below).

Reproduce with `tools/compile_census.sh`.

---

## The second measurement: does the ROM still match?  — **YES, VERIFIED**

The host patches were justified with an argument — "everything is guarded or
name-only, so `make` stays byte-identical." Arguments are not evidence. Built it:

```
pristine tree                abe01e4aeb033b6c0836819f549c791b26cfde83   MATCH
tree with the host patches   5cdf2fa9d5b5e55ff1ce60b8522905e840ced868   MISMATCH
```

So the argument was wrong. Rather than bisect by rebuilding subsets, I built
both trees and diffed the **object files** — one differed (`src/game/prop.o`),
by **one instruction**. That named the culprit outright: the `CCTVRecord::pad`
rename, which silently re-resolved `arg1->pad` to the inherited
`ObjectRecord::pad` — different field, different width, different offset. Full
write-up in `patches/HOST-PORT-PATCHES.md` §5.

After making the rename host-only and routing the four call sites through
`CCTV_PAD()`:

```
all host patches applied     abe01e4aeb033b6c0836819f549c791b26cfde83   MATCH
host compile census          134 / 134 files, 0 errors
```

Both invariants hold at once. **Object-file diffing should be the default
technique here** — it localises to one instruction in two builds, where
file-group bisection took three builds and had not yet converged.

Standing rule going forward: a host patch must leave the IDO *token stream*
identical, not merely be semantically equivalent — and `make` decides that, not
reasoning.

---

## P0 — `Gfx` typed accessors do not exist on a host target  — **ACCESSORS DONE**

**651 of ~1,400 errors — 46% — from one cause.**

`PR/gbi.h:1741`:

```c
typedef union {
    Gwords      words;
#if !defined(F3D_OLD) && IS_BIG_ENDIAN && !IS_64_BIT
    Gdma        dma;
    Gtri        tri;
    /* ...every other typed accessor... */
#endif
    long long int force_structure_alignment;
} Gfx;
```

The typed views into a display-list command exist **only on a big-endian 32-bit
target**. On x86-64 they compile away and `Gfx` has nothing but `words`. gbi.h's
own comment says why: the bitfield layouts do not match on other targets.

This matters far more than the error count suggests, because game code reads
display lists back through those accessors:

- `src/game/bg.c:2772` — `gdl[cmdindex].dma.cmd == G_VTX`
- `src/game/lightfixture.c:180` — `while (gfx->dma.cmd != G_VTX)`

I flagged these call sites in the first architecture doc as the reason the port
must keep building real GBI in memory rather than replacing the `gSP*` macros
with immediate-mode calls. That was right, but the reason was incomplete: it is
not merely that the game *reads* display lists, it is that it reads them through
structs that **silently vanish** off a big-endian 32-bit target.

**Fix:** provide host-endian accessors in `hostcompat/` that extract from
`words` with explicit shifts — exactly what `src/gbi/gbi_interp.cpp` already
does (`uint8_t(w0 >> 24)`). Then the read-back sites work unchanged.

**Status: the compat header is written and validated.**
`hostcompat/ge_gbi_compat.h` provides `GFX_CMD`, `GFX_DMA_PAR/LEN/ADDR`,
`GFX_TRI_V`, `GFX_LOADTILE_*`. Rather than trust the shift arithmetic, the
macros were run over all 1,937 real display lists: the `GFX_CMD` histogram
matches `dl_validate` exactly (53,440 `G_TRI4`; 22,649 `G_VTX`; 9,675
`G_SETTEX`; 1,937 `G_ENDDL`; 1,991 `G_TRI1`), and every `G_VTX` decodes to a
legal 1..16 count and every `G_TRI1` index to a valid ×10 value.

**Remaining:** convert the ~17 call sites (`.dma.cmd` → `GFX_CMD(...)` etc.).
Despite the 651-error count the codebase uses only six distinct fields; the
errors multiply because the headers are included everywhere.

**Size:** small — one header (done) plus ~17 mechanical edits. **Value:**
unblocks ~46% of failures and removes a class of silent-miscompile risk.

---

## P1 — `##` token pasting against float literals  — **FIX WRITTEN & VERIFIED**

**340 errors (275 + 65).** `include/CPPLib.h:257` pastes `_` onto a macro
argument that is sometimes a float literal — `PROPFILERECORD(alarm1, 0.1)`
produces `_ ## 0.1`. IDO accepted it; GCC and Clang reject it, correctly, since
`0.1` is not a valid preprocessing token to paste onto.

**Root cause:** `_IS_EMPTY(x)` does `CAT(_IS_EMPTY, _##x##_)`. That needs
`_##x##_` to be a valid preprocessing token, and `_0.1` is not — the `.`
terminates the identifier. The construct was always outside the standard; IDO
was simply lenient.

**Status: replacement written and verified** — `patches/decomp-p1-token-pasting.md`.
Uses the portable emptiness idiom that never pastes the argument, only the
resulting flags. Compiled and run against all six relevant cases including the
two that break today (`0.1`, `1.0`); results identical to intent.

**Apply guarded by `#ifdef TARGET_N64`.** The decomp is a *matching*
decompilation and `IS_EMPTY` feeds symbol-name construction; leaving the N64
path untouched keeps `make` byte-identical. Same containment strategy as
`hostcompat/`.

**Size:** small. **Value:** 24% of failures.

---

## P2 — Circular include  — **DONE**

**200 errors — DONE.**

> **My original diagnosis was wrong.** I recorded this as "missing generated
> headers", assuming the structs came from `scripts/generate_chr_c.py`. They do
> not — they are defined in `src/bondtypes.h:1399` all along.

The real cause is a circular include. `bondtypes.h:31` includes
`game/chrobjdata.h`, which includes `bondtypes.h` straight back; the guard is
already set so that is a no-op, leaving `chrobjdata.h:20` using
`struct ItemModelFileRecord` about 1,380 lines before it is defined.

**Fix applied:** move the `chrobjdata.h` include to the end of `bondtypes.h`.

### P2b — `inherits` (found while fixing P2) — **DONE**

654 errors, invisible until P0 cleared. `bondtypes.h:43` is
`#define inherits struct`, so `inherits coord16;` becomes a bare
`struct coord16;` inside a struct body. IDO read that as an anonymous member;
GCC reads a forward declaration that declares nothing.

`-fms-extensions` restores it — but **on its own it makes things worse**
(77 → 31 files), because it exposes a single name collision that fails inside a
header and takes down everything including it. All 102 collisions were the same
member: `CCTVRecord::pad`, colliding with the inherited `ObjectRecord::pad`, on
one line. Renamed to `lookpad` (which is what its own comment calls it; member
names do not affect the emitted binary).

One rename + one flag cleared 654 errors.

---

## P3 — Finish the compile, then link  — **COMPILE DONE (134/134)**

The compile is finished. Both invariants are verified together:

```
tools/compile_census.sh   134 / 134 files, 0 errors   (GCC)
CC=clang  ... same        134 / 134 files, 0 errors   (clang)
tools/link_census.sh      241 objects; 0 symbols of port work left
tools/link_game.sh        239 objects -> a real executable that boots to init()
make && sha1sum           abe01e4aeb033b6c0836819f549c791b26cfde83   MATCH
```

**The target is Windows**, so the census now takes `CC` and is run under both
compilers. Clang found six issues GCC accepted silently — most importantly three
`#pragma weak` aliases, one of which aliases a **scalar to an array** and whose
obvious macro replacement compiled fine under GCC and was wrong. Details in
`patches/HOST-PORT-PATCHES.md` §16. A second compiler is the cheapest
adversarial review available and should stay in the loop.

The last round was five individual IDO-leniency cases plus two real preprocessor
findings (`SWITCH`, `CALL`) — see `patches/HOST-PORT-PATCHES.md` §13–15.

**The `SWITCH` finding is the one worth carrying forward.** The
variadic-front-end padding used throughout this port re-expands its arguments
before the fixed-arity macro separates them. That is harmless when the arguments
are inert tokens and silently destructive when they are macro calls: a 4-case
`SWITCH` reached its body as 99 arguments with the case content reduced to a
single token. It compiled. Only a diff against IDO's own preprocessor output
caught it — the same lesson as the `pad` rename, in a different disguise.

### The link census — M1's coverage, finally measured

`tools/link_census.sh` compiles the game to **real object files** (not
`-fsyntax-only`) and asks the symbol table what is missing. Syntax proves the
source parses; only the symbol table proves there is an implementation behind
it.

```
src/game/              134 / 134 objects
src/ (system)           28 /  28 objects
src/ultra/ (shim)        6 objects

unresolved: 976
  asset data (built from the user's ROM)      827
  ---- actual port work: 149 ----
  linker segment symbols                       34
  libaudio sequencer (al*)                     30
  libultra (os*/__os*)                         41
  gu* matrix helpers                           13
  everything else                              31
```

**Every C file in the decomp now compiles to a host object.** That includes the
28 system files outside `src/game/`, which the compile census never covered —
three of them needed `-D_LANGUAGE_C`, a macro IDO predefines. Without it
`PR/ultratypes.h` expands to nothing while still setting its include guard, so
any file reaching `<PR/os.h>` before `<ultra64.h>` never gets `s32` at all
(4,464 errors in `boss.c`, all "unknown type name").

**827 of the 976 unresolved symbols are asset data** — character models
(`C…Z`), props, `ANIM_DATA_*`, `globalDL_*`. Not port work: they come from the
decomp's own asset pipeline, built from the user's ROM. The census identifies
them by asking the decomp's built asset objects what they define, rather than
pattern-matching names, so this split is measured rather than estimated.

That leaves **149 symbols of genuine work**, and they are not evenly hard:

| group | was | now | how |
|---|---|---|---|
| `gu*` matrix helpers | 13 | **0** | compiled the decomp's own `src/libultra/gu/` |
| libaudio sequencer (`al*`) | 30 | **0** | compiled the decomp's own `src/libultra/audio/` |
| linker segment symbols | 34 | **0** | extracted table + guarded macros (below) |
| `os*` / `__os*` | 41 | 41 | the genuinely hardware-bound ones — next |
| everything else | 31 | 25 | `random*`, TLB helpers, microcode blob bounds |

**149 → 66, and most of it was not implementation work.**

#### `gu*` and `al*`: the decomp already had them

I had these queued as "implement 43 functions", with a note that the `gu*` math
was already written and tested in `src/xr_math.cpp`. That framing was wrong.
libultra's *software* parts are shipped in the decomp — `gu/` is pure matrix
math, `audio/` is the libaudio sequencer and bank parser (CPU code; only the
synthesis kernels run on the RSP, and those are the ACMD interpreter's job), and
`libc/` is string and printf.

**All 61 files compiled for the host with one signature fix** (`_Printf`
declared `u8 *`, defined `char *`). Reimplementing them would have been slower
*and* less faithful — these are the actual implementations the game was built
against, not my reading of what they should do.

The line the census draws is now explicit: `io/` and `os/` are excluded because
those are the hardware ones — PI, SI, AI, DP, VI, SP, threads, TLB, caches — and
replacing them is the entire point of `src/ultra/`.

#### Segment symbols: the one that needed a decision

The game reaches its own ROM layout through linker-defined symbols:

```c
romCopy(dst, &_animation_dataSegmentRomStart, size);
size = (u32)&_fontdlSegmentRomEnd - (u32)&_fontdlSegmentRomStart;
```

On N64 these are absolute link-time constants. **A host toolchain cannot give an
ordinary symbol a chosen address, and on Windows there is no portable way to
define an absolute symbol at all** — so the port cannot reproduce them as
symbols. It needs their values.

Those values only exist after linking, so they cannot come from `ge007.ld`. But
they are fixed per ROM version, and the port already requires the user's own ROM
and checks its SHA1. So: `tools/extract_segments.py` reads a linked map file and
emits `hostcompat/ge_segments.h` — 111 constants keyed by ROM SHA1 — and
`hostcompat/ge_segments_compat.h` turns each name into a macro naming the byte
*at* that address, so `&name` is the address and **every use site works
unchanged**. The 42 `extern` declarations are guarded, because a macro and a
declaration of the same name cannot coexist.

Two things the extraction got wrong at first, both caught by validating rather
than trusting:

- **ld prints two line shapes.** Plain definitions, and `PROVIDE`-style
  assignments (`_codeSegmentRomStart = _startSegmentRomEnd`) where the resolved
  value is on the left and an expression follows. Matching only the first shape
  silently dropped **95 of 116 symbols** — and silently, because what remained
  looked like a plausible table.
- **Five `get_*Segment*` "symbols" are functions**, not constants: `src/boot.s`
  defines two-instruction accessors that return a linker symbol. Their addresses
  are code addresses in the TLB region, so leaving them in produced entries that
  looked like wild ROM offsets. They are now real functions in
  `src/ultra/segments.c`.

`geSegmentsCheckRom()` refuses a ROM whose SHA1 does not match the table. A
near-miss would make every offset subtly wrong and surface much later as corrupt
textures or a hang in decompression — the kind of bug that is miserable to trace
back to its cause.



### Original P3 notes (now historical)

Whatever remains after P0–P2: `osSyncPrintf` arity (14, trivial), non-constant
initialisers (59), stragglers. Then link against the shim and find the missing
libultra symbols — which is the first true test of M1's API coverage.

**Now bounded.** ~204 errors remain:

| count | issue |
|---|---|
| 74 | undeclared identifiers |
| 59 | initializer element not computable at load time |
| 46 | `unknown type name 'PLAYERFLAG'` |
| 14 | `osSyncPrintf` arity |
| ~10 | more `##` pasting, `src/bondaicommands.h:281` |

`PLAYERFLAG` and the residual pasting look like one root cause — `PLAYERFLAG`
comes from `BITFLAG(...)` in `bondconstants.h:505`, and `bondaicommands.h:281`
pastes `)` onto `_` in the same CPPLib macro family. Fixing that paste should
clear ~56 of the 204.

The 59 non-computable initialisers are the genuinely interesting ones: almost
certainly static initialisers taking addresses of other objects, which IDO
folded at link time. Those may need a runtime init pass, not a macro fix — the
first item so far that is a real porting decision rather than a toolchain
difference.

---

## P3b — The link gap is closed — **DONE**

```
unresolved: 827
  asset data (built from the user's ROM)      822
  ---- actual port work: 5 ----
  (host libc/compiler, resolved at link)        5
```

**Zero symbols of port work remain.** 149 -> 0. The 822 are asset data the
decomp's own pipeline emits from the user's ROM; the 5 are `memset`, `memcmp`,
`sqrtf` and friends, which the linker supplies and which only appear here
because this census never actually links.

What the last 66 turned into:

| group | how it closed |
|---|---|
| 41 `os*` / `__os*` | `src/ultra/os_hw.c` + `os_sp.c` + two exports from the scheduler |
| 6 microcode blob bounds | generated from the decomp's own extracted `bin/*.bin` |
| 3 TLB helpers | `src/ultra/os_tlb.c` — genuine no-ops, see below |
| 3 RNG symbols | `src/ultra/random.c` — a real implementation, with a test |
| the rest | `bcopy`/`bzero`/`__libm_qnan_f`, and reclassifying libc |

### The rule this stage was written under

A file of stubs returning plausible values is the same trap as the `pad` rename,
with better camouflage. So every function in `os_hw.c` carries one of four
labels, and the label is the claim:

- **REAL** — implemented, behaves as the hardware did.
- **NO-OP** — doing nothing *is* correct here. Cache maintenance on coherent
  memory: our "RSP" is a function call on the same CPU reading the same memory,
  so there is nothing to flush. A fact, not a shortcut.
- **ABSENT** — reports hardware that genuinely is not there. "No Controller Pak
  inserted" is a state the game already handles, because it happens on real
  hardware. Faking a working save device would corrupt saves instead.
- **TODO** — not implemented, and says so on stderr the first time it is called.
  It never fabricates a value that lets the caller carry on as if it worked.

### `os_sp.c` is the one that decides what a frame looks like

`osSpTaskLoad` / `osSpTaskStartGo` is where the game hands work to the RSP, and
therefore where the port takes it away: graphics tasks to the display-list
interpreter and RT64, audio tasks to the ACMD interpreter. Dispatch is by
callback, because the shim is libultra's replacement and libultra does not know
what a renderer is.

An unrecognised task type is **reported, not guessed**. Feeding an audio command
list to the display-list walker produces a spectacular crash a long way from the
cause.

`osSpTaskYielded` returns 0 (ran to completion) rather than OS_TASK_YIELDED.
Here a task is a synchronous call that has already finished, so that is the
truth — and claiming a yield would leave the game's scheduler waiting forever
for a resume that never comes.

### Three things measurement caught that reasoning would not have

- **`__libm_qnan_f` is not the canonical quiet NaN.** I wrote
  `__builtin_nanf("")` (0x7FC00000). The real value is **0x7F810000** — quiet bit
  clear, a *signalling* NaN with payload 0x10000. `__builtin_nansf("0x10000")`
  reproduces it, confirmed by bit-comparing rather than by reasoning. Any test
  asking only "is it NaN" would have passed.
- **`randomSetSeed` sign-extends.** The parameter is declared `u32`, but the
  MIPS o32 ABI delivers 32-bit arguments sign-extended and `daddiu` then works on
  all 64 bits. Seed 0x80000000 becomes 0xFFFFFFFF80000001, not 0x80000001 — a
  different starting state and therefore a different game, for **half of all
  possible seeds**. The obvious `state = seed + 1` is wrong.
- **The decomp's `stdarg.h` stub shadows the compiler's.** Any file that includes
  `<stdio.h>` fails inside glibc with `unknown type name '__gnuc_va_list'` —
  a system header failing, in a file that never mentions varargs.
  `hostcompat/stdarg.h` fixes it the same way `stddef.h` did.

The RNG got `tests/test_random.cpp` rather than trust: an instruction-by-
instruction transcription checked against an independently-derived algebraic
form over a million iterations, plus a check that the generator does not
collapse to a short cycle — which a botched xorshift usually does, and which
"does it produce numbers" would never notice.

### What is NOT done, and must not be mistaken for done

**Linking is not booting.** Every symbol resolves; nothing has been run. Known
gaps that will surface the moment it does:

- `jump_decompressfile` returns failure — the boot decompressor is not wired to
  `src/inflate` yet. This is the next real blocker.
- Asset objects have to be produced from the user's ROM and put on the link line.
- No handlers are registered with `os_sp.c` yet, so a graphics task currently
  draws nothing and says so.
- EEPROM reports absent, so the game will run and be unable to save.

---

## P3c — It links, and it runs — **MILESTONE**

```
$ tools/link_game.sh /path/to/n64decomp-007
objects: 239
LINKED: ge007  (2379024 bytes)

$ ge007 --rom ge007.u.z64 --frames 5
RDRAM  : 8 MB at 0x80000000
ROM    : ge007.u.z64 (12582912 bytes, "GOLDENEYE")
         sha1 abe01e4aeb033b6c0836819f549c791b26cfde83
assets : 821 symbols defined, NOT YET LOADED
rsp    : placeholder handlers -- no renderer attached
boot   : calling init()
```

**There is an executable.** It maps RDRAM, mounts a real ROM, identifies it,
runs the game's own `init()` to completion and reaches the scheduler's first
context switch, where it currently faults inside `swapcontext`. That is the
frontier.

`tools/link_census.sh` answers "does every symbol have a definition?".
`tools/link_game.sh` answers the harder question -- and the two differ. Things
that only appeared at the real link:

- **`-fms-extensions` missing from the link script.** 16 files failed and
  produced 350 "undefined reference" lines that read exactly like missing game
  code. That flag is what makes `inherits` an anonymous member.
- **`multiple definition of osMotorInit`.** `src/motor.c` is the decomp's own
  rumble implementation and `src/ultra/os_io.cpp` provides the host one.
- **17 asset symbols with no size.** `nm -S` covers symbols with a `.size`
  directive; the obseg assembly uses `end_X` labels, `ramrom.s` uses `X_end`,
  and some are a plain `.incbin` of a file on disk. Reading only the first form
  gave 267 of 822 and looked like a plausible answer.

### The addressing decision, and why it had to be made here

The game does this on the first function it runs (`src/init.c`):

```c
csegmentSegmentVaddrStart = get_csegmentSegmentStart();   /* 0x80020D90 */
dataziprom = csegmentSegmentVaddrStart;
datazipram[j] = dataziprom[j];                            /* dereferenced */
```

It takes an N64 address, puts it in a `u8 *`, and dereferences it. There are
hundreds of sites like it and no realistic way to find them all -- each one
missed is a wild pointer, not a compile error.

So the port **maps RDRAM at KSEG0 (0x80000000)**, with KSEG1 (0xA0000000) as a
shared alias of the same pages rather than a second copy: two independent blocks
would give the CPU and the interpreter different views of the same display list,
which presents as random corruption. `tests/test_rdram_map.cpp` reproduces the
`init.c` pattern exactly.

**That was not sufficient, and running it is what proved so.** The first run
faulted storing to 0x7020159F -- inside the TLB-mapped inflate segment. My claim
in `os_tlb.c` that "the port maps every address directly" covered every address
reached through RDRAM and none reached through the TLB. Two 16 MB windows at
0x70000000 and 0x7F000000 now cover them, with an honest note that they are
separate memory rather than aliases of RDRAM, and what the symptom would be if
something turns out to depend on that.

### Two findings worth carrying forward

- **The game's libc overrides the host's, for the whole program.**
  `src/sprintf.c` defines `sprintf`; once linked, *every* caller gets it,
  including port code. The ROM hash printed as `0078383000783830...` -- a
  repeating pattern that reads as a broken hash rather than a broken formatter.
  `src/host/sha1.c` now formats hex by hand and says why.
- **Boot-time decompression cannot work on a host and does not need to.** It
  unpacks the code segment `boot.s` DMA'd from the cartridge; here that code was
  compiled into the executable, so the copy reads zeroed memory and the
  decompressor walks off the end of its input -- it faulted in `inflate_stored()`
  reading 0x71000000 after consuming ~14 MB of nothing. Guarded for the host.
  The decompressor itself is still linked and still used for level data.

### Next, in order

1. The `swapcontext` fault at the first context switch.
2. Load the assets -- `src/host/ge_assets.c` defines 821 symbols and a manifest;
   they are all zero until something fills them from the ROM.
3. Attach RT64 to the graphics handler in `os_sp.c`.

---

## P3d — It boots, runs frames, and loads assets — **CURRENT FRONTIER**

```
$ ge007 --rom ge007.u.z64 --frames 10
RDRAM  : 8 MB at 0x80000000
ROM    : ge007.u.z64 (12582912 bytes, "GOLDENEYE")
         sha1 abe01e4aeb033b6c0836819f549c791b26cfde83
assets : 764 loaded from ROM, 57 SKIPPED (no ROM offset, or out of range)
rsp    : placeholder handlers -- no renderer attached
boot   : calling init()
threads (at exit):
  id 5   pri 40  waiting   blocked on a queue
  id 2   pri 30  waiting   blocked on a queue
  id 0   pri 250 stopped   not queued
  id 1   pri 0   RUNNING   on run queue
  id 3   pri 10  waiting   blocked on a queue
  switches=27 blocks=16 sends=14 recvs=10 timers=1
  events: 9 delivered, 1 undelivered (no queue registered)
frames : 10
tasks  : 0 graphics, 0 audio, 0 unrecognised
```

No crash, no hang, clean exit. The game boots, creates its five threads, runs
its scheduler, receives retrace events, and blocks and wakes across ten frames.
It does not yet issue a graphics task -- that is the frontier.

### The bug that was costing everything: 448 vs 472

The port faulted inside `swapcontext()` at the first context switch. The cause
was nowhere near the scheduler.

The game allocates its own threads as statics (`OSThread mainThread;` in
`src/init.c`) compiled against the decomp's `<PR/os.h>`. The shim had appended
three host-only pointers to *its* `OSThread`. So the game's was **448 bytes and
the shim's was 472**, and every `osCreateThread()` wrote 24 bytes past the end
of the caller's object, over whatever static followed it.

The fix puts the host state inside `context` -- the saved MIPS register file,
400 bytes that exist only for source compatibility and are never used on a host.
Both definitions are now 448, so the disagreement is impossible rather than
merely corrected.

`tests/test_abi_layout.cpp` checks every type both sides define. It cannot
include both headers at once -- they define the same type names, which is
exactly why the drift was invisible -- so `tools/check_abi.sh` compiles a probe
against the decomp's headers and prints the numbers. **The first draft of that
test asserted `sizeof(OSMesgQueue) == 32` from memory; the real answer is 40,
and the "mismatch" it reported was the test being wrong.** Hence the tool.

### A cooperative scheduler cannot host a busy-wait idle thread

`idleproc` is `for (;;);`. On N64 that is correct -- it runs at the lowest
priority and the VI interrupt preempts it. This scheduler has no preemption by
design, so once idle ran, nothing else ever did.

`geIdleWait()` hands control back to the host frame loop and stays runnable,
which is what the interrupt did. Explicitly **not** a scheduler heuristic like
"if the only runnable thread is low priority, give up" -- that would guess at
which thread is the idle one and would change behaviour for any game thread that
happens to sit at a low priority.

### Assets: the offsets are in the map, not the objects

`nm` on an unlinked object reports section-relative addresses. `CarmourguardZ`
reads as 0x002B4530 there and actually lives at **0x006ECB90**. Loading from the
object address would have read a completely different part of the ROM and looked
like corrupt assets rather than a wrong offset.

Verified rather than assumed: the ROM at each obseg map offset begins with the
`1172` magic that marks a compressed asset -- the same format documented back in
the display-list work.

764 of 821 load. The 57 skipped have no entry in the map; they are **skipped,
not clamped**, because a partial asset decompresses into plausible garbage
instead of failing.

### Next, in order

1. **Why no graphics task.** The scheduler thread (pri 40) and main (pri 30)
   both block and wake each frame but never reach a frame submission. The 57
   unloaded assets are the first suspect; the second is a message queue the port
   is not feeding.
2. Attach RT64 to the graphics handler in `os_sp.c`.
3. Then, and only then, VR.

---

## P3e — Boot progresses to asset decompression

The port now boots past `init()`, runs the game's scheduler, loads assets from
the ROM, and gets far enough to be **decompressing a real language file** before
it stalls. Where it stops has moved four times this session, each time for a
different and specific reason.

### What the debugging actually found

Working backwards from a `SIGSEGV` in `memcpy`, in order:

1. **`os_scheduler` was healthy all along.** The first suspicion was the
   scheduler blocking in `joyPoll()`. It was not: that `osRecvMesg` passes
   flags=0, which is `OS_MESG_NOBLOCK`. Reading the flag value rather than the
   function name settled it.
2. **Main was waiting on a 100 ms timer, four times.** `bossInitMainthreadData()`
   arms one per controller. That is ~24 frames of startup, and every run so far
   had used `--frames 10`. Nothing was wrong; the run was too short.
3. **PI DMA rejected any destination outside RDRAM.** Correct on N64, where every
   static IS in RDRAM. In this port the game's statics live in the host
   executable's BSS, so a DMA destination is simply a CPU pointer. The check was
   testing something that is no longer true.
4. **The heap spanned from RDRAM to a host static.** `boss.c:218` builds the main
   memory pool from `&_bssSegmentEnd` (0x8008E360, correct) up to
   `tlbmanageGetTlbAllocatedBlock()`, which is placed below `&sp_boot` — a host
   address of 0x536000. The size underflowed and the allocator returned pointers
   into `.text`; the symptom was `memcpy` writing into the middle of a FUNCTION.
   The block now sits near the top of RDRAM, giving the game ~7 MB.
5. **Two pointer-through-`int` truncations.** `mempCheckMemflagTokens(int, int)`
   and a local `s32 mempStart` both carry pointer values. On a 64-bit host
   0x8008E360 becomes a negative `int` and sign-extends back to
   0xFFFFFFFF8008E360. Same family as the `MonScriptWord` tables, same fix —
   `uintptr_t`, guarded. **Half of all RDRAM addresses have the top bit set, so
   this fails for most of the heap and works for the rest.**
6. **The game uses asset symbol ADDRESSES as ROM offsets.**
   `doRomCopy(target, source = &LgunE, size)` feeds `&LgunE` to the PI as a
   device address, because on N64 a symbol's address *is* its cartridge
   position. Here `LgunE` is a generated BSS array, so the DMA read a
   plausible-looking but completely wrong part of the ROM, and the game's own
   zlib faulted on it. The port now translates host addresses back to ROM
   offsets through the asset manifest.

### Where it stops

`lvlStageLoad` completes. The game reaches its **main loop** -- `bossMainloop`
-> `lvlViewMoveTick` -> `MoveBond` -- with Bond standing on a real collision
tile in Frigate at (531.8, 356.2, -1320.5), spawned from the setup file's own
start pad.

It dies in `bondviewTrySimpleMovePlayerCollision` (bondview2.c:2267) with
`next_pos` NaN, on a tick where the stick is centred. The player's position and
tile are both sound, so the NaN is produced inside the movement computation
rather than carried in from the level data. That is the next thread, and it is
the first bug in this port that is about the game's own arithmetic rather than
about the shape of cartridge data.

## P3p -- the level setup file, and the two-stride problem


The propDef records were the last big-endian section and turned out to be a
different KIND of problem from every other one. They are variable-length and
type-tagged, which was expected; what was not is that they cannot be pinned.

`ObjectRecord` is 128 bytes on the cartridge and 144 on the host, because `prop`
and `model` are four bytes there and eight here. `tools/pin_structs.py` would fix
that and does not compile: `src/game/gobjdata.c` and its siblings are static
initializer tables of these very structs, and clang refuses an address-space-cast
pointer in a static initializer. 12 files, 525 undefined symbols, measured.
`AIListRecord` hits the same wall from the AI action-block tables.

So a record has TWO strides -- one in the file, one in memory -- and they are
different numbers. `tools/gen_struct_expand.py` generates the conversion: it
builds the same headers twice, with gcc `-m32` for the cartridge layout and with
clang for the host's (clang, because `GE_N64PTR` is `__ptr32` and gcc has no such
thing -- under gcc the "host" probe silently reports unpinned offsets), pairs
every leaf field by PATH, and emits a `geExpand_T(dst, src)` that moves each one
from the offset it has there to the offset it has here.

`hostcompat/ge_propdef_sizes.h` holds both strides in one table so they cannot
drift apart. Getting only one of them right is not half a fix: with the file
stride correct and the memory stride left at Rare's constant, `PROPDEF_ARMOUR`
records sat 136 bytes apart while `BodyArmourRecord` is 152 here, and
`domakedefaultobj` wrote `shadecol` over the next record's type tag.

### What else this unblocked, and what it says about the remaining bugs

Four bugs surfaced behind it, each a different class:

- **`pname` and `AIListRecord` unpinned.** Both are cartridge tables with a
  pointer member, so their relocation loops strode twice as far as the file and
  wrote host addresses past the end of the table -- over the pad-name strings.
  Every pad name in Frigate read as four good characters and then a pointer.
  `pname` could be pinned; `AIListRecord` could not, and is expanded instead.
- **A positional pun into a swapped word** (`stanMatchTileName`), which no
  amount of correct byte-swapping fixes. See HOST-PORT-PATCHES 31.
- **A matching hack that is an out-of-bounds access**
  (`validationGroupCursors[-3]`), which hung the load with no fault to point at.
  See HOST-PORT-PATCHES 32. This one is a class with no audit yet.
- **A word-swapped record header**, in the port's own conversion code, which
  moved the type tag from offset 3 to offset 0 and put every walker four bytes
  behind the list. Caught by the invariant that now runs after each conversion:
  walk the built list with the game's own `sizepropdef()` and check it lands
  exactly on the terminator.

Two stand-ins are in place and are marked as such in the code, both reporting
once rather than passing silently: one pad in Frigate ("p477e1") matches a tile
by name but tests as geometrically outside it, and is anchored to the named tile;
and a walk from a null tile answers "crossed nothing" instead of faulting.
Neither is a fix, and the second exists only to keep the first from being fatal.

### The intro section, and what it cost

`langGet` taking a null bank at `lv.c:574` turned out to be the same problem
again, one section over. The setup file's INTRO list is also type-tagged and
variable-length -- with a full `s32` tag at offset 0 rather than a byte at
offset 3 -- and `SetupIntroCamera` carries two `union { integer; char *; }`
members and a `prev` pointer, so it is 40 bytes on the cartridge and 56 here.
One record type changing size is enough: the walker strides by `sizeof`, so a
single camera put everything after it 16 bytes out, and the camera's own
`lang1c.lang_index[1]` was read from the middle of its predecessor.

Converting it needed no new machinery, which is the point -- `GE_INTRO_TABLE`
sits next to `GE_PROPDEF_TABLE` in the same header and the expanders come from
the same generator. It did improve the generator's union rule, and the
improvement is worth stating because it is a fact about the DATA rather than a
heuristic:

> **The file cannot contain a host pointer.** Every union in these records that
> changes size does so because one arm is a pointer, and that arm is always the
> runtime one -- the file holds an index, or a pair of ids, and the game
> overwrites it with an address later. So the pointer arms are dropped and the
> rest decide the conversion.

That one rule resolves `union { PropRecord *first; s32 Index1; }` and
`union { u16 lang_index[2]; char *lang_ptr; }` correctly and by the same
reasoning, and it leaves `union { s8 keyID; u32 keyflags; }`, `Mtxf` and
`rgba_u8` as hand-written hooks exactly as before -- so the two generators still
agree about which unions are genuinely ambiguous.

`fs15_16` is a new hook, and an instructive one. It is a 15.16 fixed-point word
with four spellings, one of which splits it into two 16-bit halves, so the
generator is right to call it ambiguous. The evidence settles it: the loader
reads `.ival` and divides -- `unk04.fval = unk04.ival / 100.0f` -- so the
cartridge stores scaled integers and the word reading is not merely live, it is
the only one that could be right. `fval` on unconverted bytes would be a
denormal.

### Next

1. **The NaN in `MoveBond`.** The current stop, and the first bug here that is
   about the game's arithmetic rather than the shape of its data.
2. **Tick a level.** Expect more of the same classes; they are all tooled now,
   except the out-of-bounds matching hacks, which are not.
3. **An actual image.** `tools/room_render.cpp` rasterises with no GPU. Wiring it
   to the graphics handler produces a PNG of a real game frame, in this
   container, without RT64 -- and that is the last milestone reachable without
   the target hardware.
4. Then RT64 on a machine with a display, then VR.
