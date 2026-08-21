/* ge_swap_util.c -- see the header. Hand-written; ge_swap.c is generated. */
#include <stdio.h>
#include <stdlib.h>

#include "ge_swap_util.h"

static void **ge_swapped;
static unsigned ge_swapped_n, ge_swapped_cap;

void geSwapOnceReset(void)
{
    free(ge_swapped);
    ge_swapped = 0;
    ge_swapped_n = ge_swapped_cap = 0;
}

int geSwapOnce(void *p, void (*fn)(void *))
{
    unsigned i;

    if (p == 0 || fn == 0) {
        return 0;
    }
    for (i = 0; i < ge_swapped_n; ++i) {
        if (ge_swapped[i] == p) {
            if (getenv("GE_SWAP_TRACE")) {
                fprintf(stderr, "[swap] DECLINE %p fn=%p\n", p, (void *)fn);
            }
            return 0;                    /* already done */
        }
    }
    if (ge_swapped_n == ge_swapped_cap) {
        unsigned cap = ge_swapped_cap ? ge_swapped_cap * 2u : 256u;
        void **grown = (void **)realloc(ge_swapped, cap * sizeof(*grown));
        if (grown == 0) {
            return 0;                    /* refuse rather than swap twice */
        }
        ge_swapped = grown;
        ge_swapped_cap = cap;
    }
    ge_swapped[ge_swapped_n++] = p;
    fn(p);
    return 1;
}

void geSwapOnceForget(void *base, unsigned int size)
{
    unsigned char *lo = (unsigned char *)base;
    unsigned char *hi = lo + size;
    unsigned i = 0;

    if (base == 0 || size == 0) {
        return;
    }

    /*
     * "Already swapped" is a property of the BYTES, not of the address. When a
     * file is loaded over a buffer, the records that used to live there are
     * gone and the new ones arrive big-endian -- but the address is the same,
     * so geSwapOnce recognised it and declined to swap.
     *
     * Measured symptom: the Nintendo logo model loaded into the same buffer a
     * previous model had used, its root node's Next offset came through
     * unswapped as 0x54000005 instead of 0x05000054, and the walker followed it
     * to 0xCF16C7D5 and died. Nothing about that crash points back here, which
     * is why this is worth the range scan.
     */
    while (i < ge_swapped_n) {
        unsigned char *q = (unsigned char *)ge_swapped[i];
        if (q >= lo && q < hi) {
            ge_swapped[i] = ge_swapped[--ge_swapped_n];   /* swap-with-last */
        } else {
            ++i;
        }
    }
}

void *geSwapOncePtr(void *p, void (*fn)(void *))
{
    geSwapOnce(p, fn);
    return p;
}

/* ---- ALWaveTable: the union needs `type`, so this is not generated -------- */

#define GE_AL_ADPCM_WAVE 0
#define GE_AL_RAW16_WAVE 1

void geSwap_ALWaveTable(void *p)
{
    unsigned char *b = (unsigned char *)p;
    unsigned char type;

    geSwap32(b + 0);          /* base */
    geSwap32(b + 4);          /* len  */
    type = b[8];              /* type: a byte, so no swap and safe to read */
    /* b[9] is flags, also a byte */

    if (type == GE_AL_ADPCM_WAVE) {
        geSwap32(b + 12);     /* waveInfo.adpcmWave.loop */
        geSwap32(b + 16);     /* waveInfo.adpcmWave.book */
    } else if (type == GE_AL_RAW16_WAVE) {
        geSwap32(b + 12);     /* waveInfo.rawWave.loop */
    }
    /* Any other type: leave the union alone rather than swap it as one of the
       two shapes above. A wrong swap here is silent; an unswapped one is not. */
}

unsigned int geBE32(const void *p)
{
    const unsigned char *b = (const unsigned char *)p;
    return ((unsigned int)b[0] << 24) | ((unsigned int)b[1] << 16)
         | ((unsigned int)b[2] << 8)  |  (unsigned int)b[3];
}

unsigned short geBE16(const void *p)
{
    const unsigned char *b = (const unsigned char *)p;
    return (unsigned short)(((unsigned int)b[0] << 8) | b[1]);
}

/* ---- union policies (see the header) ------------------------------------- */

/*
 * ModelRoData_HeaderRecord @8: `struct { u16 Group1; u16 Group2; }` or
 * `f32 GroupsAsF32` over the same four bytes. These are NOT interchangeable --
 * two 2-byte swaps and one 4-byte swap produce different bytes.
 *
 * Swapped as the u16 pair. The game reads Group1 as a matrix index
 * (model.c:1755, model.c:309) and the float spelling appears once; a pair of
 * small indices is also what the field names describe. If a model ever renders
 * with its parts in the wrong places, this is the first thing to reconsider.
 */
void geSwapUnion_ModelRoData_HeaderRecord__anon__at8(void *p)
{
    unsigned char *b = (unsigned char *)p;
    geSwap16(b + 0);
    geSwap16(b + 2);
}

/*
 * ModelRoData_Op11Record @0: `coord3d pos` (3 f32) or `u32 unk0c[16]`.
 * Not actually in conflict -- both are 32-bit quantities, and the array simply
 * covers more of them. Swapping all sixteen words is correct for either
 * reading. The generator flagged it only because the arms differ in LENGTH.
 */
void geSwapUnion_ModelRoData_Op11Record__anon__at0(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) {
        geSwap32((unsigned char *)p + i * 4);
    }
}

/* Vertex @8: display reading -- s16 s, s16 t. */
void geSwapUnion_Vertex__anon__at8(void *p)
{
    geSwap16((unsigned char *)p + 0);
    geSwap16((unsigned char *)p + 2);
}

/* Vertex @12: display reading -- four bytes (r,g,b,a or nx,ny,nz,nflag).
   Bytes need no swap, so this is deliberately empty rather than absent. */
void geSwapUnion_Vertex__anon__at12(void *p)
{
    (void)p;
}

/*
 * The collision reading of the same two unions: a 4-byte node pointer at 8 and
 * an s16 pair at 12.
 */
void geSwap_VertexCollision(void *p)
{
    unsigned char *b = (unsigned char *)p;
    geSwap16(b + 0);            /* coord.x */
    geSwap16(b + 2);            /* coord.y */
    geSwap16(b + 4);            /* coord.z */
    geSwap16(b + 6);            /* index   */
    geSwap32(b + 8);            /* CollisionRelatedNode */
    geSwap16(b + 12);           /* CollisionRelatedIndex */
    geSwap16(b + 14);           /* CollisionReserved */
}

/* ---- fonts --------------------------------------------------------------- */

#define GE_FONT_KERNING (13 * 13)
#define GE_FONT_CHARS   94
#define GE_FONTCHAR_SZ  24          /* 5 * s32 + one 4-byte pinned pointer */

void geSwap_font(void *p)
{
    unsigned char *b = (unsigned char *)p;
    int i;

    for (i = 0; i < GE_FONT_KERNING; ++i) {
        geSwap32(b + i * 4);
    }
    b += GE_FONT_KERNING * 4;
    for (i = 0; i < GE_FONT_CHARS; ++i) {
        geSwap_fontchar(b + i * GE_FONTCHAR_SZ);
    }
}

/* ---- whole-segment display lists ------------------------------------------ */

/*
 * A ROM segment that is NOTHING BUT Gfx commands, swapped as u32 words.
 *
 * The per-struct swappers exist because most cartridge blobs are a mixture and
 * only the format description says which field is which width. A display-list
 * segment is the one case where that ambiguity does not arise: a GBI command is
 * two big-endian u32s, and a segment declared as `Gfx dl[N]` in the decomp's own
 * assets/ source is a flat array of them with nothing else inside. That is a
 * fact about the source, not an assumption -- assets/font_dl.c is 24 gs*
 * initialisers and no other data.
 *
 * DO NOT reach for this for the room and model segments. Those interleave
 * display lists with vertices, and a u32 swap over a Vtx would put the s16
 * coordinates in the wrong order -- silently, since the result is still a
 * plausible-looking number. Those need the record-level swappers.
 *
 * Idempotence is NOT provided and cannot be: swapping twice restores the
 * original bytes and there is no spare bit in a Gfx word to record that it
 * happened. Call this exactly once, at the load site.
 */
void geSwapGfxBlob(void *p, unsigned int size)
{
    unsigned char *b = (unsigned char *)p;
    unsigned int i;

    /* Truncate rather than round up: a trailing partial word is not a command,
       and swapping past the end of the allocation to "finish" it would be a
       heap overrun in the name of tidiness. */
    for (i = 0; i + 4u <= size; i += 4u) {
        geSwap32(b + i);
    }
}

/* ---- image table entries -------------------------------------------------- */

/*
 * sImageTableEntry: one big-endian u32 followed by eight bytes.
 *
 * Hand-written rather than generated for the same reason geSwap_font is: it is
 * not in gen_struct_swap.py's input list, because nothing DECLARES an array of
 * them -- the model file carries a variable-length run reached through
 * ModelRoData_Op05Record::Images and indexed by a child record. The generator
 * only sees what the DWARF describes, and the DWARF describes a pointer.
 *
 * Only `index` needs it. width/height/level/format/depth/flagsS/flagsT/pad are
 * bytes and are already right.
 */
void geSwap_sImageTableEntry(void *p)
{
    geSwap32((unsigned char *)p);
}

/* ---- the level background file -------------------------------------------- */

/*
 * A BG file is a section table followed by rooms, portals and environment data,
 * DMA'd out of the cartridge whole and read in place. Every multi-byte field in
 * it is big-endian.
 *
 * This is NOT a blanket u32 swap, and the reason is worth stating: the file is
 * mostly 32-bit words -- offsets, floats, display-list commands -- but
 * bg_portal_data_entry is one u32 followed by FOUR SEPARATE BYTES, and
 * bg_envdata_entry is a byte plus padding plus a word. A blanket swap would
 * reverse those bytes and the portal graph would connect the wrong rooms, which
 * is the kind of wrongness that renders and looks almost right.
 *
 * So each list is walked with its own record shape. The lists are
 * self-terminating -- a room ends the array with a null mapping pointer, a
 * portal with a null offset, env data with a zero type -- and a swapped zero is
 * still zero, so the terminator can be tested after conversion without a
 * chicken-and-egg problem.
 *
 * The section table's offsets are stored biased: the game reaches a section with
 * BG_SEG_TO_PTR(base, off) = base + off + 0xF1000000 (src/game/bg.h). The same
 * bias is applied here rather than assumed away.
 */

/*
 * TWO different counts, for two different reads.
 *
 * The section table is exactly FIVE words. The game reads indices 0 through 4
 * and nothing else, and section 1 -- the room list -- begins at file offset
 * 0x14, which is where the table ends. So geSwapBgFile converts five, and lets
 * the room walk handle everything after.
 *
 * The loader's FIRST read is different: a 0x40-byte probe into a stack buffer,
 * done only to reach `roomlist[1].pPointTableBin` and learn how big the file
 * is. Those 64 bytes are the table plus the first two room records, and every
 * field in that range is 32 bits, so converting all sixteen words is both
 * correct and the only way the field at 0x2C gets converted at all.
 *
 * Getting this backwards is not subtle in its effects and is very subtle in its
 * appearance. Sixteen words over the real file reaches 0x2C into the room list
 * and converts the first rooms a SECOND time, so rooms 0 and 1 come out
 * big-endian while room 2 onwards are fine. Five words over the probe leaves
 * the size field reversed, and the loader asks for a 1.75 MB file that is 7 KB.
 * Both were observed.
 */
#define GE_BG_SECTIONS   5
#define GE_BG_PROBE_WORDS 0x10

#define GE_BG_SEG_BIAS   0xF1000000u

static unsigned char *geBgSection(unsigned char *base, unsigned int off)
{
    if (off == 0) {
        return 0;
    }
    return base + (unsigned int)(off + GE_BG_SEG_BIAS);
}

void geSwapBgHeader(void *base)
{
    unsigned char *b = (unsigned char *)base;
    unsigned i;

    /* The probe buffer: table plus the first room records, all 32-bit. */
    for (i = 0; i < GE_BG_PROBE_WORDS; ++i) {
        geSwap32(b + i * 4u);
    }
}

void geSwapBgFile(void *base)
{
    unsigned char *b = (unsigned char *)base;
    unsigned char *p;

    {
        unsigned i;
        for (i = 0; i < GE_BG_SECTIONS; ++i) {
            geSwap32(b + i * 4u);
        }
    }

    /* Section 1: bg_room_data[] -- three offsets and a coord3d, six 32-bit
       fields, terminated by a null pPriMappingBin (the SECOND word). */
    p = geBgSection(b, ((unsigned int *)b)[1]);
    if (p != 0) {
        unsigned index = 0;
        for (;;) {
            unsigned i;
            for (i = 0; i < 6; ++i) {
                geSwap32(p + i * 4u);
            }
            /*
             * Entry 0 is not a room. The game's own count starts at 1
             * (`for (i = 1; ...[i].pPriMappingBin != NULL; i++)` in
             * load_bg_file), and entry 0's mapping offset is zero -- so a loop
             * that tested the terminator from the start stopped after the first
             * record and left every actual room big-endian. The room sizes then
             * came out as differences between byte-reversed offsets, and the
             * loader asked for a block of -985,220,816 bytes.
             */
            if (index > 0 && ((unsigned int *)p)[1] == 0) {
                break;
            }
            ++index;
            p += 24;
        }
    }

    /* Section 2: bg_portal_data_entry[] -- one offset then four single bytes,
       which must NOT be touched. Terminated by a null offset. */
    p = geBgSection(b, ((unsigned int *)b)[2]);
    if (p != 0) {
        for (;;) {
            geSwap32(p);
            if (((unsigned int *)p)[0] == 0) {
                break;
            }
            p += 8;
        }
    }

    /* Section 3: bg_envdata_entry[] -- u8 type, three pad bytes, s32 data.
       Terminated by a zero type, which is a byte and needs no conversion. */
    p = geBgSection(b, ((unsigned int *)b)[3]);
    if (p != 0) {
        while (p[0] != 0) {
            geSwap32(p + 4);
            p += 8;
        }
    }
}

/* ---- room vertex runs ------------------------------------------------------ */

/*
 * A decompressed room point table: a flat run of Vtx, 16 bytes each.
 *
 *     s16 ob[3]; u16 flag; s16 tc[2]; u8 cn[4];
 *
 * Six 16-bit fields and then four bytes that must NOT be touched -- cn is the
 * vertex colour or normal, one byte per channel, and reversing it would tint
 * every surface in the level while leaving the geometry perfectly correct.
 * That is the failure mode worth guarding against here: a blanket u32 swap over
 * this run produces a level that renders, which is exactly why it would survive
 * a casual look.
 *
 * A trailing partial record is left alone rather than half-converted.
 */
void geSwapVtxRun(void *p, unsigned int size)
{
    unsigned char *b = (unsigned char *)p;
    unsigned int i;

    for (i = 0; i + 16u <= size; i += 16u) {
        geSwap16(b + i + 0);    /* ob[0] */
        geSwap16(b + i + 2);    /* ob[1] */
        geSwap16(b + i + 4);    /* ob[2] */
        geSwap16(b + i + 6);    /* flag  */
        geSwap16(b + i + 8);    /* tc[0] */
        geSwap16(b + i + 10);   /* tc[1] */
        /* +12..+15: cn[4], bytes. */
    }
}

/* ---- the level setup file --------------------------------------------------- */

/*
 * Everything in a setup file is 32-bit -- offsets, floats, IDs -- with a
 * handful of byte and half-word fields inside the path records. Each list is
 * self-terminating and each terminator survives conversion, which is what makes
 * a swap-then-test pass safe:
 *
 *   waypoints   until padID < 0          (-1 is 0xFFFFFFFF either way round)
 *   waygroups   until neighbours == 0
 *   ailists     until ailist == 0
 *   paths       until waypoints == 0
 *   pads        until plink == 0
 *   boundpads   until plink == 0
 *   id lists    until the entry is -1
 *
 * These run BEFORE proplvreset2's own relocation loops, which read the same
 * terminators and then add the file base to every offset. Converting after the
 * relocation would mean byte-reversing real addresses.
 */

static void geSwapWords(unsigned char *p, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; ++i) {
        geSwap32(p + i * 4u);
    }
}

void geSwapIdListMinus1(void *p)
{
    unsigned char *b = (unsigned char *)p;

    if (b == 0) {
        return;
    }
    for (;;) {
        geSwap32(b);
        if (*(int *)b < 0) {
            break;
        }
        b += 4;
    }
}

void geSwapSetupWaypoints(void *p)
{
    unsigned char *b = (unsigned char *)p;

    if (b == 0) {
        return;
    }
    for (;;) {
        geSwapWords(b, 4);              /* padID, neighbours, groupNum, dist */
        if (*(int *)b < 0) {
            break;
        }
        b += 16;
    }
}

void geSwapSetupWaygroups(void *p)
{
    unsigned char *b = (unsigned char *)p;

    if (b == 0) {
        return;
    }
    for (;;) {
        geSwapWords(b, 3);              /* neighbours, waypoints, dist */
        if (*(unsigned int *)b == 0) {
            break;
        }
        b += 12;
    }
}

void geSwapSetupAiLists(void *p)
{
    unsigned char *b = (unsigned char *)p;

    if (b == 0) {
        return;
    }
    for (;;) {
        geSwapWords(b, 2);              /* ailist, ID */
        if (*(unsigned int *)b == 0) {
            break;
        }
        b += 8;
    }
}

void geSwapSetupPaths(void *p)
{
    unsigned char *b = (unsigned char *)p;

    if (b == 0) {
        return;
    }
    for (;;) {
        /* waypoints (u32), then ID and isLoop as BYTES, then len as a u16.
           The two bytes are left alone; reversing them would swap a path's
           identity with its loop flag. */
        geSwap32(b + 0);
        geSwap16(b + 6);
        if (*(unsigned int *)b == 0) {
            break;
        }
        b += 8;
    }
}

void geSwapSetupPads(void *p)
{
    unsigned char *b = (unsigned char *)p;

    if (b == 0) {
        return;
    }
    /* PadRecord: pos, up, look (nine floats), plink, stan. 44 bytes, all
       32-bit. plink at +0x24 is the terminator. */
    for (;;) {
        geSwapWords(b, 11);
        if (*(unsigned int *)(b + 0x24) == 0) {
            break;
        }
        b += 44;
    }
}

void geSwapSetupBoundPads(void *p)
{
    unsigned char *b = (unsigned char *)p;

    if (b == 0) {
        return;
    }
    /* BoundPadRecord: a PadRecord followed by a bbox. 68 bytes, all 32-bit. */
    for (;;) {
        geSwapWords(b, 17);
        if (*(unsigned int *)(b + 0x24) == 0) {
            break;
        }
        b += 68;
    }
}
