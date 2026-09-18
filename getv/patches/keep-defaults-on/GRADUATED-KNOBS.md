# Graduated KEEP knobs (C-default when env unset)

Public tree carries reference `getv/port/*` plus `MANIFEST.json`. On the private
workshop, run `getv/tools/flip_keep_ship_defaults.py --root . --apply` against the
full port (same manifest).

## Bool - unset means ON (dig `GETV_*=0`)

- GETV_VR
- GETV_VR_CORPSEKEEP
- GETV_VR_TEXINVAL / GETV_VR_TEXDLRETAG
- GETV_VR_VFXTMEM / GETV_VR_VFXSHIFT
- GETV_TEX16BE / GETV_TEX32BE
- GETV_VR_ADSSIGHT / GETV_VR_ADSCULL
- GETV_STEREO_REBUILD / GETV_STEREO_HUDGATE
- GETV_XR_HEAD_TRANSLATE / GETV_XR_PLAY_AUTORECENTER
- GETV_XR_PLAY_SRCFBO (required with GETV_SUPERSAMPLE=3)
- GETV_VR_DRAWALL / GETV_VR_SKYMESH
- GETV_XR_PLAY / GETV_VR_GUNAIM / GETV_VR_GUNMOUNT

## Int / string - unset means ship value

- GETV_SUPERSAMPLE -> 3
- GETV_VR_VTXGUARD -> 64 (KB)
- GETV_VR_HITSNAP -> 2
- GETV_VR_CORPSEKEEP_MAX -> 48 / CEIL -> 440
- GETV_VR_PLAYSPACE -> 1
- GETV_STEREO_SRC -> xr
- GETV_VR_PROPFOGALPHA -> 0 (opaque props; do not arm GETV_VR_PROPFOGW)
- GETV_VR_OCCLSKIP -> 1 (dam crates #29 path)

## Still bat- or player-assigned (not C-default ON)

- PLAYER_PREF: FPS, turn scale/dead, audio queue, reticle meters, hand melee radii, etc.
- DIG_OFF pins: GETV_AUTOAIM=0, GETV_XR_FLOOR_INJECT=0, trace/census knobs, GETV_RGBA16BE=0.
- FLAT_FORCE: Play-on-monitor.bat (GETV_STEREO=0, GE_VR_XR=0, ...).
- Wiped dig falsifiers: FOGSKIP, DISTSKIP, WATERTILE (remain env-only; do not ship ON).
- FOVMATCH, PROPFOGW, ROM, LAN, public zip (out of this change).
