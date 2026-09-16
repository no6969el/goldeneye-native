# GEVR Beta vr440 — Auto-Aim default OFF + XR B/Y = Start

Apply these on the **workshop** `vendor/ge-decomp` and `getv/port/src/port_input.c` trees (not on vanilla `n64decomp/007` alone for `003` — merge `GETV_AUTOAIM` into the existing GEVR `fileLoadSettingsForFolder` after `cur_player_set_autoaim`).

## Decomp (`vendor/ge-decomp`)

From the decomp root:

```bash
patch -p1 < /path/to/goldeneye-native/getv/patches/vr440-beta/001-decomp-options-autoaim-off.patch
patch -p1 < /path/to/goldeneye-native/getv/patches/vr440-beta/002-decomp-file2-default-options.patch
# Insert GETV_AUTOAIM block (see 003) into the GEVR file2.c that already has GETV_CONTROLS / GETV_INVERTLOOK
patch -p1 < /path/to/goldeneye-native/getv/patches/vr440-beta/003-decomp-file2-getv-autoaim.patch
```

Patches use paths `src/game/...`; from `vendor/ge-decomp`, use `patch -p1` with the patch file copied or `-d vendor/ge-decomp`.

## Port (`getv/port/src/port_input.c`)

```bash
patch -p1 < /path/to/goldeneye-native/getv/port/src/port_input_vr440_xr_btn_b.patch
```

Or apply manually: default `geXrActBtnB()` from `GE_XRACT_WEAPON` to `GE_XRACT_START`; env `GETV_XR_BTN_B=weapon` keeps weapon-on-B.

## Defaults after merge

| Setting | Default |
|--------|---------|
| Auto-Aim (options table + new save `DEFAULT_OPTIONS`) | **OFF** |
| `GETV_AUTOAIM` unset | Save / options table wins |
| `GETV_AUTOAIM=0` / `=1` | Force off / on (`[getv] auto-aim forced to …`) |
| XR B / Y (`GETV_XR_BUTTONS=1`) | **Start / pause** (`GE_XRACT_START`) |
| `GETV_XR_BTN_B=weapon` | Weapon on B (vr439) |

Unchanged: A=use/reload, squeeze=aim, fire=trigger, recenter=both thumbstick clicks.
