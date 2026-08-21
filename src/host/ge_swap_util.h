/*
 * ge_swap_util.h -- hand-written companions to the GENERATED ge_swap.{c,h}.
 *
 * Separate file on purpose. ge_swap.c/h are rewritten in full every time
 * tools/gen_struct_swap.py runs, so anything hand-written there is silently
 * deleted the next time a format is added -- which is exactly what happened
 * once already, and is the same trap that ate geAssetsRomOffsetFor out of the
 * generated ge_assets.h. Generated files hold generated code only.
 */
#ifndef GE_SWAP_UTIL_H
#define GE_SWAP_UTIL_H

#include "ge_swap.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Swap a record exactly once.
 *
 * A record is often reachable by more than one route -- the animation table is
 * indexed by two arrays AND reached by offset through ANIM_PTR(). Swapping
 * twice restores the original bytes, which presents as the swap layer doing
 * nothing at all rather than as double application: the most misleading symptom
 * available.
 *
 * Deliberately not a flag inside the record. There is no spare bit in a format
 * the cartridge defines, and inventing one would corrupt the data this exists
 * to protect.
 */
int   geSwapOnce(void *p, void (*fn)(void *));

/* The same, returning p, for use inside an expression -- the game reaches
 * records through macros where there is nowhere to put a statement. */
void *geSwapOncePtr(void *p, void (*fn)(void *));

void  geSwapOnceReset(void);

/*
 * Forget every "already swapped" record inside [base, base+size).
 *
 * Call this wherever RAM is overwritten with fresh cartridge bytes -- a file
 * load, a decompression -- BEFORE anything reads the new data. Without it the
 * second model loaded into a reused buffer is left big-endian. See the .c file.
 */
void  geSwapOnceForget(void *base, unsigned int size);

/*
 * Read a big-endian value WITHOUT mutating the buffer.
 *
 * geSwap_*() converts a record in place, which is right when the port owns the
 * copy and everything downstream should see host order. It is wrong when the
 * game also WRITES to the same words -- a display list in the Globalimagetable
 * segment is read by the game as cartridge data and then patched with a host
 * address, so half of it ends up native and half big-endian, and an in-place
 * swap of the whole thing would corrupt whichever half it did not mean.
 *
 * These read one field, leave the bytes alone, and let the caller decide.
 */
unsigned int   geBE32(const void *p);
unsigned short geBE16(const void *p);

#ifdef __cplusplus
}
#endif

#endif /* GE_SWAP_UTIL_H */

/*
 * ALWaveTable is hand-written, not generated, and the reason is the union.
 *
 *     union { ALADPCMWaveInfo adpcmWave;   two 4-byte pointers
 *             ALRAWWaveInfo   rawWave;  }  one
 *
 * Which member is live is decided by `type`, a sibling field -- so there is no
 * correct swap that can be derived from the union alone, which is exactly why
 * gen_struct_swap.py refuses to guess and emits an undefined hook instead. The
 * policy needs the parent struct, so the whole swapper lives here where it can
 * read `type` before deciding.
 */
void geSwap_ALWaveTable(void *p);

/*
 * UNION POLICIES.
 *
 * gen_struct_swap.py now proves most unions unambiguous by computation: if
 * every arm produces the identical swap, it emits it directly. 17 hooks
 * collapsed to 4 that way, and what is left are real decisions.
 */
void geSwapUnion_ModelRoData_HeaderRecord__anon__at8(void *p);
void geSwapUnion_ModelRoData_Op11Record__anon__at0(void *p);
void geSwapUnion_Vertex__anon__at8(void *p);
void geSwapUnion_Vertex__anon__at12(void *p);

/*
 * Vertex has two readings and the file does not say which. A DISPLAY vertex
 * carries texture coordinates and a colour/normal; a COLLISION vertex carries a
 * node pointer and an index. Only the owning node's opcode knows.
 *
 * The hooks above implement the DISPLAY reading, because that is what rendering
 * needs and it is the overwhelmingly common case. Call this instead for
 * vertices reached through a collision record.
 */
void geSwap_VertexCollision(void *p);

/*
 * A whole font: 13x13 s32 kerning table followed by 94 fontchar records.
 * Hand-written rather than generated because `struct font` embeds a 94-element
 * array, and the generator unrolls arrays -- 733 inline swap calls for a loop.
 */
void geSwap_font(void *p);

/* One image-table entry from a model file: swaps `index` only. See the .c. */
void geSwap_sImageTableEntry(void *p);

/*
 * A level background file, converted in place after it is DMA'd from the
 * cartridge. geSwapBgHeader does the section table alone, for the 0x40-byte
 * probe read the loader does first; geSwapBgFile does the whole thing. See the
 * .c file for why this is not a blanket u32 swap.
 */
void geSwapBgHeader(void *base);
void geSwapBgFile(void *base);

/* A decompressed room point table: a run of 16-byte Vtx. See the .c file for
   why this is not a u32 swap -- the trailing colour bytes must survive. */
void geSwapVtxRun(void *p, unsigned int size);

/*
 * The level setup file, converted list by list before proplvreset2 relocates
 * it. Each takes the head of one list and stops at that list's own terminator;
 * see the .c file for the terminator each one uses.
 */
void geSwapIdListMinus1(void *p);
void geSwapSetupWaypoints(void *p);
void geSwapSetupWaygroups(void *p);
void geSwapSetupAiLists(void *p);
void geSwapSetupPaths(void *p);
void geSwapSetupPads(void *p);
void geSwapSetupBoundPads(void *p);

/*
 * Swap a ROM segment that is entirely Gfx commands (u32 pairs), in place.
 * See ge_swap_util.c for why this is safe for font_dl and wrong for the room
 * and model segments. Call exactly once -- it is not idempotent.
 */
void geSwapGfxBlob(void *p, unsigned int size);

