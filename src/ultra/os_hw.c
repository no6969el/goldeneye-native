/*
 * os_hw.c — the hardware-bound half of libultra.
 *
 * src/libultra/gu, audio and libc compile for the host unchanged, because they
 * are software. This file covers what is left: the parts that talk to the RCP,
 * the PI/SI buses, the TLB and the caches. There is no source to compile for
 * these — replacing them is the whole reason src/ultra/ exists.
 *
 * THE RULE THIS FILE FOLLOWS
 *
 * Three times on this port, something that compiled cleanly turned out to be
 * silently wrong (a struct member rename that re-resolved to a different field,
 * a macro that reduced a case body to one token, a weak alias that aliased the
 * wrong thing). A file full of stubs returning plausible values is the same
 * trap with better camouflage, so every function here is explicitly one of:
 *
 *   REAL      — implemented, behaves as the hardware did.
 *   NO-OP     — doing nothing IS correct on a host. Cache maintenance on
 *               coherent memory, for instance. Not a shortcut; a fact.
 *   ABSENT    — reports hardware that genuinely is not there (no Controller
 *               Pak). A legitimate state the game already handles.
 *   TODO      — not implemented. Calls GE_UNIMPLEMENTED, which complains on
 *               stderr the first time. It never returns a fabricated value
 *               that lets the caller carry on as if it worked.
 *
 * The classification is on every function. If one is wrong, it is wrong out
 * loud.
 */

#include <stdio.h>
#include <string.h>

#include <ultra64.h>

#include "os_hw.h"

/* ------------------------------------------------------------------------ */
/* Complaining, once per site.                                              */
/* ------------------------------------------------------------------------ */

static void geUnimplemented(const char *fn, int *once)
{
    if (*once == 0) {
        *once = 1;
        fprintf(stderr, "[ge] UNIMPLEMENTED libultra call: %s()\n", fn);
    }
}

#define GE_UNIMPLEMENTED()                       \
    do {                                         \
        static int ge_once_ = 0;                 \
        geUnimplemented(__func__, &ge_once_);    \
    } while (0)

/* ------------------------------------------------------------------------ */
/* Machine description. REAL — these are constants on any real N64.         */
/* ------------------------------------------------------------------------ */

/*
 * NTSC. The port targets the US ROM (see hostcompat/ge_segments.h), and the
 * game reads osTvType to pick frame timing and the VI mode.
 */
u32 osTvType = OS_TV_NTSC;

/* VR4300 counter rate for NTSC: 62.5 MHz. osGetCount()/osGetTime() in
 * os_thread.cpp are scaled against this, so it is not decorative. */
u64 osClockRate = 62500000ULL;

/*
 * osViModeTable is NOT here. src/libultrare/io/vitbl.c defines the real 42-entry
 * table and it is pure data, so the port compiles the decomp's own copy rather
 * than a hand-written stand-in -- the same reasoning that made gu/ and audio/
 * come from the decomp instead of being reimplemented.
 */

/* ------------------------------------------------------------------------ */
/* Caches. NO-OP — and this is a fact about the host, not a shortcut.       */
/*                                                                          */
/* On N64 the CPU writes through a cache the RCP cannot see, so the game     */
/* must flush before handing a display list or audio buffer to the RSP. Host */
/* memory is coherent: our "RSP" is a function call on the same CPU reading  */
/* the same memory. There is nothing to flush and nothing to invalidate.     */
/* ------------------------------------------------------------------------ */

void osInvalDCache(void *vaddr, s32 nbytes)        { (void)vaddr; (void)nbytes; }
void osInvalICache(void *vaddr, s32 nbytes)        { (void)vaddr; (void)nbytes; }
void osWritebackDCache(void *vaddr, s32 nbytes)    { (void)vaddr; (void)nbytes; }
void osWritebackDCacheAll(void)                    { }

/* ------------------------------------------------------------------------ */
/* FPU control. REAL enough to be correct.                                  */
/* ------------------------------------------------------------------------ */

/*
 * FS (flush denormals) and RN (round to nearest) — what libultra's osInitialize
 * sets. The game reads it back and writes it during thread setup; nothing
 * inspects the exception bits, so a faithful constant is a faithful answer.
 */
static u32 ge_fpcsr = 0x01000800;

u32 __osGetFpcCsr(void)        { return ge_fpcsr; }
u32 __osSetFpcCsr(u32 value)   { u32 old = ge_fpcsr; ge_fpcsr = value; return old; }

/* ------------------------------------------------------------------------ */
/* TLB. TODO — but see the note, this one deserves care later.              */
/*                                                                          */
/* GoldenEye TLB-maps several segments (the 0x70000000 code region is the    */
/* obvious one), which is why the segment table in hostcompat/ge_segments.h  */
/* contains virtual addresses that are not RDRAM. On a host every address is */
/* already directly reachable, so the mapping itself is unnecessary — but    */
/* code that *queries* the TLB is asking a question the host cannot answer   */
/* honestly, so it complains rather than inventing an entry.                 */
/* ------------------------------------------------------------------------ */

u32 __osGetTLBHi(s32 index)   { (void)index; GE_UNIMPLEMENTED(); return 0; }
void osUnmapTLB(s32 index)    { (void)index; /* NO-OP: nothing was mapped. */ }

/* ------------------------------------------------------------------------ */
/* Fault reporting. ABSENT — nothing has faulted, which is the truth.       */
/* ------------------------------------------------------------------------ */

OSThread *__osGetCurrFaultedThread(void)
{
    return NULL;
}

/* ------------------------------------------------------------------------ */
/* Controller Pak / EEPROM.                                                 */
/*                                                                          */
/* ABSENT rather than TODO. "No Controller Pak inserted" and "no EEPROM      */
/* responding" are states the game already handles gracefully, because they  */
/* happen on real hardware. Reporting them is honest and keeps the boot path */
/* moving; faking a working save device would corrupt saves instead.         */
/*                                                                          */
/* TODO(phase2): back osEeprom* with a host file so saves persist. Until     */
/* then the game runs and simply cannot save.                                */
/* ------------------------------------------------------------------------ */

u8 __osContLastCmd = 0;
u8 __osPfsPifRam[64];

s32 osEepromProbe(OSMesgQueue *mq)                                   { (void)mq; return 0; }
s32 osEepromRead(OSMesgQueue *mq, u8 a, u8 *b)                       { (void)mq; (void)a; (void)b; return -1; }
s32 osEepromWrite(OSMesgQueue *mq, u8 a, u8 *b)                      { (void)mq; (void)a; (void)b; return -1; }
s32 osEepromLongRead(OSMesgQueue *mq, u8 a, u8 *b, int n)            { (void)mq; (void)a; (void)b; (void)n; return -1; }
s32 osEepromLongWrite(OSMesgQueue *mq, u8 a, u8 *b, int n)           { (void)mq; (void)a; (void)b; (void)n; return -1; }

s32 osPfsInit(OSMesgQueue *mq, OSPfs *pfs, int channel)
{
    (void)mq; (void)channel;
    if (pfs != NULL) {
        memset(pfs, 0, sizeof(*pfs));
    }
    return PFS_ERR_NOPACK;   /* the honest answer */
}

s32 __osContRamRead(OSMesgQueue *mq, int ch, u16 addr, u8 *buf)
{
    (void)mq; (void)ch; (void)addr; (void)buf;
    return PFS_ERR_NOPACK;
}

s32 __osContRamWrite(OSMesgQueue *mq, int ch, u16 addr, u8 *buf, int force)
{
    (void)mq; (void)ch; (void)addr; (void)buf; (void)force;
    return PFS_ERR_NOPACK;
}

/*
 * REAL. The 5-bit address CRC the PIF uses for Controller Pak transfers.
 * Implemented rather than stubbed because it is pure arithmetic with a known
 * answer, and because a wrong CRC would be indistinguishable from a wrong
 * address at the call site.
 */
u8 __osContAddressCrc(u16 addr)
{
    u32 temp = 0;
    u32 bit;

    for (bit = addr & ~0x1Fu; bit != 0; bit <<= 1) {
        u32 x = temp & 0x10;
        temp <<= 1;
        if (bit & 0x8000) {
            temp |= (x != 0) ? 0 : 1;
        } else if (x != 0) {
            temp |= 1;
        }
    }
    for (bit = 0; bit < 5; ++bit) {
        u32 x = temp & 0x10;
        temp <<= 1;
        if (x != 0) {
            temp |= 1;
        }
    }
    return (u8)(temp & 0x1F);
}

/* ------------------------------------------------------------------------ */
/* SI. Thin wrappers — os_io.cpp owns the controller state.                 */
/* ------------------------------------------------------------------------ */

void __osSiGetAccess(void)  { /* NO-OP: the shim's SI is not concurrent. */ }
void __osSiRelAccess(void)  { /* NO-OP */ }

s32 __osSiRawStartDma(s32 dir, void *dramAddr)
{
    (void)dir; (void)dramAddr;
    GE_UNIMPLEMENTED();   /* raw PIF access; osCont* is the path the game uses */
    return -1;
}

/* ------------------------------------------------------------------------ */
/* PI. REAL — forwards to the shim's DMA, which reads the mounted ROM.      */
/* ------------------------------------------------------------------------ */

s32 osPiRawStartDma(s32 dir, u32 devAddr, void *dramAddr, u32 size)
{
    return __osPiRawStartDma(dir, devAddr, dramAddr, size);
}

/*
 * PI register access. The game pokes the cart domain timing registers at boot.
 * There is no cart to configure, so writes are dropped and reads report zero —
 * but they are counted, so "did the game try to talk to hardware we ignored?"
 * has an answer during bring-up rather than being invisible.
 */
static u32 ge_pi_io_writes = 0;
static u32 ge_pi_io_reads  = 0;

s32 osPiWriteIo(u32 devAddr, u32 data)
{
    (void)devAddr; (void)data;
    ++ge_pi_io_writes;
    return 0;
}

s32 osPiReadIo(u32 devAddr, u32 *data)
{
    (void)devAddr;
    ++ge_pi_io_reads;
    if (data != NULL) {
        *data = 0;
    }
    return 0;
}

u32 geHwPiIoWriteCount(void) { return ge_pi_io_writes; }
u32 geHwPiIoReadCount(void)  { return ge_pi_io_reads; }

/* ------------------------------------------------------------------------ */
/* Managers. REAL — the shim has no separate manager threads, by design.    */
/*                                                                          */
/* On N64 these spawn threads that serialise PI and VI requests. The shim's  */
/* PI is synchronous and its VI is latched by the frame driver, so there is  */
/* nothing to serialise. Creating a thread that only ever forwards would add */
/* a scheduling hazard for no behaviour.                                     */
/* ------------------------------------------------------------------------ */

void osCreatePiManager(OSPri pri, OSMesgQueue *cmdQ, OSMesg *cmdBuf, s32 cmdMsgCnt)
{
    (void)pri; (void)cmdQ; (void)cmdBuf; (void)cmdMsgCnt;
}

void osCreateViManager(OSPri pri)
{
    (void)pri;
}

void osInitialize(void)
{
    /*
     * The real osInitialize sets up the exception vectors, caches, PI domains
     * and the counter. Everything it configures is either already true here or
     * meaningless. The shim's own bring-up (RDRAM, ROM mount, scheduler) is
     * driven by the host entry point, not from inside the game.
     */
}

/* ------------------------------------------------------------------------ */
/* DP.                                                                      */
/* ------------------------------------------------------------------------ */

static u32 ge_dp_status = 0;

void osDpSetStatus(u32 status)      { ge_dp_status = status; }

void osDpGetCounters(u32 *dst)
{
    /*
     * Profiling counters (clock, command, pipe, TMEM busy cycles). Zero is a
     * defensible answer — there is no RDP — and the game only displays these on
     * a debug overlay.
     */
    if (dst != NULL) {
        memset(dst, 0, sizeof(u32) * 4);
    }
}

s32 osDpSetNextBuffer(void *ptr, u64 size)
{
    (void)ptr; (void)size;
    GE_UNIMPLEMENTED();   /* direct RDP command buffers; the game uses tasks */
    return -1;
}
