/*
 * ge_propdef_sizes.h -- the two strides of a level-setup propDef record.
 *
 * A setup file is a run of variable-length, type-tagged records. sizepropdef()
 * (src/game/loadobjectmodel.c) maps the tag to a length in words, and that one
 * number does two jobs on the N64 because there is only one layout: it is how
 * far apart the records sit in the FILE, and how far apart they sit in MEMORY.
 *
 * On the host those are different numbers, and conflating them cost this port
 * two separate crashes.
 *
 *   ObjectRecord is 128 bytes on the cartridge and 144 here -- `prop` and
 *   `model` are four bytes there and eight here. It cannot be pinned with
 *   GE_N64PTR: the object tables in src/game/gobjdata.c are static
 *   initializers, and clang will not put an address-space-cast pointer in one
 *   (12 files, 525 undefined symbols -- see tools/pin_structs.py).
 *
 * So the records are rebuilt at a host stride by geSwapSetupPropDefs() in
 * src/game/prop.c, and there are two questions to answer per type:
 *
 *   FILE   how far to the next record in the setup file.
 *   MEMORY how far apart to place them, and what every walker in the game will
 *          stride by, since they all call sizepropdef().
 *
 * MEMORY has to be at least the host sizeof, or the records OVERLAP. Rare's
 * own constants are exact for the N64 and too small here: PROPDEF_ARMOUR is
 * 0x22 words, which is sizeof(BodyArmourRecord) on the cartridge (136) and 16
 * bytes short of it on the host (152). domakedefaultobj then wrote
 * `shadecol` and `nextcol` -- at host offsets 136 and 140 -- straight over the
 * next record's four-byte header, destroying its type tag. The walk read a
 * 34-record run of PROPDEF_NOTHING out of the middle of a door and handed
 * domakedefaultobj a record starting 132 bytes into its predecessor: obj 256,
 * pad 256, in a level with 178 pads.
 *
 * ONE TABLE, so the two answers cannot drift apart.
 *
 *   X(tag, words, T)      `words` is what sizepropdef() returns on the N64.
 *                         `T` is the struct, or GE_PROPDEF_NOTYPE when
 *                         sizepropdef has no struct for it.
 *
 * When T is known the file stride is GE_N64SIZEOF_T, measured from a 32-bit
 * DWARF build by tools/gen_struct_expand.py, rather than from `words`: those
 * two agree for the types where sizepropdef says `sizeof(T) / 4` and disagree
 * for the hand-written constants, and the FILE is packed at whichever number
 * sizepropdef returned on the N64 -- so `words` is the file stride and
 * GE_N64SIZEOF_T is only a cross-check. Both are kept; geFilePropDefBytes()
 * uses `words`.
 *
 * The rows are a transcription of sizepropdef()'s live branch. It is a
 * transcription because that function is matching-decompiled code that cannot
 * be restructured, and because the file format is what it is -- but the
 * transcription is checked: geSwapSetupPropDefs() walks the list it builds with
 * the game's own sizepropdef() and reports if it does not land exactly on the
 * terminator.
 */
#ifndef GE_PROPDEF_SIZES_H
#define GE_PROPDEF_SIZES_H

#define GE_PROPDEF_NOTYPE 0

/*  tag                                        words   struct                 */
#define GE_PROPDEF_TABLE(X)                                                   \
    X(PROPDEF_GUARD,                              7,   GuardRecord)           \
    X(PROPDEF_DOOR,                              64,   DoorRecord)            \
    X(PROPDEF_DOOR_SCALE,                         2,   GlobalDoorScaleRecord) \
    X(PROPDEF_PROP,                              32,   ObjectRecord)          \
    X(PROPDEF_GLASS,                             32,   ObjectRecord)          \
    X(PROPDEF_TINTED_GLASS,                      37,   TintedGlassRecord)     \
    X(PROPDEF_SAFE,                              32,   ObjectRecord)          \
    X(PROPDEF_GAS_RELEASING,                     32,   ObjectRecord)          \
    X(PROPDEF_KEY,                               33,   KeyRecord)             \
    X(PROPDEF_ALARM,                             32,   ObjectRecord)          \
    X(PROPDEF_CCTV,                            0x3b,   CCTVRecord)            \
    X(PROPDEF_MAGAZINE,                        0x21,   AmmoCrateRecord)       \
    X(PROPDEF_COLLECTABLE,                     0x22,   WeaponObjRecord)       \
    X(PROPDEF_MONITOR,                         0x40,   MonitorObjRecord)      \
    X(PROPDEF_MULTI_MONITOR,                   0x95,   MultiMonitorObjRecord) \
    X(PROPDEF_RACK,                              32,   ObjectRecord)          \
    X(PROPDEF_AUTOGUN,                         0x36,   AutogunRecord)         \
    X(PROPDEF_LINK,                               3,   LinkRecord)            \
    X(PROPDEF_HAT,                               32,   ObjectRecord)          \
    X(PROPDEF_GUARD_ATTRIBUTE,                    3,   GuardAttributeRecord)  \
    X(PROPDEF_SWITCH,                             4,   LinkRecord)            \
    X(PROPDEF_SAFE_ITEM,                          5,   SafeObjectRecord)      \
    X(PROPDEF_AMMO,                            0x2d,   MultiAmmoCrateRecord)  \
    X(PROPDEF_ARMOUR,                          0x22,   BodyArmourRecord)      \
    X(PROPDEF_TAG,                                4,   TagObjectRecord)       \
    X(PROPDEF_RENAME,                            10,   RenameObjectRecord)    \
    X(PROPDEF_LOCK_DOOR,                          4,   LockDoorRecord)        \
    X(PROPDEF_VEHICHLE,                        0x2c,   VehichleRecord)        \
    X(PROPDEF_AIRCRAFT,                        0x2d,   AircraftRecord)        \
    X(PROPDEF_TANK,                            0x38,   TankRecord)            \
    X(PROPDEF_CAMERAPOS,                          7,   CutsceneRecord)

/*
 * The short objective records. sizepropdef() has no struct for these -- it
 * returns a bare count -- and they are runs of 32-bit ids with no pointer and
 * no sub-word field, so the two layouts are identical and one number serves.
 */
#define GE_PROPDEF_TABLE_PLAIN(X)                                             \
    X(PROPDEF_OBJECTIVE_START,                    4)                          \
    X(PROPDEF_OBJECTIVE_END,                      1)                          \
    X(PROPDEF_OBJECTIVE_DESTROY_OBJECT,           2)                          \
    X(PROPDEF_OBJECTIVE_COMPLETE_CONDITION,       2)                          \
    X(PROPDEF_OBJECTIVE_FAIL_CONDITION,           2)                          \
    X(PROPDEF_OBJECTIVE_COLLECT_OBJECT,           2)                          \
    X(PROPDEF_OBJECTIVE_DEPOSIT_OBJECT,           2)                          \
    X(PROPDEF_OBJECTIVE_PHOTOGRAPH,               4)                          \
    X(PROPDEF_OBJECTIVE_NULL,                     1)                          \
    X(PROPDEF_OBJECTIVE_ENTER_ROOM,               4)                          \
    X(PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM,   5)                          \
    X(PROPDEF_OBJECTIVE_COPY_ITEM,                1)                          \
    X(PROPDEF_WATCH_MENU_OBJECTIVE_TEXT,          4)

/*
 * The setup file's INTRO section, which is the same kind of list one section
 * over: type-tagged and variable-length, but with a full s32 tag at offset 0
 * rather than a byte at offset 3.
 *
 * Only `SetupIntroCamera` changes shape between the two layouts -- it carries
 * two `union { integer; char *; }` members and a `prev` pointer, so it is 40
 * bytes on the cartridge and 56 here. Every other intro record is a run of
 * s32 and is the same size on both machines. One record type is enough to
 * require rebuilding the list, because the walker strides by `sizeof`.
 *
 * The file strides are NOT written out here: they are `GE_N64SIZEOF_T` from
 * tools/gen_struct_expand.py, measured rather than transcribed. This table only
 * has to say which struct answers to which tag, and the walker in
 * bondviewLoadSetupIntroSection is the authority for that -- it is the code
 * that does the casting.
 */
#define GE_INTRO_TABLE(X)                                                     \
    X(INTROTYPE_SPAWN,    SetupIntroSpawn)                                    \
    X(INTROTYPE_ITEM,     SetupIntroItem)                                     \
    X(INTROTYPE_AMMO,     SetupIntroAmmo)                                     \
    X(INTROTYPE_SWIRL,    SetupIntroSwirl)                                    \
    X(INTROTYPE_ANIM,     SetupIntroAnim)                                     \
    X(INTROTYPE_CUFF,     SetupIntroCuff)                                     \
    X(INTROTYPE_CAMERA,   SetupIntroCamera)                                   \
    X(INTROTYPE_WATCH,    SetupIntroWatch)                                    \
    X(INTROTYPE_CREDITS,  SetupIntroCredits)

#endif /* GE_PROPDEF_SIZES_H */
