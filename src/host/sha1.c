/*
 * sha1.c — SHA-1, for identifying the user's ROM.
 *
 * Used for exactly one thing: deciding whether the ROM matches the build the
 * segment table in hostcompat/ge_segments.h was extracted from. Not security;
 * identity. A CRC would very nearly do, but the decomp and the wider N64
 * community both identify ROMs by SHA1, so a user can compare the value this
 * prints against a published one without converting anything.
 *
 * RFC 3174, written out plainly. Verified against the known SHA1 of the US ROM
 * rather than trusted.
 */

#include <stdio.h>
#include <string.h>

/*
 * NOTE: no sprintf() below, deliberately.
 *
 * The game defines its own sprintf (src/sprintf.c), strlen, memcpy and friends,
 * and once they are linked into this executable they override the host C
 * library FOR EVERY CALLER -- including port code like this file. The first
 * version of this function formatted its digest with sprintf("%08x") and
 * produced 0078383000783830..., a repeating pattern that looks like a broken
 * hash rather than a broken formatter.
 *
 * The lesson generalises past this file: in a port that links the game's own
 * libc, host code cannot assume the standard library it calls is the standard
 * one. Where the exact behaviour matters, do it by hand.
 */

#include "sha1.h"

#define ROL(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

static void sha1Block(unsigned int h[5], const unsigned char b[64])
{
    unsigned int w[80], a, bb, c, d, e, t, i;

    for (i = 0; i < 16; ++i) {
        w[i] = ((unsigned int)b[i * 4] << 24) | ((unsigned int)b[i * 4 + 1] << 16) |
               ((unsigned int)b[i * 4 + 2] << 8) | (unsigned int)b[i * 4 + 3];
    }
    for (i = 16; i < 80; ++i) {
        w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    a = h[0]; bb = h[1]; c = h[2]; d = h[3]; e = h[4];
    for (i = 0; i < 80; ++i) {
        unsigned int f, k;
        if (i < 20)      { f = (bb & c) | (~bb & d);            k = 0x5A827999u; }
        else if (i < 40) { f = bb ^ c ^ d;                      k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (bb & c) | (bb & d) | (c & d);   k = 0x8F1BBCDCu; }
        else             { f = bb ^ c ^ d;                      k = 0xCA62C1D6u; }
        t = ROL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL(bb, 30); bb = a; a = t;
    }
    h[0] += a; h[1] += bb; h[2] += c; h[3] += d; h[4] += e;
}

int geSha1File(const char *path, char out_hex[41])
{
    unsigned int h[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu,
                          0x10325476u, 0xC3D2E1F0u };
    unsigned char buf[64];
    unsigned long long total = 0;
    size_t n;
    int i;
    FILE *fh = fopen(path, "rb");

    if (fh == NULL) {
        return 0;
    }
    while ((n = fread(buf, 1, 64, fh)) == 64) {
        sha1Block(h, buf);
        total += 64;
    }
    total += n;
    fclose(fh);

    /* Pad: 0x80, zeros, then the length in BITS as a big-endian 64-bit value. */
    buf[n++] = 0x80;
    if (n > 56) {
        memset(buf + n, 0, 64 - n);
        sha1Block(h, buf);
        n = 0;
    }
    memset(buf + n, 0, 56 - n);
    {
        unsigned long long bits = total * 8ull;
        for (i = 0; i < 8; ++i) {
            buf[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
        }
    }
    sha1Block(h, buf);

    {
        static const char hex[] = "0123456789abcdef";
        int j;
        for (i = 0; i < 5; ++i) {
            for (j = 0; j < 8; ++j) {
                out_hex[i * 8 + j] = hex[(h[i] >> (28 - 4 * j)) & 0xF];
            }
        }
    }
    out_hex[40] = '\0';
    return 1;
}
