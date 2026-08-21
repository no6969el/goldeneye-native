/*
 * random.h — the game's RNG on the host. See random.c for why this is a real
 * implementation rather than a stub.
 *
 * Signatures match src/random.h and src/game/chrObjRandom.h in the decomp
 * exactly, so the game's own declarations do not conflict.
 */
#ifndef GE_ULTRA_RANDOM_H
#define GE_ULTRA_RANDOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint64_t g_randomSeed;
extern uint64_t g_chrObjRandomSeed;
extern uint64_t g_tlbRandomSeed;

/* One step of the generator over an explicit state. Exposed for tests. */
int32_t  geRandomStep(uint64_t *state);

uint32_t randomGetNext(void);
uint32_t randomGetNextFrom(uint64_t *state);
void     randomSetSeed(uint32_t seed);

uint32_t chrObjRandomGetNext(void);
void     chrObjRandomSetSeed(uint32_t seed);

uint32_t tlbRandomGetNext(void);

#ifdef __cplusplus
}
#endif

#endif /* GE_ULTRA_RANDOM_H */
