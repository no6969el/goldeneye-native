/*
 * ge_swap_prop_unions.c -- union policy for the setup file's propDef records.
 *
 * HAND-WRITTEN. tools/gen_struct_swap.py leaves every union it cannot resolve
 * as an UNDEFINED hook, so an ambiguity becomes a link error rather than a
 * silently wrong swap. These are the 56 hooks the propDef record types brought
 * in. Generated files hold generated code only, so the decisions live here.
 *
 * Three families, and every one of them is settled by evidence rather than by
 * picking an arm:
 *
 *  1. `Mtxf mtx` (64 bytes, at offset 24 in ObjectRecord and everything that
 *     inherits it) and `Mtxf unk84` in CCTVRecord. Mtxf is
 *         union { f32 m[4][4]; s32 unused; };
 *     -- both arms 32-bit, so sixteen word swaps are right whichever the game
 *     reads. Flagged only because the arms differ in LENGTH. These are also in
 *     the runtime region: the record is written by instcalcmatrices at runtime,
 *     not read out of the cartridge.
 *
 *  2. `rgba_u8 shadecol` / `rgba_u8 nextcol`, four bytes each:
 *         union { struct { u8 r, g, b, a; }; u8 rgba[4]; u32 word; };
 *     The game reaches these through the byte arms everywhere it matters --
 *     chr.c:1662-1665, 2810-2813, 3018-3021 -- and both fields are filled in at
 *     runtime by set_color_shading_from_tile (chr.c:1660, 2321, 2481), never
 *     read from the file. Bytes need no conversion, so these are deliberately
 *     EMPTY rather than absent: an empty body is a decision on the record, a
 *     missing one is an oversight.
 *
 *  3. KeyRecord's `union { s8 keyID; u32 keyflags; }`. Not interchangeable --
 *     one byte against one word. The u32 arm is the live one: bondinv.c:764
 *     ORs keyflags into heldkeyflags, chrai.c:2504 and :2517 set and clear bits
 *     in it. keyID is never read. Swapped as a word.
 *
 * If a prop ever appears with its parts in the wrong places, or a key opens the
 * wrong door, family 1 and family 3 respectively are the first things to
 * reconsider -- which is the point of writing the reasoning down next to the
 * code rather than in a commit message.
 */
#include "ge_swap.h"

/* ObjectRecord.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_ObjectRecord_mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* ObjectRecord.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_ObjectRecord_shadecol__anon__at120(void *p)
{
    (void)p;
}

/* ObjectRecord.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_ObjectRecord_nextcol__anon__at124(void *p)
{
    (void)p;
}

/* DoorRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_DoorRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* DoorRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_DoorRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* DoorRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_DoorRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* KeyRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_KeyRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* KeyRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_KeyRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* KeyRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_KeyRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* KeyRecord.<anon>, 4 bytes -- `s8 keyID` or `u32 keyflags`. The u32 arm is the live one:
   bondinv.c:764 and chrai.c:2504/2517 read and write keyflags; keyID is
   never read. Swapped as a word. */
void geSwapUnion_KeyRecord__anon__at128(void *p)
{
    geSwap32((unsigned char *)p);
}

/* TintedGlassRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_TintedGlassRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* TintedGlassRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_TintedGlassRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* TintedGlassRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_TintedGlassRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* CCTVRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_CCTVRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* CCTVRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_CCTVRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* CCTVRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_CCTVRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* CCTVRecord.unk84, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_CCTVRecord_unk84_at132(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* WeaponObjRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_WeaponObjRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* WeaponObjRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_WeaponObjRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* WeaponObjRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_WeaponObjRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* MonitorObjRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_MonitorObjRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* MonitorObjRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_MonitorObjRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* MonitorObjRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_MonitorObjRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* MultiMonitorObjRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_MultiMonitorObjRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* MultiMonitorObjRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_MultiMonitorObjRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* MultiMonitorObjRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_MultiMonitorObjRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* AutogunRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_AutogunRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* AutogunRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_AutogunRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* AutogunRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_AutogunRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* MultiAmmoCrateRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_MultiAmmoCrateRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* MultiAmmoCrateRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_MultiAmmoCrateRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* MultiAmmoCrateRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_MultiAmmoCrateRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* BodyArmourRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_BodyArmourRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* BodyArmourRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_BodyArmourRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* BodyArmourRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_BodyArmourRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* VehichleRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_VehichleRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* VehichleRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_VehichleRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* VehichleRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_VehichleRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* AircraftRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_AircraftRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* AircraftRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_AircraftRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* AircraftRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_AircraftRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* TankRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_TankRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* TankRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_TankRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* TankRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_TankRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* AmmoCrateRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_AmmoCrateRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* AmmoCrateRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_AmmoCrateRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* AmmoCrateRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_AmmoCrateRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* HatRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_HatRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* HatRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_HatRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* HatRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_HatRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* GlassRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_GlassRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* GlassRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_GlassRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* GlassRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_GlassRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* SafeRecord.<anon>.mtx, 64 bytes -- Mtxf: `f32 m[4][4]` or `s32 unused`. Both arms are 32-bit, so
   sixteen word swaps are correct either way. Runtime region. */
void geSwapUnion_SafeRecord__anon__mtx_at24(void *p)
{
    int i;
    for (i = 0; i < 16; ++i) geSwap32((unsigned char *)p + i * 4);
}

/* SafeRecord.<anon>.shadecol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_SafeRecord__anon__shadecol__anon__at120(void *p)
{
    (void)p;
}

/* SafeRecord.<anon>.nextcol.<anon>, 4 bytes -- rgba_u8: four bytes, `u8 rgba[4]`, or `u32 word`.
   Deliberately empty; see the header for why the byte reading wins. */
void geSwapUnion_SafeRecord__anon__nextcol__anon__at124(void *p)
{
    (void)p;
}

/* ---- the setup file's INTRO section --------------------------------------
 *
 * `fs15_16` is a 15.16 fixed-point word with four spellings:
 *
 *     union fs15_16 { f32 fval; s32 ival; s32 full;
 *                     struct { s16 integer : 16; u16 fraction : 16; }; };
 *
 * The generator is right to call this ambiguous: two 16-bit swaps and one
 * 32-bit swap produce different bytes, so the halves arm genuinely disagrees
 * with the other three.
 *
 * Settled by what the game does with these fields rather than by preference.
 * bondviewLoadSetupIntroSection reads the WORD and converts it:
 *
 *     src/game/bondview_r.c:336   unk04.fval = unk04.ival / 100.0f;
 *     src/game/bondview_r.c:337   unk08.fval = unk08.ival / 100.0f;
 *     src/game/bondview_r.c:338   unk0C.fval = unk0C.ival / 100.0f;
 *     src/game/bondview_r.c:339   unk10.fval = unk10.ival / M_U16_MAX_VALUE_F;
 *     src/game/bondview_r.c:340   unk14.fval = unk14.ival / M_U16_MAX_VALUE_F;
 *
 * `integer`/`fraction` are never read anywhere. One word swap each.
 *
 * Note what the division says about the encoding: the cartridge stores these as
 * scaled integers -- centimetres, and a 16-bit turn -- and the game makes
 * floats of them at load. So the `ival` reading is not merely the live one, it
 * is the only one that could be right: `fval` on unconverted bytes would be a
 * denormal.
 */
#define GE_FS15_16_HOOK(name)                                                 \
    void name(void *p) { geSwap32((unsigned char *)p); }

GE_FS15_16_HOOK(geSwapUnion_SetupIntroCamera__anon__unk04_at4)
GE_FS15_16_HOOK(geSwapUnion_SetupIntroCamera__anon__unk08_at8)
GE_FS15_16_HOOK(geSwapUnion_SetupIntroCamera__anon__unk0C_at12)
GE_FS15_16_HOOK(geSwapUnion_SetupIntroCamera_unk10_at16)
GE_FS15_16_HOOK(geSwapUnion_SetupIntroCamera_unk14_at20)

/*
 * The same union again in SetupIntroSwirl: three offsets from Bond in
 * centimetres, a spline scale and a duration. Read through `.fval` after
 * bondviewLoadSetupIntroSection has divided them, exactly as above -- see
 * `intro_swirl` in src/game/bondview_r.c.
 */
GE_FS15_16_HOOK(geSwapUnion_SetupIntroSwirl__anon__unk08_at8)
GE_FS15_16_HOOK(geSwapUnion_SetupIntroSwirl__anon__unk0C_at12)
GE_FS15_16_HOOK(geSwapUnion_SetupIntroSwirl__anon__unk10_at16)
GE_FS15_16_HOOK(geSwapUnion_SetupIntroSwirl__anon__at20)
GE_FS15_16_HOOK(geSwapUnion_SetupIntroSwirl__anon__at24)

#undef GE_FS15_16_HOOK
