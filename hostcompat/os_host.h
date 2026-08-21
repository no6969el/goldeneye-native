/*
 * os_host.h — the few host hooks the game's own source calls directly.
 *
 * Lives in hostcompat/ rather than src/ultra/ deliberately. Putting src/ultra
 * on the game's include path shadows the decomp's own headers -- src/ultra/
 * has random.h and os.h, and so does the decomp -- which silently swapped the
 * game's RNG declarations for the shim's and produced 30 undefined references
 * at link time. hostcompat/ is already first on the path and contains only
 * files meant to shadow.
 */
#ifndef GE_ULTRA_OS_HOST_H
#define GE_ULTRA_OS_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Yield to the host frame loop and stay runnable. Used by the idle thread in
 * place of `for (;;);`, which cannot work under a cooperative scheduler.
 * See src/ultra/os_thread.cpp for the full reasoning.
 */
void geIdleWait(void);

#ifdef __cplusplus
}
#endif

#endif /* GE_ULTRA_OS_HOST_H */
