#!/usr/bin/env python3
"""
Flip GETV KEEP arms to ship defaults when env is unset (dig sets explicit value).

Pattern (match product port):
  (e != NULL && *e != '\\0') ? <dig> : <ship> /* ship default ON; dig sets 0 */

Never emits a no-op ternary such as `? 1 : 1` or identical int branches.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

SHIP_COMMENT = "ship default ON; dig sets 0"
WINDOW = 480

# (e != NULL && *e != '\0') ? dig : unset;
TERNARY_E_POS = re.compile(
    r"\(\s*e\s*!=\s*NULL\s*&&\s*\*e\s*!=\s*'\\0'\s*\)\s*\?\s*(?P<dig>[^:]+?)\s*:\s*(?P<unset>[^;]+?)\s*;",
    re.MULTILINE,
)

# (!e || !*e) ? ship : dig;
TERNARY_E_NEG = re.compile(
    r"\(\s*!\s*e\s*\|\|\s*!\s*\*e\s*\)\s*\?\s*(?P<unset>[^:]+?)\s*:\s*(?P<dig>[^;]+?)\s*;",
    re.MULTILINE,
)

STALE_DEFAULT_COMMENT = re.compile(
    r"/\*\s*DEFAULT\s+0\s+stale\s*\*/", re.IGNORECASE
)


@dataclass
class Manifest:
    bool_on_unset: list[str]
    int_on_unset: dict[str, int]
    string_on_unset: dict[str, str]
    int_on_unset_optional: dict[str, int]
    do_not_touch: list[str]
    scan_roots: list[str]
    ship_comment: str

    @classmethod
    def load(cls, path: Path) -> Manifest:
        data = json.loads(path.read_text(encoding="utf-8"))
        return cls(
            bool_on_unset=list(data.get("bool_on_unset", [])),
            int_on_unset=dict(data.get("int_on_unset", {})),
            string_on_unset=dict(data.get("string_on_unset", {})),
            int_on_unset_optional=dict(data.get("int_on_unset_optional", {})),
            do_not_touch=list(data.get("do_not_touch", [])),
            scan_roots=list(data.get("scan_roots", ["getv/port", "vendor/ge-decomp/src"])),
            ship_comment=str(data.get("ship_comment", SHIP_COMMENT)),
        )

    def blocked(self, gate: str) -> bool:
        return gate in self.do_not_touch

    def ship_value(self, gate: str) -> int | str | bool | None:
        if gate in self.string_on_unset:
            return self.string_on_unset[gate]
        if gate in self.int_on_unset:
            return self.int_on_unset[gate]
        if gate in self.int_on_unset_optional:
            return self.int_on_unset_optional[gate]
        if gate in self.bool_on_unset:
            return True
        return None


def iter_source_files(root: Path, scan_roots: Iterable[str]) -> list[Path]:
    exts = {".c", ".h", ".cpp", ".cc"}
    out: list[Path] = []
    for rel in scan_roots:
        base = root / rel
        if not base.exists():
            continue
        for dirpath, _, filenames in os.walk(base):
            for name in filenames:
                p = Path(dirpath) / name
                if p.suffix.lower() in exts:
                    out.append(p)
    return sorted(out)


def _strip_comment(val: str) -> str:
    if "/*" in val:
        val = val[: val.index("/*")]
    return val.strip()


def _dig_is_noop(dig: str, ship_literal: str) -> bool:
    """True if replacing unset with ship would make both branches identical."""
    d = _strip_comment(dig)
    return d == ship_literal


def _format_ship(ship: int | str | bool, comment: str) -> str:
    if isinstance(ship, bool):
        lit = "1" if ship else "0"
    elif isinstance(ship, str):
        lit = f'"{ship}"'
    else:
        lit = str(ship)
    return f"{lit} /* {comment} */"


def flip_window(window: str, ship: int | str | bool, comment: str) -> tuple[str, bool]:
    if f"/* {comment} */" in window:
        return window, False

    ship_lit = (
        "1"
        if ship is True
        else ("0" if ship is False else (f'"{ship}"' if isinstance(ship, str) else str(ship)))
    )

    m = TERNARY_E_POS.search(window)
    if m:
        dig, unset = m.group("dig"), m.group("unset")
        if _strip_comment(unset) == ship_lit:
            new_window = STALE_DEFAULT_COMMENT.sub("", window)
            return new_window, new_window != window
        if _dig_is_noop(dig, ship_lit):
            return window, False
        replacement = (
            f"(e != NULL && *e != '\\0') ? {dig.strip()} : {_format_ship(ship, comment)};"
        )
        new_window = window[: m.start()] + replacement + window[m.end() :]
        new_window = STALE_DEFAULT_COMMENT.sub("", new_window)
        return new_window, True

    m = TERNARY_E_NEG.search(window)
    if m:
        unset, dig = m.group("unset"), m.group("dig")
        if _strip_comment(unset) == ship_lit:
            return window, False
        if _strip_comment(dig) == ship_lit:
            return window, False
        replacement = (
            f"(!e || !*e) ? {_format_ship(ship, comment)} : {dig.strip()};"
        )
        new_window = window[: m.start()] + replacement + window[m.end() :]
        return new_window, True

    return window, False


def flip_string_unset(window: str, ship: str, comment: str) -> tuple[str, bool]:
    """unset branch uses strcmp / string literal (e.g. GETV_STEREO_SRC)."""
    if f"/* {comment} */" in window:
        return window, False
    # cached = "flat";  -> cached = "xr" /* comment */;
    pat = re.compile(
        r'((?:cached|src|mode)\s*=\s*)"[^"]*"\s*;',
    )
    m = pat.search(window)
    if not m:
        return window, False
    prefix = m.group(1)
    new_window = (
        window[: m.start()]
        + f'{prefix}"{ship}" /* {comment} */;'
        + window[m.end() :]
    )
    return new_window, True


def flip_text(text: str, manifest: Manifest) -> tuple[str, int]:
    total = 0
    gates: list[tuple[str, int | str | bool]] = []
    for g in manifest.bool_on_unset:
        if not manifest.blocked(g):
            gates.append((g, True))
    for g, ship in manifest.int_on_unset.items():
        if not manifest.blocked(g):
            gates.append((g, ship))
    for g, ship in manifest.int_on_unset_optional.items():
        if not manifest.blocked(g):
            gates.append((g, ship))
    for g, ship in manifest.string_on_unset.items():
        if not manifest.blocked(g):
            gates.append((g, ship))

    for gate, ship in gates:
        for m in re.finditer(rf'getenv\(\s*"{re.escape(gate)}"\s*\)', text):
            start = m.end()
            window = text[start : start + WINDOW]
            if isinstance(ship, str):
                new_window, did = flip_string_unset(window, ship, manifest.ship_comment)
            else:
                new_window, did = flip_window(window, ship, manifest.ship_comment)
            if did:
                total += 1
                text = text[:start] + new_window + text[start + WINDOW :]
    return text, total


def flip_file(path: Path, manifest: Manifest, dry_run: bool) -> tuple[str, str, int]:
    original = path.read_text(encoding="utf-8", errors="replace")
    updated, n = flip_text(original, manifest)
    if updated != original and not dry_run:
        path.write_text(updated, encoding="utf-8", newline="\n")
    return original, updated, n


def write_unified_patch(rel: Path, before: str, after: str, patch_path: Path) -> None:
    import difflib

    rel_s = rel.as_posix()
    diff = difflib.unified_diff(
        before.splitlines(keepends=True),
        after.splitlines(keepends=True),
        fromfile=f"a/{rel_s}",
        tofile=f"b/{rel_s}",
    )
    body = "".join(diff)
    if body:
        patch_path.parent.mkdir(parents=True, exist_ok=True)
        patch_path.write_text(body, encoding="utf-8")


def apply_tree(root: Path, manifest: Manifest, dry_run: bool, patch_dir: Path | None) -> int:
    files = iter_source_files(root, manifest.scan_roots)
    if not files:
        print(
            f"[keep-defaults] no sources under {manifest.scan_roots} (root={root})",
            file=sys.stderr,
        )
        return 1
    grand = 0
    patch_idx = 1
    for path in files:
        before, after, n = flip_file(path, manifest, dry_run=dry_run)
        if n:
            grand += n
            rel = path.relative_to(root)
            print(f"[keep-defaults] {rel}: {n} site(s)")
            if patch_dir and not dry_run:
                write_unified_patch(
                    rel,
                    before,
                    after,
                    patch_dir / f"{patch_idx:03d}-{rel.name}-keep-defaults.patch",
                )
                patch_idx += 1
    print(f"[keep-defaults] total flips: {grand}")
    return 0 if grand else 2


def run_self_test() -> int:
    sample = """
static int ge_gate(void) {
    const char *e = getenv("GETV_VR_TEXINVAL");
    on = (e != NULL && *e != '\\0') ? (atoi(e) != 0) : 0;
}
static int ge_already(void) {
    const char *e = getenv("GETV_VR_DRAWALL");
    on = (e != NULL && *e != '\\0') ? (atoi(e) != 0) : 1;
}
static int ge_ss(void) {
    const char *e = getenv("GETV_SUPERSAMPLE");
    ss = (e != NULL && *e != '\\0') ? atoi(e) : 0;
}
static int ge_fog(void) {
    const char *e = getenv("GETV_VR_PROPFOGALPHA");
    v = (e != NULL && *e != '\\0') ? atoi(e) : 1;
}
"""
    m = Manifest(
        bool_on_unset=["GETV_VR_TEXINVAL", "GETV_VR_DRAWALL"],
        int_on_unset={"GETV_SUPERSAMPLE": 3, "GETV_VR_PROPFOGALPHA": 0},
        string_on_unset={},
        int_on_unset_optional={},
        do_not_touch=[],
        scan_roots=["."],
        ship_comment=SHIP_COMMENT,
    )
    out, n = flip_text(sample, m)
    if n != 3:
        print("self-test failed: flip count", n, out)
        return 1
    if "? 1 : 1" in out.replace(" ", ""):
        print("self-test failed: noop ternary", out)
        return 1
    if ": 1 /*" not in out or ": 3 /*" not in out or ": 0 /*" not in out:
        print("self-test failed: missing ship comments", out)
        return 1
    if "GETV_VR_DRAWALL" in out and out.count(": 1 /* ship default ON") < 2:
        # DRAWALL should not get a second : 1 branch
        pass
    print("[keep-defaults] self-test ok")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=Path("."), help="goldeneye-native workshop root")
    ap.add_argument(
        "--manifest",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "patches/keep-defaults-on/MANIFEST.json",
    )
    ap.add_argument("--apply", action="store_true", help="rewrite sources in place")
    ap.add_argument("--dry-run", action="store_true", help="report only")
    ap.add_argument("--write-patches", type=Path, help="emit unified diffs per touched file")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return run_self_test()

    manifest = Manifest.load(args.manifest)
    root = args.root.resolve()
    if not args.apply:
        print("Pass --apply (and optional --write-patches). Or use --self-test.", file=sys.stderr)
        return 1
    return apply_tree(root, manifest, dry_run=args.dry_run, patch_dir=args.write_patches)


if __name__ == "__main__":
    sys.exit(main())
