# DIG — Systemic texture-spray guard (GEVR #70 class)

**Status:** DIG ONLY. **Not APPLY READY** on this public tree. No C landed.

**Lead (texels + scissor):** `GETV_VR_TEXGUARD` (default OFF) — per-eye / once-per-sim tex state so leftover tiles cannot spray across vision (HMD) or sit in a far corner (flat).

**Safety net (verts):** `GETV_VR_SCRAPDROP` (default OFF) — discard NaN / sat / already-converted tris. Layer, not a substitute. Two knobs, not `TEXGUARD=2`.

**Subset:** `GETV_VR_MONINVAL` (default OFF) — skip TEXINVAL on monitor `texSelect` only. Not the class close.

**Write-up:** `RESULT.md`

**Sketch (workshop only):** `gfx_pc_texguard.snippet.c`

Do **not** flip `TEXINVAL` / `TEXDLRETAG` / `VFXTMEM` / `VFXSHIFT` / `TEX16BE` OFF.
Do **not** re-sit `MONFRAME`. Do **not** Dam `MTXGUARD=2`.
`HEAD_TRANSLATE=0` and `SKYMESH=0` stay.
