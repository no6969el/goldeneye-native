/*
 * os_tlb.h — TLB no-ops, the inflate trampoline, and the BSD string functions
 * the MSVC CRT does not provide. See os_tlb.c for why the TLB needs nothing.
 */
#ifndef GE_ULTRA_OS_TLB_H
#define GE_ULTRA_OS_TLB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void initTLBPrepareContext(void);
void resolve_TLBaddress_for_InvalidHit(void);

uint32_t jump_decompressfile(uint32_t source, uint32_t target, uint32_t buffer);

void bcopy(const void *src, void *dst, size_t n);
void bzero(void *dst, size_t n);

extern float __libm_qnan_f;

#ifdef __cplusplus
}
#endif

#endif /* GE_ULTRA_OS_TLB_H */
