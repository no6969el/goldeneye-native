#!/usr/bin/env python3
"""Extract every room display list from the GoldenEye decomp into a corpus.

Feeds tools/dl_validate.cpp, which runs the interpreter over the result. This
is the difference between "the decoder agrees with a reference decoder on
synthetic input" and "the interpreter survives every display list in the game".

Usage:
    python3 tools/extract_display_lists.py <path-to-n64decomp/007> <out-dir>
    ./build/dl_validate <out-dir>

No ROM is required: the room data lives in the repo as C source
(assets/obseg/bg/*.c), 1172-compressed inside `u32 ..._mapping_binary_N[]`
arrays.

Two format details, both of which cost a debugging session to find:

1. The 1172 container is a 2-byte magic (0x1172) followed by a RAW DEFLATE
   stream — no zlib or gzip header. Python needs wbits=-15.

2. **1172 omits trailing zero bytes.** The game inflates into an already-zeroed
   buffer, so a final G_ENDDL — `B8 00 00 00 00 00 00 00` — is stored as the
   single byte 0xB8. Roughly 1% of lists come out not a multiple of 8 bytes and
   must be zero-padded back to whole commands. Skip this and those lists have no
   terminator, the walker runs off the end into whatever follows, and the
   failure looks like an interpreter bug rather than an extraction bug.
   (The decomp's own tools/1172inflate.sh hints at this: it filters gzip's
   "unexpected end of file" warning rather than fixing it.)
"""

import collections
import glob
import os
import re
import struct
import sys
import zlib

ARRAY_RE = re.compile(
    r'u32 ((?:pri|sec)_mapping_binary_\d+)\[\]\s*=\s*\{(.*?)\};', re.S)
HEX_RE = re.compile(r'0x([0-9A-Fa-f]+)')

OPCODES = {
    0x00: 'G_SPNOOP', 0x01: 'G_MTX', 0x03: 'G_MOVEMEM', 0x04: 'G_VTX',
    0x06: 'G_DL', 0xB1: 'G_TRI4', 0xB6: 'G_CLEARGEOMETRYMODE',
    0xB7: 'G_SETGEOMETRYMODE', 0xB8: 'G_ENDDL', 0xB9: 'G_SETOTHERMODE_L',
    0xBA: 'G_SETOTHERMODE_H', 0xBB: 'G_TEXTURE', 0xBC: 'G_MOVEWORD',
    0xBD: 'G_POPMTX', 0xBE: 'G_CULLDL', 0xBF: 'G_TRI1', 0xC0: 'G_SETTEX',
    0xE7: 'G_RDPPIPESYNC', 0xFB: 'G_SETENVCOLOR', 0xFC: 'G_SETCOMBINE',
    0xFD: 'G_SETTIMG', 0xFF: 'G_SETCIMG',
}


def inflate_1172(raw: bytes) -> bytes | None:
    if len(raw) < 3 or raw[:2] != b'\x11\x72':
        return None
    try:
        d = zlib.decompressobj(-15)          # raw deflate, no header
        out = d.decompress(raw[2:]) + d.flush()
    except zlib.error:
        return None
    if len(out) % 8:                          # see note 2 in the docstring
        out += b'\x00' * (8 - len(out) % 8)
    return out


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    repo, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)

    bg_files = sorted(glob.glob(os.path.join(repo, 'assets/obseg/bg/*.c')))
    if not bg_files:
        print(f'no bg source found under {repo}/assets/obseg/bg/', file=sys.stderr)
        return 2

    hist = collections.Counter()
    lists = total_bytes = 0

    for path in bg_files:
        with open(path, errors='ignore') as f:
            src = f.read()
        stem = os.path.basename(path)[:-2]
        for m in ARRAY_RE.finditer(src):
            name = m.group(1)
            words = [int(x, 16) for x in HEX_RE.findall(m.group(2))]
            raw = b''.join(struct.pack('>I', w) for w in words)
            out = inflate_1172(raw)
            if out is None or len(out) < 8:
                continue
            with open(os.path.join(outdir, f'{stem}__{name}.bin'), 'wb') as g:
                g.write(out)
            # Histogram stops at the first G_ENDDL, matching what the
            # interpreter will actually walk. Counting past it would include
            # padding and disagree with dl_validate for no reason.
            for i in range(0, len(out) - 7, 8):
                op = out[i]
                hist[op] += 1
                if op == 0xB8:
                    break
            lists += 1
            total_bytes += len(out)

    print(f'levels:        {len(bg_files)}')
    print(f'display lists: {lists}')
    print(f'bytes:         {total_bytes}')
    print(f'\nopcode histogram (to first G_ENDDL):')
    for op in sorted(hist):
        name = OPCODES.get(op, '*** UNKNOWN ***')
        print(f'  0x{op:02X}  {hist[op]:8d}  {name}')
    unknown = [op for op in hist if op not in OPCODES]
    if unknown:
        print(f'\nWARNING: {len(unknown)} opcode(s) not in the known map: '
              + ', '.join(f'0x{o:02X}' for o in unknown))
        return 1
    print(f'\ndistinct opcodes: {len(hist)}, all known')
    return 0


if __name__ == '__main__':
    sys.exit(main())
