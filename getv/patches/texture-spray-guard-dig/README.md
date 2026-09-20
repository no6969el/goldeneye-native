# DIG — Systemic texture-spray guard (GEVR #70 class)

**Status:** DIG ONLY. **Not APPLY READY** on this public tree. No C landed.

**Lead:** `GETV_VR_TEXGUARD` (default OFF) — engine-level shared TMEM / `texSelect` / stereo guard so leftover tiles cannot spray across vision (HMD) or sit in a far corner (flat).

**Subset:** `GETV_VR_MONINVAL` (default OFF) — skip TEXINVAL on monitor `texSelect` only. Not the class close.

**Write-up:** `RESULT.md`

**Sketch (workshop only):** `gfx_pc_texguard.snippet.c`

Do **not** flip `TEXINVAL` / `TEXDLRETAG` / `VFXTMEM` / `VFXSHIFT` / `TEX16BE` OFF.
Do **not** re-sit `MONFRAME`. Do **not** Dam `MTXGUARD=2`.
`HEAD_TRANSLATE=0` and `SKYMESH=0` stay.
