// os.h — libultra-compatible OS surface, host implementation.
//
// This replaces <PR/os.h> for the port. Signatures match libultra exactly so
// game code compiles unchanged; the implementations are host-side.
//
// LAYOUT NOTE: OSThread carries two extra host-only fields at the end. That
// makes sizeof(OSThread) larger than on N64, which is fine because the port
// compiles ALL code against this header — but it means you must not mix this
// header with the original PR/os.h in one build, and you must not memcpy an
// OSThread across that boundary. The game statically allocates its OSThreads
// (src/init.c:170, src/sched.c:183, src/audi.c:397), so growth is harmless.
//
// The original __OSThreadContext (register save area) is retained at its
// original offset but is never written: context switching is done by the fiber,
// not by saving MIPS registers. It stays so that any game code reading
// thread->context.pc for diagnostics still compiles.

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// --- ultratypes -----------------------------------------------------------
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

typedef s32 OSPri;
typedef s32 OSId;
typedef void* OSMesg;
typedef u32 OSEvent;
typedef u32 OSIntMask;

typedef union { struct { f32 f_odd; f32 f_even; } f; f64 d; } __OSfp;

typedef struct {
    u64 at, v0, v1, a0, a1, a2, a3;
    u64 t0, t1, t2, t3, t4, t5, t6, t7;
    u64 s0, s1, s2, s3, s4, s5, s6, s7;
    u64 t8, t9, gp, sp, s8, ra;
    u64 lo, hi;
    u32 sr, pc, cause, badvaddr, rcp;
    u32 fpcsr;
    __OSfp  fp0,  fp2,  fp4,  fp6,  fp8, fp10, fp12, fp14;
    __OSfp fp16, fp18, fp20, fp22, fp24, fp26, fp28, fp30;
} __OSThreadContext;

typedef struct OSThread_s {
    struct OSThread_s*  next;      // run/mesg queue link
    OSPri               priority;
    struct OSThread_s** queue;     // queue this thread is on
    struct OSThread_s*  tlnext;    // all-threads link
    u16                 state;     // OS_STATE_*
    u16                 flags;
    OSId                id;
    int                 fp;
    __OSThreadContext   context;   // see GeThreadHost below
} OSThread;

// ---------------------------------------------------------------------------
// Host thread state — and why it lives INSIDE `context`.
//
// The obvious place for it is three extra pointers appended to OSThread. That
// is what this file used to do, and it was silently corrupting memory.
//
// The game allocates its own OSThreads as statics (`OSThread mainThread;` in
// src/init.c) and compiles them against the DECOMP's <PR/os.h>, which knows
// nothing about host fields. So the game's OSThread was 448 bytes and the
// shim's was 472, and every osCreateThread() wrote 24 bytes past the end of the
// caller's object — over whatever static followed it. The symptom was a fault
// inside swapcontext() at the first context switch, with nothing to connect it
// to a struct layout.
//
// `context` is the saved MIPS register file: 400 bytes that exist only for
// source compatibility and are never used on a host. Putting the host state
// there keeps both definitions at 448 bytes and makes the disagreement
// impossible rather than merely fixed.
// ---------------------------------------------------------------------------

typedef struct GeThreadHost {
    void* fiber;   // ge_ultra::Fiber*
    void* entry;   // original entry fn
    void* arg;
} GeThreadHost;

#ifdef __cplusplus
static_assert(sizeof(GeThreadHost) <= sizeof(__OSThreadContext),
              "host thread state must fit inside the unused register context");
#endif

#define GE_THREAD_HOST(t) ((GeThreadHost*)&(t)->context)

typedef struct OSMesgQueue_s {
    OSThread* mtqueue;      // threads blocked on empty (receivers)
    OSThread* fullqueue;    // threads blocked on full (senders)
    s32       validCount;
    s32       first;
    s32       msgCount;
    OSMesg*   msg;
} OSMesgQueue;

typedef u64 OSTime;

typedef struct OSTimer_s {
    struct OSTimer_s* next;
    struct OSTimer_s* prev;
    OSTime            interval;
    OSTime            value;
    OSMesgQueue*      mq;
    OSMesg            msg;
} OSTimer;

// --- constants ------------------------------------------------------------
#define OS_STATE_STOPPED   1
#define OS_STATE_RUNNABLE  2
#define OS_STATE_RUNNING   4
#define OS_STATE_WAITING   8

#define OS_PRIORITY_MAX      255
#define OS_PRIORITY_VIMGR    254
#define OS_PRIORITY_RMON     250
#define OS_PRIORITY_RMONSPIN 200
#define OS_PRIORITY_PIMGR    150
#define OS_PRIORITY_SIMGR    140
#define OS_PRIORITY_APPMAX   127
#define OS_PRIORITY_IDLE       0

#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK   1

// N64 CPU counter runs at 46.875 MHz (93.75 MHz / 2).
#define OS_CPU_COUNTER 46875000ull
#define OS_NSEC_TO_CYCLES(n) (((u64)(n) * (OS_CPU_COUNTER / 15625000ull)) / 64ull)
#define OS_USEC_TO_CYCLES(u) (((u64)(u) * (OS_CPU_COUNTER / 15625ull)) / 64ull)
#define OS_CYCLES_TO_NSEC(c) (((u64)(c) * 64ull) / (OS_CPU_COUNTER / 15625000ull))
#define OS_CYCLES_TO_USEC(c) (((u64)(c) * 64ull) / (OS_CPU_COUNTER / 15625ull))

// --- threads --------------------------------------------------------------
void  osCreateThread(OSThread* t, OSId id, void (*entry)(void*), void* arg,
                     void* sp, OSPri pri);
void  osStartThread(OSThread* t);
void  osStopThread(OSThread* t);
void  osDestroyThread(OSThread* t);
void  osYieldThread(void);
void  osSetThreadPri(OSThread* t, OSPri pri);
OSPri osGetThreadPri(OSThread* t);
OSId  osGetThreadId(OSThread* t);

// --- message queues -------------------------------------------------------
void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msgBuf, s32 count);
s32  osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flags);
s32  osJamMesg(OSMesgQueue* mq, OSMesg msg, s32 flags);
s32  osRecvMesg(OSMesgQueue* mq, OSMesg* msg, s32 flags);
void osSetEventMesg(OSEvent e, OSMesgQueue* mq, OSMesg msg);

// --- time -----------------------------------------------------------------
OSTime osGetTime(void);
u32    osGetCount(void);
int    osSetTimer(OSTimer* t, OSTime countdown, OSTime interval,
                  OSMesgQueue* mq, OSMesg msg);
int    osStopTimer(OSTimer* t);

// --- PI / DMA -------------------------------------------------------------
#define OS_READ  0
#define OS_WRITE 1
#define OS_MESG_PRI_NORMAL 0
#define OS_MESG_PRI_HIGH   1
#define PI_STATUS_DMA_BUSY 0x01
#define PI_STATUS_IO_BUSY  0x02
#define PI_STATUS_ERROR    0x04

typedef struct OSPiHandle_s {
    struct OSPiHandle_s* next;
    u8  type;
    u8  latency;
    u8  pageSize;
    u8  relDuration;
    u8  pulse;
    u8  domain;
    u32 baseAddress;
    u32 speed;
} OSPiHandle;

typedef struct {
    u16          type;
    u8           pri;
    u8           status;
    OSMesgQueue* retQueue;
} OSIoMesgHdr;

typedef struct {
    OSIoMesgHdr hdr;
    void*       dramAddr;
    u32         devAddr;
    u32         size;
    OSPiHandle* piHandle;
} OSIoMesg;

OSPiHandle* osCartRomInit(void);
s32  osPiStartDma(OSIoMesg* mb, s32 pri, s32 direction, u32 devAddr,
                  void* dramAddr, u32 size, OSMesgQueue* mq);
s32  __osPiRawStartDma(s32 direction, u32 devAddr, void* dramAddr, u32 size);
u32  osPiGetStatus(void);
u32  osVirtualToPhysical(void* p);

// --- VI -------------------------------------------------------------------
void  osViInit(void);
void  osViSetMode(void* mode);
void  osViSetEvent(OSMesgQueue* mq, OSMesg msg, u32 retraceCount);
void  osViSwapBuffer(void* framebuffer);
void* osViGetCurrentFramebuffer(void);
void* osViGetNextFramebuffer(void);
void  osViBlack(u8 active);
void  osViSetSpecialFeatures(u32 func);
void  osViSetXScale(f32 value);
void  osViSetYScale(f32 value);
void  osViRepeatLine(u8 active);

// --- SI / controllers -----------------------------------------------------
typedef struct {
    u16 type;
    u8  status;
    u8  errnum;
} OSContStatus;

typedef struct {
    u16 button;
    s8  stick_x;
    s8  stick_y;
    u8  errnum;
} OSContPad;

#define CONT_A      0x8000
#define CONT_B      0x4000
#define CONT_G      0x2000  // Z
#define CONT_START  0x1000
#define CONT_UP     0x0800
#define CONT_DOWN   0x0400
#define CONT_LEFT   0x0200
#define CONT_RIGHT  0x0100
#define CONT_L      0x0020
#define CONT_R      0x0010
#define CONT_E      0x0008  // C-up
#define CONT_D      0x0004  // C-down
#define CONT_C      0x0002  // C-left
#define CONT_F      0x0001  // C-right
#define CONT_TYPE_NORMAL 0x0005

#define MAXCONTROLLERS 4

s32  osContInit(OSMesgQueue* mq, u8* bitpattern, OSContStatus* status);
s32  osContStartReadData(OSMesgQueue* mq);
void osContGetReadData(OSContPad* pad);
s32  osContStartQuery(OSMesgQueue* mq);
void osContGetQuery(OSContStatus* status);
s32  osMotorInit(OSMesgQueue* mq, void* pfs, int channel);
s32  osMotorStart(void* pfs);
s32  osMotorStop(void* pfs);

// --- AI (audio interface) -------------------------------------------------
// osAiGetLength is load-bearing: src/audi.c:517 sizes every audio frame from it.
// See the header comment in os_ai.cpp.
s32 osAiSetFrequency(u32 frequency);
s32 osAiSetNextBuffer(void* buf, u32 size);
u32 osAiGetLength(void);
u32 osAiGetStatus(void);

// --- interrupts -----------------------------------------------------------
// On N64 these masked the interrupt controller. Here they are a nesting counter
// that suppresses preemption, which is the only property game code relies on.
OSIntMask osSetIntMask(OSIntMask m);
OSIntMask osGetIntMask(void);
u32       __osDisableInt(void);
void      __osRestoreInt(u32 mask);

#ifdef __cplusplus
}  // extern "C"
#endif

// ---------------------------------------------------------------------------
// Host control surface. Not part of libultra; used by the port's main().
// ---------------------------------------------------------------------------
#ifdef __cplusplus
namespace ge_ultra {

// Must be called once before any osCreateThread. Wraps the calling context so
// the scheduler has somewhere to return to.
void bootScheduler();
void shutdownScheduler();

// Run until every thread is blocked or stopped. Returns the number of context
// switches performed. The port calls this once per frame after posting the
// retrace message; when it returns, the frame's work is done.
int runUntilIdle();

// Advance the virtual clock and fire any expired timers. Time does NOT advance
// on its own: the frame loop owns it, so a debugger breakpoint cannot cause the
// game to think an hour passed. This is also what makes the scheduler tests
// deterministic.
void advanceTime(OSTime cycles);

// Deliver an event (OS_EVENT_VI, OS_EVENT_SP, ...) to whatever queue
// osSetEventMesg registered. This is how the port injects "retrace happened"
// and "RSP task finished".
bool sendEvent(OSEvent e);

OSThread* runningThread();

// Peak stack usage in bytes, from the 0xCD poison pattern. Returns 0 if the
// thread has no fiber. The game's stack sizes were tuned for a 1997 compiler;
// check this before trusting them.
size_t stackHighWater(OSThread* t);

struct SchedStats {
    uint64_t context_switches = 0;
    uint64_t mesg_sends = 0;
    uint64_t mesg_recvs = 0;
    uint64_t blocks = 0;
    uint64_t timers_fired = 0;
    uint64_t events_sent = 0;
    uint64_t events_undelivered = 0;   // no queue registered for that event
};
const SchedStats& schedStats();

/*
 * Dump every thread the game created, with its state and what it is waiting on.
 *
 * During bring-up "the port ran 10 frames and issued no graphics tasks" is not
 * a useful observation on its own -- it is the same output whether the render
 * thread never started, or started and blocked on a message queue nothing
 * sends to. This tells them apart.
 */
void dumpThreads(const char* label);

}  // namespace ge_ultra
#endif
