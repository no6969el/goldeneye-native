/*
 * ge_fault.h -- see ge_fault.c. Install once, early in main().
 */
#ifndef GE_FAULT_H
#define GE_FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Install a SIGSEGV/SIGBUS handler that recognises a faulting address of the
 * form 0xFFFFFFFF8xxxxxxx -- an RDRAM pointer truncated to s32 and
 * sign-extended -- and says so. It reports and re-raises; it never repairs the
 * access and continues.
 */
void geFaultInstall(void);

#ifdef __cplusplus
}
#endif

#endif /* GE_FAULT_H */
