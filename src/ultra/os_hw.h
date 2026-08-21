/*
 * os_hw.h — bring-up visibility into the hardware shim.
 *
 * The libultra entry points themselves are declared by <PR/os.h>; this header
 * only exposes the counters os_hw.c keeps, so "did the game try to talk to
 * hardware we ignored?" is a question with an answer during bring-up.
 */
#ifndef GE_ULTRA_OS_HW_H
#define GE_ULTRA_OS_HW_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned int geHwPiIoWriteCount(void);
unsigned int geHwPiIoReadCount(void);

#ifdef __cplusplus
}
#endif

#endif /* GE_ULTRA_OS_HW_H */
