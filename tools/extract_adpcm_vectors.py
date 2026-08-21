#!/usr/bin/env python3
"""Extract bit-exact VADPCM test vectors from a GoldenEye instrument bank.

Feeds tools/audio_validate.cpp.

WHY THIS EXISTS
---------------
The audio command interpreter's A_ADPCM was implemented from the documented
VADPCM algorithm, and the unit tests only check structure — right sample count,
silence stays silent, non-zero residuals give non-zero output. None of that
catches a wrong predictor formulation, which produces confident-looking noise.

The bank file contains an oracle. `ALADPCMloop` (include/PR/libaudio.h:177) is:

    typedef struct {
        u32         start;      /* loop start, in SAMPLES  */
        u32         end;
        u32         count;
        ADPCM_STATE state;      /* s16[16] — see abi.h:245 */
    } ALADPCMloop;

`state` is the sixteen decoded samples immediately preceding `start`, stored so
a looping voice can resume without recomputing. Nintendo's encoder wrote those
values with the real encoder; the real microcode reproduces them.

So: decode from the beginning of the sample up to `start`, and the last sixteen
samples MUST equal `state`, sample for sample. That is ground truth from the
ROM, not a plausibility check.

USAGE
    python3 tools/extract_adpcm_vectors.py <decomp-root> <out.bin>
    ./build/audio_validate <out.bin>

The output contains only decoder inputs and expected outputs — codebook
coefficients and a few hundred bytes of compressed samples per case. It is a
test fixture, generated locally from your own ROM; do not redistribute it.

FILE FORMAT (little-endian, host order)
    magic   'GEAV'  u32
    count          u32
    per case:
        order        u32
        npredictors  u32
        book_count   u32          (order * npredictors * 8 entries)
        book         s16[book_count]
        loop_start   u32          (in samples)
        expected     s16[16]
        data_bytes   u32
        data         u8[data_bytes]   (VADPCM, 9-byte frames)
"""

import os
import struct
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    repo, outpath = sys.argv[1], sys.argv[2]

    ctl_path = os.path.join(repo, 'assets/music/instruments.ctl')
    tbl_path = os.path.join(repo, 'assets/music/instruments.tbl')
    if not os.path.exists(ctl_path):
        print(f'missing {ctl_path} — run the decomp asset extraction first',
              file=sys.stderr)
        return 2

    ctl = open(ctl_path, 'rb').read()
    tbl = open(tbl_path, 'rb').read()

    def s16(o): return struct.unpack_from('>h', ctl, o)[0]
    def s32(o): return struct.unpack_from('>i', ctl, o)[0]
    def u32(o): return struct.unpack_from('>I', ctl, o)[0]

    if (s16(0) & 0xFFFF) != 0x4231:
        print('not an ALBankFile (bad revision)', file=sys.stderr)
        return 2

    cases = []
    seen = set()
    nbanks = s16(2)

    for bi in range(nbanks):
        b = s32(4 + 4 * bi)
        if b == 0:
            continue
        inst_count = s16(b)
        for ii in range(inst_count):
            io = s32(b + 12 + 4 * ii)
            if io == 0:
                continue
            sound_count = s16(io + 14)
            for si in range(sound_count):
                so = s32(io + 16 + 4 * si)
                if so == 0:
                    continue
                wt = s32(so + 8)
                if wt == 0:
                    continue
                base, length = s32(wt), s32(wt + 4)
                wtype = ctl[wt + 8]
                loop, book = s32(wt + 12), s32(wt + 16)
                # Only ADPCM waves that actually carry a loop have the oracle.
                if wtype != 0 or loop == 0 or book == 0:
                    continue

                order, npred = s32(book), s32(book + 4)
                n = order * npred * 8
                coeffs = [s16(book + 8 + 2 * k) for k in range(n)]

                loop_start = u32(loop)
                if loop_start < 16:
                    continue                     # nothing preceding to compare
                expected = [s16(loop + 12 + 2 * k) for k in range(16)]
                if all(v == 0 for v in expected):
                    continue                     # a silent state proves nothing

                # WHICH SIXTEEN SAMPLES ARE THESE?
                #
                # This originally assumed state == the sixteen samples
                # IMMEDIATELY BEFORE `start`, and reported 0/4 for a decoder
                # that turned out to be correct. The assumption was argued, not
                # measured, and it was wrong.
                #
                # Measured instead: decode a whole wave and search the output
                # for the expected sequence. It is there every time, at
                # (start // 16) * 16 -- the frame CONTAINING the loop point,
                # not the frame before it. That is what a resume seed has to be:
                # the decoder restarts on a frame boundary at or below `start`
                # and needs that frame's samples as its history.
                #
                #   start=3952 -> found at 3952   (start-16 would be 3936)
                #   start=5742 -> found at 5728   (start-16 would be 5726)
                #   start=6917 -> found at 6912   (start-16 would be 6901)
                #
                # A non-frame-aligned start is fine under this reading, so the
                # old skip is gone and more waves qualify as vectors.
                frames_needed = loop_start // 16 + 1
                bytes_needed = frames_needed * 9
                if base + bytes_needed > len(tbl) or bytes_needed > length:
                    continue

                key = (base, loop_start)
                if key in seen:
                    continue
                seen.add(key)

                cases.append({
                    'order': order,
                    'npred': npred,
                    'book': coeffs,
                    'loop_start': loop_start,
                    'expected': expected,
                    'data': tbl[base:base + bytes_needed],
                })

    if not cases:
        print('no looped ADPCM waves found', file=sys.stderr)
        return 1

    with open(outpath, 'wb') as f:
        f.write(b'GEAV')
        f.write(struct.pack('<I', len(cases)))
        for c in cases:
            f.write(struct.pack('<III', c['order'], c['npred'], len(c['book'])))
            f.write(struct.pack(f'<{len(c["book"])}h', *c['book']))
            f.write(struct.pack('<I', c['loop_start']))
            f.write(struct.pack('<16h', *c['expected']))
            f.write(struct.pack('<I', len(c['data'])))
            f.write(c['data'])

    total = sum(len(c['data']) for c in cases)
    print(f'{len(cases)} test vectors, {total} bytes of VADPCM')
    print(f'loop starts: min={min(c["loop_start"] for c in cases)} '
          f'max={max(c["loop_start"] for c in cases)}')
    print(f'wrote {outpath}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
