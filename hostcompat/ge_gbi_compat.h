/*
 * hostcompat/ge_gbi_compat.h — host-endian access to display-list commands.
 *
 * WHY THIS EXISTS
 *
 * PR/gbi.h declares the typed views into a Gfx command like this:
 *
 *     typedef union {
 *         Gwords  words;
 *     #if !defined(F3D_OLD) && IS_BIG_ENDIAN && !IS_64_BIT
 *         Gdma    dma;
 *         Gtri    tri;
 *         ... every other typed accessor ...
 *     #endif
 *         long long int force_structure_alignment;
 *     } Gfx;
 *
 * Every typed accessor exists ONLY on a big-endian 32-bit target. On x86-64 they
 * compile away and Gfx has nothing but `words`. gbi.h's own comment gives the
 * reason: the bitfield layouts do not match on other targets, so exposing them
 * would be worse than omitting them — you would silently read the wrong bytes
 * instead of failing to compile.
 *
 * That is not a nuisance, it is the single largest compile blocker in the port:
 * 651 of ~1,400 errors in src/game/ trace to it. Game code reads display lists
 * back through those accessors, e.g.
 *
 *     src/game/bg.c:2772           gdl[cmdindex].dma.cmd == G_VTX
 *     src/game/lightfixture.c:180  while (gfx->dma.cmd != G_VTX)
 *
 * The macros below extract the same values from `words` with explicit shifts,
 * which is endianness-independent and is exactly what src/gbi/gbi_interp.cpp
 * already does. Call sites change from `g->dma.cmd` to `GFX_CMD(g)`.
 *
 * SCOPE: despite the error count, the whole codebase uses just six distinct
 * fields across roughly seventeen sites — .dma.cmd/.par/.addr, .tri.v, and
 * .loadtile.sl/.tl. The errors multiply because the headers are included
 * everywhere, not because the problem is broad.
 *
 * Field layouts are taken from PR/gbi.h's Gdma / Gtri / Gloadtile, interpreted
 * as the RSP sees them: bit 31 of w0 is the top of the command byte.
 */

#ifndef GE_GBI_COMPAT_H
#define GE_GBI_COMPAT_H

/* ---------------------------------------------------------------------------
 * MATCHING-BUILD NOTE — read before touching this.
 *
 * On N64 these MUST expand to the original bitfield accesses. A shift-and-mask
 * read is semantically identical but compiles to different instructions (lb vs
 * lw+srl+andi), and in a *matching* decompilation different instructions mean a
 * different ROM. Verified the hard way: using the portable forms unconditionally
 * built a ROM that linked fine and hashed differently.
 *
 * So: bitfields for IDO, shifts for the host. Same values, and the N64 output
 * stays byte-identical.
 * ------------------------------------------------------------------------- */
#ifdef TARGET_N64

#define GFX_CMD(g)      ((g)->dma.cmd)
#define GFX_DMA_PAR(g)  ((g)->dma.par)
#define GFX_DMA_LEN(g)  ((g)->dma.len)
#define GFX_DMA_ADDR(g) ((g)->dma.addr)
#define GFX_TRI_FLAG(g) ((g)->tri.tri.flag)
#define GFX_TRI_V(g, i) ((g)->tri.tri.v[i])
#define GFX_LOADTILE_SL(g)   ((g)->loadtile.sl)
#define GFX_LOADTILE_TL(g)   ((g)->loadtile.tl)
#define GFX_LOADTILE_TILE(g) ((g)->loadtile.tile)
#define GFX_LOADTILE_SH(g)   ((g)->loadtile.sh)
#define GFX_LOADTILE_TH(g)   ((g)->loadtile.th)
#define GFX_SET_LOADTILE_SL(g, v) ((g)->loadtile.sl = (v))
#define GFX_SET_LOADTILE_TL(g, v) ((g)->loadtile.tl = (v))
#define GFX_SET_CMD(g, v)         ((g)->dma.cmd = (v))

#else /* host build: the typed accessors do not exist -- see above */

/* Works on a Gfx* or a Gfx. Both spellings appear in the decomp. */
#define GE_GFX_W0(g) (((const Gfx *)(g))->words.w0)
#define GE_GFX_W1(g) (((const Gfx *)(g))->words.w1)

/* ---- Gdma: cmd:8, par:8, len:16 | addr ---- */

/*
 * SIGN-EXTENDED, and that is not a detail.
 *
 * PR/gbi.h declares the opcode as `int cmd : 8` -- a SIGNED eight-bit bitfield.
 * So on the N64 `g->dma.cmd` for the byte 0xB8 is -72, and the constants it is
 * compared against are negative too: G_IMMFIRST is -65 and G_ENDDL is
 * (G_IMMFIRST-7) = -72.
 *
 * An unsigned read returns 184, which equals none of them. Every display-list
 * scan in the game then runs forever: bgBuildRoomVtxBounds looking for the end
 * of a room's list, lightfixture.c looking for the last G_VTX. Observed as the
 * level loader hanging on room 1 of Archives, with a list that was verified
 * correct and a terminator sitting at command 356 of 358.
 *
 * The other accessors below read fields declared `unsigned int`, so they stay
 * unsigned. Only the opcode is signed, and only because of how gbi.h happens to
 * spell it.
 */
#define GFX_CMD(g)      ((int)(signed char)((GE_GFX_W0(g) >> 24) & 0xFF))
#define GFX_DMA_PAR(g)  ((unsigned)((GE_GFX_W0(g) >> 16) & 0xFF))
#define GFX_DMA_LEN(g)  ((unsigned)(GE_GFX_W0(g) & 0xFFFF))
#define GFX_DMA_ADDR(g) ((unsigned)(GE_GFX_W1(g)))

/*
 * ---- Gtri: cmd:8, pad:24 | Tri ----
 * Tri is { flag, v[3] } packed into w1, so v[i] is byte (i+1) counting from the
 * top of w1. Note these indices are the RAW command bytes: stock F3D stores
 * them pre-multiplied by 10, which is why the game's own decoder divides
 * (src/game/lightfixture.c:199). This macro does not divide — it reproduces
 * what the bitfield returned, nothing more.
 */
#define GFX_TRI_FLAG(g) ((unsigned)((GE_GFX_W1(g) >> 24) & 0xFF))
#define GFX_TRI_V(g, i) ((unsigned)((GE_GFX_W1(g) >> (16 - 8 * (i))) & 0xFF))

/* ---- Gloadtile: cmd:8, sl:12, tl:12 | pad:5, tile:3, sh:12, th:12 ---- */
#define GFX_LOADTILE_SL(g)   ((unsigned)((GE_GFX_W0(g) >> 12) & 0xFFF))
#define GFX_LOADTILE_TL(g)   ((unsigned)(GE_GFX_W0(g) & 0xFFF))
#define GFX_LOADTILE_TILE(g) ((unsigned)((GE_GFX_W1(g) >> 24) & 0x07))
#define GFX_LOADTILE_SH(g)   ((unsigned)((GE_GFX_W1(g) >> 12) & 0xFFF))
#define GFX_LOADTILE_TH(g)   ((unsigned)(GE_GFX_W1(g) & 0xFFF))

/*
 * SETTERS
 *
 * Not every use of these fields is a read. src/game/unk_092E50.c:264-273 uses
 * them as assignment targets:
 *
 *     MipMap2C_Something_Setup[2].loadtile.sl = flt_CODE_bss_80079E80;
 *
 * A bitfield write is a read-modify-write of the containing word, so the
 * rvalue macros above cannot serve — they are not lvalues, and even a compound
 * "lvalue macro" trick would clobber the neighbouring fields. These do the
 * masked insert explicitly.
 *
 * Note the source assigns a FLOAT to a 12-bit bitfield there. The original
 * compiler truncated toward zero on the implicit conversion; the casts below
 * preserve that, so behaviour matches rather than merely compiling.
 */
#define GE_GFX_W0_LV(g) (((Gfx *)(g))->words.w0)
#define GE_GFX_W1_LV(g) (((Gfx *)(g))->words.w1)

#define GFX_SET_LOADTILE_SL(g, v) \
    (GE_GFX_W0_LV(g) = (GE_GFX_W0_LV(g) & ~0x00FFF000u) | \
                       (((unsigned)(long)(v) & 0xFFFu) << 12))
#define GFX_SET_LOADTILE_TL(g, v) \
    (GE_GFX_W0_LV(g) = (GE_GFX_W0_LV(g) & ~0x00000FFFu) | \
                       ((unsigned)(long)(v) & 0xFFFu))
#define GFX_SET_CMD(g, v) \
    (GE_GFX_W0_LV(g) = (GE_GFX_W0_LV(g) & 0x00FFFFFFu) | \
                       (((unsigned)(v) & 0xFFu) << 24))

#endif /* TARGET_N64 */

#endif /* GE_GBI_COMPAT_H */
