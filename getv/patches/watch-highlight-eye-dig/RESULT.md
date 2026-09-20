# RESULT — watch menu highlight left-eye only (GEVR #58)

**Status:** DIG ONLY. No C landed. **APPLY READY sketch** below (workshop `options.c`).
**Ask:** why the watch **highlight** draws in the LEFT eye only (and flashes) while **words stay visible in both eyes**. Smallest falsifier knob, **default OFF**.
**Build:** public Latest **vr442** / cook 452. Harvest `docs/NOTE-WATCH-PER-EYE-HIGHLIGHT-MISS-20260918.md` (GEVR).
**Not this dig:** #32 confirm/select (Quit / face-button latch). #57 arm-watch panel. Text-glued-across-both-eyes stereo debt.
**Date:** 2026-09-20.
**Evidence:** public `n64decomp/007` `src/game/options.c` + `textrelated.c`; public GEVR textbook / packaging / issue #58; this repo `bondviewRenderWatch` call site (`patches/decomp-host-port.patch`). Workshop stereo eye loop (`lv.c` / `stereo.c`) is **not** on a public remote (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols already in the public textbook.

Director can green-light `GETV_WATCHHL`, park it, or demand the HUDGATE second sit from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Wear-only on vr442? | **No.** vr442 notes do not claim highlight-in-both-eyes. Issue still OPEN. |
| Separate fill / scissor / one-eye HUD blit? | **No.** Highlight is **`textRenderOutlined`** (extra glyph texrects). Words are **`textRender`**. Same XY. |
| Why left-only + flash? | `draw_watch_current_page` runs **per eye**. A/Z **inside that draw** calls `watch_play_beep_sound()`, which **toggles** `watch_item_is_actively_selected`. Eye 0 draws outlined. Eye 1 toggles it off and draws plain words. |
| HUDGATE / scissor / menu-capture / one-eye DL? | **Ranked out as first cause.** Same glyph path as the words that already land in both eyes. Outline is not a fillrect. |
| Smallest falsifier | **`GETV_WATCHHL` default OFF.** Force the outlined look on the **current line** for every eye. Do **not** take `draw_toggle_option_values` state 2 (`game_option_toggle_input`) — that is #32. |
| Do not re-chair | **`GETV_STEREO=0`.** Owner F0 2026-09-18: FAIL / blocked (unreadable). |
| Not this knob | `GETV_WATCHEYE_ONCE` (first-eye A gate) is the #32 latch. Leave it off this page. |
| APPLY tonight? | **Sketch is APPLY READY on the workshop.** Public tree has no product `options.c`. Nothing landed here. |

```
eye 0  A this frame → watch_play_beep_sound() → selected=1 → textRenderOutlined  (LEFT highlight)
eye 1  A still this frame → watch_play_beep_sound() → selected=0 → textRender     (RIGHT words only)
end of frame: selected=0  → flash; next frames neither eye outlined
```

---

## 1. Scope split (do not collapse)

| Ticket | Symptom | This dig |
|--------|---------|----------|
| **#58** | Highlight **pixels** missing in the right eye; words both eyes; flash on A | **Yes.** Primitive choice + per-eye flag. |
| **#32** | Face-button **confirm** rough; Quit never reaches Yes/No | **No.** Same flag, different consumer (`game_option_toggle_input` in state 2). |
| **#57** | Arm-attached / farther watch panel | **No.** `NOTE-ARM-WATCH-PAUSE-PANEL-DIG-20260918.md`. |
| Text glued across both eyes | Stereo HUD depth / cant | **No.** Harvest already excludes it. |

A prior owner comment on #58 treated #58 and #32 as the same toggle. The **toggle is shared**. The **draw path is not** the confirm path. This RESULT names the highlight primitive and a highlight-only knob.

---

## 2. Per-eye draw path (files + functions)

### 2.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `n64decomp/007` | `src/game/options.c`, `textrelated.c` | Highlight is colour + `textRenderOutlined`. A/Z is handled **inside the draw**. |
| This repo | `patches/decomp-host-port.patch` (`bondviewRenderWatch`) | `draw_watch_current_page(gdl, finalmtx, watch_animation_state == 5 \|\| == 12)`. |
| Public GEVR | `docs/NOTE-WATCH-PER-EYE-HIGHLIGHT-MISS-20260918.md`, `docs/292`, `docs/293`, issue #58 | Eye loop at `lvlRender`. Sim gated `geStereoIsFirstEye()`. Chair: left-only flash. F0 STEREO=0 FAIL. |
| Public GEVR packaging | `KEEP-DEFAULTS-INVENTORY-vr441.md`, `gevr-vr441-boot.cmd` | `GETV_STEREO_HUDGATE=1` KEEP_SHIP. No `WATCHHL` / `WATCHEYE` reader in the public boot. |
| Workshop | `vendor/ge-decomp/src/game/lv.c`, `stereo.c`, product `options.c` | Eye loop wraps player render (watch included). **Do not push.** |

### 2.2 Call chain (one walk)

```
lvlRender                          workshop lv.c  — eye loop (STEREO=1 on headset)
  └─ player render body            not first-eye gated (sim IS)
       └─ bondviewRenderWatch      decomp bondview.c
            └─ draw_watch_current_page          options.c
                 ├─ A/Z pressed this frame?     (not inventory page)
                 │    watch_play_beep_sound()   TOGGLE selected + beep
                 ├─ set_page_rectangle_colors   page-tab vertex greens
                 └─ page drawer
                      ├─ draw_watch_game_options_page
                      │    MUSIC / FX labels
                      │    draw_toggle_options → draw_options_labels
                      │                        → draw_toggle_option_values
                      └─ draw_watch_control_options_page
                           STYLE / INPUTS labels
```

`geStereoIsFirstEye()` protects **`propsTick` / sim**, not this draw. Watch input in `draw_watch_current_page` therefore runs **twice per frame** when stereo is on.

### 2.3 The highlight primitive (not a bar)

Public `options.c` (game-options + control-options + toggle list):

| State | Label colour | Drawer |
|-------|----------------|--------|
| Idle other lines | `0xFF00B0` | `textRender` |
| Current line, not selected | `0xA0FFA0F0` | `textRender` |
| Current line, **selected** | `-1` (white) + outline `0x7000A0` | **`textRenderOutlined`** |

`draw_options_labels(..., outlined=1)` is the same split: `textRender` vs `textRenderOutlined`.

`textrelated.c` `textRenderOutlined` → `textRenderGlyphOutlined`: **extra glyph texrects** in the outline colour. Not `gDPFillRectangle`. Words and highlight are the same texrect family. That is why words can be fine in both eyes while the outline is not — the outline commands are only appended when `watch_item_is_actively_selected` is 1 **at that eye's draw**.

`set_page_rectangle_colors` also dims the page-tab verts when selected. Secondary; the chair is the line outline.

### 2.4 The toggle that makes it flash

```c
/* options.c — watch_play_beep_sound */
if (watch_item_is_actively_selected == 1)
    watch_item_is_actively_selected = 0;
else {
    watch_item_is_actively_selected = 1;
    sndPlaySfx(..., CAMERA_BEEP1_SFX, ...);
}
```

Called from `draw_watch_current_page` when `watch_transitioning` and A or Z this frame (inventory excluded). Retail ran this **once per frame**. Stereo runs the whole watch draw per eye, so A is on+beep then off before the right eye appends its DL.

Chair match:

- Highlight **flashes** (selected is 0 again at frame end).
- **Left** (eye 0) saw selected=1.
- **Right** (eye 1) saw selected=0 → plain `textRender` (words only).
- Owner F0: “A on options felt like on/off toggle.”

Inventory is excluded from that A/Z toggle. #58 chair is options / controls / status / briefing, not the inv list.

### 2.5 Harvest candidates (asked 2026-09-18)

| Candidate | Rank | Why |
|-----------|------|-----|
| **Per-eye selected flip** | **First** | Toggle inside per-eye draw. Explains flash + left-only + words-both. |
| Scissor | Out | Same glyph XY as the words that land in both eyes. Outline is +1 px, not a half-FB miss. |
| `GETV_STEREO_HUDGATE` | **Second sit only** | KEEP ON. Public tree has the getenv, not the gate body. Outline is texrect, not fill. If `WATCHHL=1` is still left-only, then sit `HUDGATE=0` (do not ship-change KEEP). |
| Menu capture / `TEXDLRETAG` | Out as first | A last-eye capture would copy the **unoutlined** DL to both eyes. Chair has highlight on the **left**. |
| One-eye DL tag | Same as the flip | Eye 0 DL has outline commands; eye 1 does not. Cause is the flag, not a capture tag. |

---

## 3. Knobs — what not to flip, what to add

| Knob | Ship | This dig |
|------|------|----------|
| `GETV_STEREO` | `1` headset / `0` monitor bat | **Do not re-chair `=0`.** F0 FAIL 2026-09-18. |
| `GETV_STEREO_HUDGATE` | KEEP `1` | F1 only if `WATCHHL=1` still misses the right eye. |
| `GETV_STEREO_REBUILD` / `_SRC=xr` | KEEP | Leave. |
| `GETV_WATCHEYE_ONCE` | **does not exist in public boot** | #32 first-eye A gate. **Not this knob.** |
| **`GETV_WATCHHL`** | **new, default OFF** | Force outlined current-line on **every** eye. Highlight-only. |

Unset / empty `GETV_WATCHHL` = retail (bug remains). Explicit `1` = falsifier. Never KEEP-ON this without a chair PASS.

---

## 4. APPLY READY sketch — **NOT LANDED**

Workshop: product `options.c` (decomp twin of public `n64decomp/007` `src/game/options.c`). Helper can live in the same TU or next to other `getenv("GETV_*")` readers.

### 4.1 Reader (default OFF)

```c
/* GETV_WATCHHL — DIG #58. Unset = OFF. Do not KEEP-ON. */
static int ge_watchhl(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_WATCHHL");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}
```

### 4.2 Current-line outline only (no state 2)

Force `textRenderOutlined` / `draw_options_labels(..., outlined=1, outlinecolour=0x7000A0)` on the **current** index when `ge_watchhl()`.

**Do not** enter `draw_toggle_option_values(..., state=2)` under the knob. State 2 calls `game_option_toggle_input` during draw — that is **#32**. Use state **1** (colours only).

Sites (public names; confirm line numbers on the workshop copy):

1. `draw_watch_game_options_page` — MUSIC (`game_options_index == 0`) and FX (`== 1`)
2. `draw_toggle_options` — row `i == game_options_index - 2`
3. `draw_watch_control_options_page` — STYLE / INPUTS
4. Optional: `set_page_rectangle_colors` selected-dim. Not required for the first sit.

Sketch for the toggle list (retail `else` paths unchanged when the knob is off):

```c
/* draw_toggle_options — current row only */
if (i == game_options_index - 2) {
    if (ge_watchhl()) {
        /* outlined look, both eyes; NO state 2 */
        gdl = draw_toggle_option_values(
            draw_options_labels(gdl, XOFFSET_1, y_offset,
                langGet(game_options_entries[i].text[0]),
                -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0),
            y_offset, i, 1);
    } else if (watch_item_is_actively_selected) {
        /* retail selected — leave for #32; do not edit in this APPLY */
        gdl = draw_toggle_option_values(
            draw_options_labels(gdl, XOFFSET_1, y_offset,
                langGet(game_options_entries[i].text[0]),
                -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0),
            y_offset, i, 2);
    } else {
        gdl = draw_toggle_option_values(
            draw_options_labels(gdl, XOFFSET_1, y_offset,
                langGet(game_options_entries[i].text[0]),
                0xA0FFA0F0, 0, -1, 0, 0, 0x3000B0, 0),
            y_offset, i, 1);
    }
}
```

MUSIC / FX / control labels: `if ((watch_item_is_actively_selected || ge_watchhl()) && <this is the current index>) textRenderOutlined; else textRender`.

No boot.cmd allowlist until a sit PASS. No pack smoke. No ship default ON.

### 4.3 Out of scope (do not APPLY with this sketch)

- First-eye gate on `watch_play_beep_sound` / `GETV_WATCHEYE_ONCE` (#32)
- Farther / arm watch panel (#57)
- `GETV_STEREO=0`
- Flipping KEEP `HUDGATE`
- Text-glued stereo overlay

---

## 5. Chair stare (vr442 / Latest)

**Setup:** `Start-GEVR.bat` on the vr442 zip. Recenter both sticks. Open pause watch (Menu / system button; Tab on keyboard). Go to **Game Options** (or Controls). `GETV_WATCHHL` unset on the first look so the bug is still the baseline.

**Run 0 — bug still there (unset):**

| | Tester sentence |
|--|-----------------|
| **0-PASS (bug live)** | “A on a watch line: outline / bright highlight **flashes in my LEFT eye only**. Words stay in **both** eyes.” |
| **0-FAIL (already fixed)** | “Highlight is steady in both eyes on Latest.” Stop. Close #58. Do not APPLY `WATCHHL`. |

**Run H — `GETV_WATCHHL=1` (no rebuild if the APPLY is already in the chair exe):**

| | Tester sentence |
|--|-----------------|
| **H-PASS 1** | “The **current** stick line has the green outline in **both** eyes. I did not need to press A.” |
| **H-PASS 2** | “It **does not flash off** in the right eye.” |
| **H-PASS 3** | “Other lines stay dim. Only the current line is outlined.” |
| **H-PASS 4** | “A / confirm still behaves like tonight (#32). This knob did not suddenly make Quit reach Yes/No.” |
| **H-FAIL → F1** | “Still left-only with `WATCHHL=1`.” Then one sit: `GETV_STEREO_HUDGATE=0` (KEEP stays 1 for everyone else). If that brings the outline to the right eye, HUDGATE is gating outlined texrects. File a new note; do not KEEP-flip from this page. |
| **H-FAIL other** | “Every line outlined.” / “Stereo world broke.” / “Watch text vanished.” Revert the knob. |

**Do not run** `GETV_STEREO=0` as an #58 probe.

---

## 6. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green `WATCHHL`** | Workshop APPLY §4. Sit Run 0 then H on vr442-class exe. Keep default OFF. |
| **Green `WATCHHL` + later #32** | Same APPLY. Separate first-eye A gate (`WATCHEYE_ONCE`) on its own PR. |
| **H-FAIL → HUDGATE** | Do not ship `HUDGATE=0`. Instrument outlined texrect counts per eye. |
| **Already gone on Latest** | Close #58. Delete the knob. |
| **Reject** | Stop. Leave tonight’s left-only flash. |

---

## 7. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Public decomp names are the textbook map. Workshop bodies stay private until release policy flips.
- #32 / #57 files and knobs were not edited.
