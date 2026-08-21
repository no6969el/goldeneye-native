# Patches to `n64decomp/007`

The VR layer is designed so the decomp changes stay small and reviewable. Every
patch below is guarded by `geVrIsActive()`, so a build with VR disabled produces
byte-identical behaviour to the unpatched game.

Line numbers are against `master` as of this writing and will drift.

---

## 1. `src/fr.c` — per-eye projection

**Function:** `viSetupCurrentPlayerView(Gfx *gdl)` (~line 695)
**Why:** `guPerspectiveF` builds a *symmetric* frustum from one `fovy`. HMD eyes
need four independent half-angles, different per eye.

```diff
+#include "ge_vr/ge_vr.h"
+
 Gfx *viSetupCurrentPlayerView(Gfx *gdl)
 {
     if (g_CurrentPlayer != NULL)
     {
         /* ... viewport scale/trans unchanged ... */
     }
 
     gSPViewport(gdl++, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->viewports[g_ViBackIndex]));
 
     g_viProjectionMatrix = dynAllocateMatrix();
-    guPerspectiveF(g_viProjectionMatrixF, &g_viPerspNorm,
-                   g_ViBackData->fovy, g_ViBackData->aspect,
-                   g_ViBackData->znear, g_ViBackData->zfar, 1.0f);
+    if (!geVrBuildProjectionF(g_viProjectionMatrixF, &g_viPerspNorm,
+                              g_ViBackData->znear, g_ViBackData->zfar, 1.0f))
+    {
+        guPerspectiveF(g_viProjectionMatrixF, &g_viPerspNorm,
+                       g_ViBackData->fovy, g_ViBackData->aspect,
+                       g_ViBackData->znear, g_ViBackData->zfar, 1.0f);
+    }
     guMtxF2L(g_viProjectionMatrixF, g_viProjectionMatrix);
```

**Watch out:** `currentPlayerSetProjectionMatrixF()` further down stores this
matrix for game code to read back (billboarding, muzzle flash placement,
world→screen projection). It must hold the **current eye's** matrix during that
eye's pass. If any system needs a single frustum for both eyes, it must ask for
`GE_VR_EYE_UNION` explicitly — see §5.3 of the architecture doc.

Also in this function: `gDPSetColorImage` targets `g_ViBackData->framebuf`. In
VR the target is the eye swapchain image, supplied by
`IGraphicsBackend::beginEyeTarget`. The framebuffer pointer becomes a handle the
backend resolves rather than a physical address.

---

## 2. `src/game/bondview2.c` — per-camera projection

**Site:** ~line 8423

```c
guPerspective(perspmtx, &perspNorm, g_CurrentPlayer->zoominfovy, 1.4005603f, 10.0f, 300.0f, 1.0f);
```

Same substitution as above. Note `zoominfovy` — this is the scope/zoom FOV.

**Design decision required:** in VR you cannot narrow the *display* FOV to
simulate a scope; the headset's FOV is fixed by the hardware and shrinking the
frustum makes the world balloon. The sniper scope must instead become a
**magnified render into a scope quad** attached to the weapon. Until that exists,
clamp `zoominfovy` to the headset FOV and disable zoom.

---

## 3. `src/game/bondview2.c` — head-driven view angles

**Fields:** `vv_theta` (0x0148), `vv_verta` (0x015c) in `src/game/bondview.h`

Applied once per frame, after the game's own input-driven update, before the
render pass:

```c
if (geVrIsActive()) {
    f32 head_theta, head_verta, head_roll;
    geVrGetHeadAngles(&head_theta, &head_verta, &head_roll);

    /* ADDITIVE. The stick-accumulated yaw stays authoritative for "which way is
       the body facing"; the head adds the rest. Making head yaw authoritative
       means the player can never turn past their own neck. */
    g_CurrentPlayer->vv_theta = stick_accumulated_theta + head_theta;
    g_CurrentPlayer->vv_verta = head_verta;

    /* Roll is NOT written here — the N64 camera has no roll term. The bridge
       folds it into the eye view matrix instead. */

    g_CurrentPlayer->vv_costheta = cosf(g_CurrentPlayer->vv_theta);
    g_CurrentPlayer->vv_sintheta = sinf(g_CurrentPlayer->vv_theta);
    g_CurrentPlayer->vv_verta360 = normalise360(g_CurrentPlayer->vv_verta);
}
```

The cached trig fields (`vv_costheta`, `vv_sintheta`, `vv_cosverta`,
`vv_sinverta`) are read by movement and AI code and **must** be refreshed
whenever the angles are written, or the player will walk in a direction other
than the one they are facing.

**Open question (arch doc §12.3):** is the `vv_verta` pitch clamp enforced
downstream, or only at the input stage? If downstream, head pitch beyond the
clamp will be silently discarded and looking up will feel broken.

---

## 4. `src/game/gunfire.c` — hitscan from the controller

This is the patch that makes it a VR shooter rather than a stereo screenshot.

Find the ray construction in the fire path (entry point is
`gunUpdateAndFireBothHands()`, `src/game/gun.h:310`) and replace the
camera-derived origin/direction:

```c
coord3d origin, dir;
if (geVrIsActive() && geVrHandIsTracked(hand)) {
    geVrGetAimRay((GeVrHand)hand, (f32*)&origin, (f32*)&dir);
} else {
    /* original camera-derived ray, unchanged */
}
```

Everything downstream — penetration, `bondwalkItemGetObjectsShootThrough()`,
damage, decals — is untouched.

**Tracking loss must fall back, not fail.** `geVrHandIsTracked()` returning 0
means a controller went out of view; the correct behaviour is to shoot where the
player is looking, not to shoot at the last stale pose (which is usually the
floor).

---

## 5. Weapon aim displacement

**Fields:** `weapon_theta_displacement`, `weapon_verta_displacement`
(`src/game/bondview.h:189-190`)

These already exist for sway and recoil. In VR they carry the controller's
angular offset from the view:

```c
if (geVrIsActive() && geVrHandIsTracked(hand)) {
    geVrGetWeaponDisplacement((GeVrHand)hand,
                              &player->weapon_theta_displacement,
                              &player->weapon_verta_displacement);
}
```

Then **disable** the procedural sway that normally writes these
(`gunSetBondWeaponSway`, `sub_GAME_7F067F58` aim-lock) while VR is active. Sway
on top of real hand tracking feels like the gun is greased.

---

## 6. Weapon model in world space

The first-person weapon is drawn with a view-relative transform. Replace it with
`geVrGetWeaponModelMatrixF()`, which returns a world-space matrix at the
controller's **grip** pose (not the aim pose — aim is the barrel line, used only
for the hitscan ray).

Muzzle flash and shell-eject attach points will need re-authoring: they were
positioned to look right in a fixed screen composition, not on a tracked object
you can hold up to your face.

---

## 7. Recoil → haptics, not view kick

Wherever recoil currently perturbs the view (`thetadie`/`vertadie` at
`bondview.h:840-841` and the recoil path in `gun.c`), route it to
`geVrHaptic()` and to the weapon model instead.

**View kick from an untracked source is one of the reliable ways to make people
sick in VR.** Kick the gun, never the head.

---

## 8. HUD

Tier 1 (ship this first): render the HUD to an offscreen target and composite it
as a head-locked quad at ~2 m. This is a renderer-level change, not a game-code
change — the HUD keeps drawing its `gDPFillRectangle` / `gSPTextureRectangle`
calls into what it thinks is the framebuffer.

Tier 2: diegetic. GoldenEye's watch is already a raisable in-world object and is
the best HUD affordance the game has; `geVrWatchGestureActive()` exists to hook
the physical raise-your-wrist gesture to it.

---

## 9. `src/sched.c` — do not port

The scheduler thread paces the game off VI retrace. In VR, `xrWaitFrame` paces
the game. `src/sched.c` is **replaced**, not emulated. Same for `src/vi.c`'s
retrace message plumbing.

---

## Verification checklist

Before calling any of this done:

1. Build with VR disabled and confirm behaviour is unchanged from the unpatched
   flat-screen port.
2. `ge_vr_tests` green — in particular the symmetric-parity test, which proves
   `geVrBuildProjectionF` reduces exactly to `guPerspectiveF`.
3. Render both eyes with **identical** poses and confirm the two images are
   pixel-identical. Any difference is a per-eye state leak.
4. Screenshot each eye separately and check a known world point projects to the
   expected disparity for the runtime's reported IPD.
5. Stand still, whip your head. Anything that lags is reading a stale pose or
   locating at the wrong time.
