# KEEP ship defaults ON (patches-only for public tree)

Graduate vr441 KEEP arms into C: **unset or empty env = ship ON**; **explicit `0` = OFF**
(dig). Same convention as `GETV_INVERTLOOK` in the product port: one ternary, one-line
comment `ship default ON; dig sets 0`.

Public `goldeneye-native` has **no** `getv/port/src`. Apply on the private workshop:

`F:\Projects\GEVR\GoldenEyeVR\goldeneye-native`

(`vendor/ge-decomp` + `getv`).

Reference boot allowlist: `no6969el/GEVR` `packaging/templates/gevr-vr441-boot.cmd`
and `packaging/KEEP-SHIP-DEFAULTS.md`.

---

## Minimum flip list (this bundle)

| Knob | Unset / empty | Explicit 0 |
|------|----------------|------------|
| `GETV_VR_CORPSEKEEP` | ON | OFF |
| `GETV_VR_TEXINVAL` | ON | OFF |
| `GETV_VR_TEXDLRETAG` | ON | OFF |
| `GETV_VR_VFXTMEM` | ON | OFF |
| `GETV_VR_ADSSIGHT` | ON | OFF |
| `GETV_VR_ADSCULL` | ON | OFF |
| `GETV_VR_HITSNAP` | `2` | `0` (and other explicit values honored) |
| `GETV_VR_VTXGUARD` | `64` (KB) | `0` |
| `GETV_STEREO_REBUILD` | ON | OFF |
| `GETV_STEREO_HUDGATE` | ON | OFF |
| `GETV_XR_HEAD_TRANSLATE` | ON | OFF |
| `GETV_XR_PLAY_AUTORECENTER` | ON | OFF |
| `GETV_XR_PLAY_SRCFBO` | ON | OFF |
| `GETV_SUPERSAMPLE` | `3` | honors `0` / `1` / `2` when set |
| `GETV_VR_DRAWALL` | ON | OFF |

Also flipped when the same unset-OFF pattern appears (vr441 boot KEEP, not DIG):

- `GETV_STEREO_SRC` -> `xr`
- `GETV_XR_PLAY`
- `GETV_VR_SKYMESH`
- `GETV_VR_PLAYSPACE` -> `1`
- `GETV_VR_GUNAIM` / `GETV_VR_GUNMOUNT`

**Not changed:** `GETV_AUTOAIM`, floor inject, turn scale, button maps, `FLAT_FORCE`,
`GETV_XR_FOVMATCH`, or DIG_OFF (`GETV_VR_OCCLSKIP` / `FOGSKIP` / `DISTSKIP` /
`WATERTILE`).

---

## Apply steps (workshop)

1. Open a shell at the workshop root (`goldeneye-native` with `getv/` and
   `vendor/ge-decomp/`).

2. Optional dry run:

   ```bash
   python3 getv/tools/flip_keep_ship_defaults.py --root . --apply --dry-run
   ```

3. Apply in place (recommended on SimRig after `git status` is clean):

   ```bash
   python3 getv/tools/flip_keep_ship_defaults.py --root . --apply \
     --write-patches getv/patches/keep-defaults-on/generated
   ```

   Or apply the checked-in unified diffs (if you regenerated them on the same
   revision):

   ```bash
   cd F:/Projects/GEVR/GoldenEyeVR/goldeneye-native
   patch -p1 < path/to/goldeneye-native/getv/patches/keep-defaults-on/001-harness-bool-keep.patch
   patch -p1 < path/to/goldeneye-native/getv/patches/keep-defaults-on/002-harness-int-keep.patch
   ```

   Use `patch -p1` from the workshop root. If a hunk fails, use step 3's script
   on the real tree (it matches typical `getenv` ternary sites).

4. Rebuild VR Windows target (`getv/build_windows.ps1 -Target all -Vr ...` per
   product docs). Regenerate `getv/patches/thirdparty/0001-getv-port-layer.patch`
   if `gfx_pc.c` / `gfx_sdl2.c` changed.

5. Smoke without boot KEEP assigns:

   - Product: `_smoke-keep-nobat.ps1` on staged zip (see GEVR
     `packaging/KEEP-SHIP-DEFAULTS.md`).
   - Launch with **no** `gevr-vr441-boot.cmd` (or a wipe-only stub): corpses,
     explosion texture path, SS3, and XR play arms must still be ON.

6. Commit product + refresh public patches if you re-ran `--write-patches`.

---

## Files in this folder

| File | Role |
|------|------|
| `MANIFEST.json` | Gate list and ship values (source of truth for the script) |
| `001-harness-bool-keep.patch` | Example bool flips (typical ternary shape) |
| `002-harness-int-keep.patch` | Example int flips (`GETV_SUPERSAMPLE`, etc.) |
| `../tools/flip_keep_ship_defaults.py` | Scans `getv/port`, `vendor/ge-decomp/src` |

Run `python3 getv/tools/flip_keep_ship_defaults.py --self-test` after edits.

---

## Pattern (match `GETV_INVERTLOOK` style)

```c
const char *e = getenv("GETV_VR_TEXINVAL");
on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1; /* ship default ON; dig sets 0 */
```

Integer ship values (`GETV_SUPERSAMPLE`, `GETV_VR_VTXGUARD`, `GETV_VR_HITSNAP`):

```c
ss = (e != NULL && *e != '\0') ? atoi(e) : 3; /* ship default ON; dig sets 0 */
```

`GETV_STEREO_SRC`: unset must behave as `xr`; if your tree uses a string compare
instead of a ternary, set the unset branch to choose `xr` (see product `stereo.c` /
play path). The script flags `getenv("GETV_STEREO_SRC")` sites for manual review
when unset handling is not a `: 0` ternary.

---

## BarZ / Mainline Director note

vr440 failed when the public boot **forgot KEEP lines** while exe defaults stayed OFF.
These C defaults decouple ship behaviour from `gevr-*-boot.cmd` so `_smoke-keep-nobat.ps1`
can pass before boot slimming.
