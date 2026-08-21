/* sha1.h — ROM identity. See sha1.c. */
#ifndef GE_HOST_SHA1_H
#define GE_HOST_SHA1_H
#ifdef __cplusplus
extern "C" {
#endif
/* Writes 40 lowercase hex digits plus NUL. Returns 0 if the file cannot be read. */
int geSha1File(const char *path, char out_hex[41]);
#ifdef __cplusplus
}
#endif
#endif
