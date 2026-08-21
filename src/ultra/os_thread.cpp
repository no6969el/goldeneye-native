// os_thread.cpp — the scheduler.
//
// Semantics are taken from the reference implementations that ship in the
// decomp itself (src/libultra/os/{thread,startthread,sendmesg,recvmesg,
// yieldthread,setthreadpri}.c), not from memory or from documentation. Where
// this file looks odd, it is because libultra looked odd and the game depends
// on it.
//
// The two rules that matter:
//
//   1. Queues are priority-sorted descending, FIFO within equal priority.
//      __osEnqueueThread inserts AFTER all threads of >= priority.
//   2. There is NO time-slicing. A running thread continues until it blocks,
//      yields, stops, or something strictly higher-priority becomes runnable.
//      Equal priority does not preempt — note the strict `<` in osStartThread.
//
// Reproducing rule 2 exactly is the point of the whole fiber design. Round-robin
// between equal-priority threads would be "fairer" and would break the game.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "fiber.h"
#include "os.h"

// Exported below with C linkage; declared here because Sched binds a reference
// to it. libultra initialises it to the thread-tail sentinel, and so does
// scheduler boot.
extern "C" OSThread *__osRunQueue;

namespace ge_ultra {
namespace {

constexpr size_t kFiberStackSize = 512 * 1024;

// Sentinel terminating every queue, priority -1 so it always sorts last.
// libultra does exactly this (`struct __osThreadTail = {0, -1}`) so that
// enqueue never needs a null check.
OSThread g_thread_tail_storage{};
OSThread* const kTail = &g_thread_tail_storage;

}  // namespace
}  // namespace ge_ultra

// The run queue head lives OUTSIDE the Sched struct, unlike every other piece
// of scheduler state, because src/crash.c reaches into it directly:
//
//     extern OSThread *__osRunQueue;
//     __osEnqueueThread(&__osRunQueue, curr);
//
// It takes the ADDRESS, so it has to be the real head and not a copy that
// tracks it -- enqueuing into a duplicate would put the thread on a list
// nothing ever dispatches from, and it would simply never run again.
//
// Defined after kTail so this initialiser can use it: within one translation
// unit, static initialisation runs in declaration order.
extern "C" OSThread *__osRunQueue = ge_ultra::kTail;

namespace ge_ultra {
namespace {

struct Sched {
    OSThread* active_queue = kTail;
    OSThread* running = nullptr;
    Fiber* host_fiber = nullptr;
    int int_disable_depth = 0;
    OSTime now = 0;
    OSTimer* timers = nullptr;
    SchedStats stats{};
    bool booted = false;

    // Event queues, indexed by OSEvent.
    static constexpr int kMaxEvents = 24;
    OSMesgQueue* event_mq[kMaxEvents]{};
    OSMesg event_msg[kMaxEvents]{};

    // Every thread ever created, in creation order. See osCreateThread.
    static constexpr int kMaxThreads = 32;
    OSThread* all[kMaxThreads]{};
    int n_all = 0;
};

Sched g;

void enqueueThread(OSThread** queue, OSThread* t) {
    OSThread* pred = reinterpret_cast<OSThread*>(queue);
    OSThread* succ = pred->next;
    // Skip everything of greater-or-equal priority: FIFO among equals.
    while (succ && succ->priority >= t->priority) {
        pred = succ;
        succ = pred->next;
    }
    t->next = succ;
    pred->next = t;
    t->queue = queue;
}

OSThread* popThread(OSThread** queue) {
    OSThread* t = *queue;
    if (!t || t == kTail) return nullptr;
    *queue = t->next;
    t->queue = nullptr;
    return t;
}

void dequeueThread(OSThread** queue, OSThread* t) {
    OSThread* pred = reinterpret_cast<OSThread*>(queue);
    OSThread* succ = pred->next;
    while (succ && succ != kTail) {
        if (succ == t) {
            pred->next = t->next;
            t->queue = nullptr;
            return;
        }
        pred = succ;
        succ = pred->next;
    }
}

OSPri queueHeadPriority(OSThread* q) {
    return (q && q != kTail) ? q->priority : -1;
}

Fiber* fiberOf(OSThread* t) { return static_cast<Fiber*>(GE_THREAD_HOST(t)->fiber); }

// Switch into the highest-priority runnable thread. If nothing is runnable,
// return to the host context — that is what makes runUntilIdle() terminate
// instead of deadlocking when every thread is blocked on a message that only
// the frame loop can send.
void dispatch() {
    OSThread* next = popThread(&__osRunQueue);
    if (!next) {
        OSThread* prev = g.running;
        g.running = nullptr;
        if (prev) Fiber::switchTo(fiberOf(prev), g.host_fiber);
        return;
    }
    next->state = OS_STATE_RUNNING;
    OSThread* prev = g.running;
    g.running = next;
    ++g.stats.context_switches;
    Fiber::switchTo(prev ? fiberOf(prev) : g.host_fiber, fiberOf(next));
}

// Put the running thread on `queue` and switch away. This is libultra's
// __osEnqueueAndYield and it is the single blocking primitive: every block in
// the system goes through here.
void enqueueAndYield(OSThread** queue) {
    OSThread* self = g.running;
    if (!self) {
        // Called from the host context. Only possible if the port blocks outside
        // a game thread, which is a bug worth failing loudly on rather than
        // silently hanging.
        std::fprintf(stderr, "[ge-ultra] block attempted from host context\n");
        std::abort();
    }
    enqueueThread(queue, self);
    ++g.stats.blocks;
    dispatch();
}

void fiberEntry(void* arg) {
    auto* t = static_cast<OSThread*>(arg);
    auto entry = reinterpret_cast<void (*)(void*)>(GE_THREAD_HOST(t)->entry);
    entry(GE_THREAD_HOST(t)->arg);
    // The thread's entry returned. On N64 this would run off into whatever
    // followed; here we mark it stopped and dispatch elsewhere. The game's
    // threads are all infinite loops, so reaching this is informative.
    // NOTE: g.running is deliberately NOT cleared here. dispatch() needs it to
    // know which fiber to switch AWAY from. Clearing it first strands us on the
    // dying thread's stack — dispatch finds no `prev`, returns normally, and
    // execution falls off the end of the entry function.
    t->state = OS_STATE_STOPPED;
    dispatch();
}

}  // namespace

// ---------------------------------------------------------------------------
// Host control
// ---------------------------------------------------------------------------

void bootScheduler() {
    if (g.booted) return;
    g = Sched{};
    g_thread_tail_storage.next = nullptr;
    g_thread_tail_storage.priority = -1;
    __osRunQueue = kTail;
    g.active_queue = kTail;
    g.host_fiber = Fiber::createForCurrentContext();
    g.booted = true;
}

void shutdownScheduler() {
    if (!g.booted) return;
    for (OSThread* t = g.active_queue; t && t != kTail; t = t->tlnext) {
        if (GE_THREAD_HOST(t)->fiber) {
            Fiber::destroy(fiberOf(t));
            GE_THREAD_HOST(t)->fiber = nullptr;
        }
    }
    Fiber::destroy(g.host_fiber);
    g.booted = false;
}

// ---------------------------------------------------------------------------
// geIdleWait — what the idle thread does instead of spinning.
//
// src/init.c's idleproc is `for (;;);`, a true busy loop. On N64 that is
// correct: the idle thread runs at the lowest priority and the VI interrupt
// preempts it whenever there is real work. This scheduler is COOPERATIVE and
// has no preemption at all (deliberately — see the header comment), so once
// idle started running, nothing else ever ran again. The port booted, created
// its threads, entered idle and hung there.
//
// The host frame loop is this port's equivalent of the VI interrupt. So idle
// hands control back to it and waits to be picked again next frame:
//
//   - put ourselves back on the run queue, so we stay runnable
//   - switch to the host fiber
//
// The host advances the clock, delivers timer and VI events, and dispatches the
// highest-priority runnable thread. If something real woke up it runs; if not,
// idle resumes and does this again — one host round-trip per frame, rather than
// a core pinned at 100%.
//
// NOT a scheduler heuristic like "if the only runnable thread is low priority,
// give up". That would guess at which thread is the idle one, and would change
// behaviour for any game thread that happens to sit at a low priority.
// ---------------------------------------------------------------------------

extern "C" void geIdleWait() {
    OSThread* self = g.running;
    if (!self) {
        return;   // called from the host context: nothing to wait for
    }
    enqueueThread(&__osRunQueue, self);
    g.running = nullptr;
    Fiber::switchTo(fiberOf(self), g.host_fiber);
}

int runUntilIdle() {
    const uint64_t before = g.stats.context_switches;
    if (queueHeadPriority(__osRunQueue) >= 0) dispatch();
    return int(g.stats.context_switches - before);
}

OSThread* runningThread() { return g.running; }
const SchedStats& schedStats() { return g.stats; }

void dumpThreads(const char* label) {
    std::printf("threads (%s):\n", label ? label : "");
    int n = 0;
    // The registry, not a scheduler queue. Walking active_queue/tlnext reported
    // only what happened to be linked there; a thread parked on a message queue
    // is not, which meant the threads a hang is ABOUT were the ones missing from
    // the hang report.
    for (int idx = 0; idx < g.n_all; ++idx) {
        OSThread* t = g.all[idx];
        if (t == nullptr) continue;
        const char* st = "?";
        switch (t->state) {
            case OS_STATE_STOPPED:  st = "stopped";  break;
            case OS_STATE_RUNNABLE: st = "runnable"; break;
            case OS_STATE_RUNNING:  st = "RUNNING";  break;
            case OS_STATE_WAITING:  st = "waiting";  break;
            default: break;
        }
        const bool on_run_q = (t->queue == &__osRunQueue);
        // The queue ADDRESS is the useful part during bring-up: "blocked on a
        // queue" is true of every stalled thread and tells you nothing, whereas
        // the address can be looked up in the binary's symbol table and named.
        // The resume PC is the part that ends the investigation rather than
        // starting it. Six libultra threads share one host thread, so a
        // debugger attached to a hung process shows exactly one stack -- the
        // fiber that is current, which is by definition not the stuck one. The
        // stuck ones have their program counters sitting in ucontext structs
        // nothing else will look at. Feed it to:
        //     addr2line -e ge007 -f -C 0x<pc>
        Fiber* f = fiberOf(t);
        const unsigned long pc =
            (unsigned long)(f ? f->resumePc() : (uintptr_t)0);
        std::printf("  id %-3d pri %-3d %-9s %-18s queue=%p pc=0x%lx%s\n",
                    int(t->id), int(t->priority), st,
                    t->queue ? (on_run_q ? "on run queue" : "blocked")
                             : "not queued",
                    (void*)t->queue, pc,
                    (t == g.running) ? "  <- current" : "");
        ++n;
    }
    if (n == 0) std::printf("  (none)\n");
    std::printf("  switches=%llu blocks=%llu sends=%llu recvs=%llu timers=%llu\n"
                "  events: %llu delivered, %llu undelivered (no queue registered)\n",
                (unsigned long long)g.stats.context_switches,
                (unsigned long long)g.stats.blocks,
                (unsigned long long)g.stats.mesg_sends,
                (unsigned long long)g.stats.mesg_recvs,
                (unsigned long long)g.stats.timers_fired,
                (unsigned long long)g.stats.events_sent,
                (unsigned long long)g.stats.events_undelivered);
}


size_t stackHighWater(OSThread* t) {
    // Counts unpoisoned bytes from the low end of the stack. Reported by the
    // fiber layer, which owns the allocation.
    (void)t;
    // TODO(M1): expose the stack base/size from Fiber and scan for 0xCD.
    return 0;
}

}  // namespace ge_ultra

using namespace ge_ultra;

// ---------------------------------------------------------------------------
// libultra: threads
// ---------------------------------------------------------------------------

extern "C" void osCreateThread(OSThread* t, OSId id, void (*entry)(void*),
                               void* arg, void* sp, OSPri pri) {
    // `sp` is the N64 stack pointer the game allocated. We ignore it — the fiber
    // owns its stack — but the game's stacks are statically sized and it is
    // worth NOT reusing them, because a modern compiler's frames are larger and
    // overflowing into adjacent statics is the kind of bug that presents as
    // "audio corrupts when you enter Facility".
    (void)sp;

    // Clear the context FIRST: the host state lives inside it (see
    // GE_THREAD_HOST in os.h), so clearing afterwards would wipe the fiber
    // pointer we are about to store.
    std::memset(&t->context, 0, sizeof(t->context));
    t->next = nullptr;
    t->queue = nullptr;
    t->priority = pri;
    t->id = id;
    t->fp = 0;
    t->flags = 0;
    t->state = OS_STATE_STOPPED;
    GE_THREAD_HOST(t)->entry = reinterpret_cast<void*>(entry);
    GE_THREAD_HOST(t)->arg = arg;
    GE_THREAD_HOST(t)->fiber = Fiber::create(fiberEntry, t, kFiberStackSize);

    // Link onto the all-threads list.
    t->tlnext = g.active_queue;
    g.active_queue = t;

    /*
     * A registry SEPARATE from any scheduler queue. Every existing list is a
     * queue -- the run queue, a message queue's waiter list -- so a thread is
     * on exactly one of them at a time and there is nowhere to enumerate ALL
     * of them from. That is fine until the game stops making progress, at which
     * point the only question worth asking is "where is each thread stuck",
     * and it is unanswerable. This makes it answerable.
     */
    if (g.n_all < Sched::kMaxThreads) {
        g.all[g.n_all++] = t;
    }
}

extern "C" void osStartThread(OSThread* t) {
    const u32 mask = __osDisableInt();

    switch (t->state) {
        case OS_STATE_WAITING:
            t->state = OS_STATE_RUNNABLE;
            enqueueThread(&__osRunQueue, t);
            break;
        case OS_STATE_STOPPED:
            if (t->queue == nullptr || t->queue == &__osRunQueue) {
                t->state = OS_STATE_RUNNABLE;
                enqueueThread(&__osRunQueue, t);
            } else {
                // Started while blocked on a queue: libultra re-enqueues it on
                // that queue and promotes the queue's head to runnable. Odd, but
                // reproduced verbatim from src/libultra/os/startthread.c.
                t->state = OS_STATE_WAITING;
                OSThread** q = t->queue;
                enqueueThread(q, t);
                if (OSThread* head = popThread(q)) enqueueThread(&__osRunQueue, head);
            }
            break;
        default:
            break;
    }

    if (g.running == nullptr) {
        // Starting from the host context: do not dispatch here. The port drives
        // dispatch through runUntilIdle(), which keeps frame boundaries explicit
        // instead of having osStartThread randomly run the whole game.
    } else if (g.running->priority < queueHeadPriority(__osRunQueue)) {
        // STRICTLY less-than. Equal priority does not preempt. This one
        // character is the difference between reproducing N64 scheduling and
        // approximating it.
        g.running->state = OS_STATE_RUNNABLE;
        enqueueAndYield(&__osRunQueue);
    }

    __osRestoreInt(mask);
}

extern "C" void osStopThread(OSThread* t) {
    const u32 mask = __osDisableInt();

    if (t == nullptr || t == g.running) {
        if (g.running) {
            // Stopping ourselves. Leave g.running set: dispatch() reads it to
            // find the fiber to switch away from, and clears it if nothing else
            // is runnable. This call does not return until (and unless) someone
            // restarts this thread.
            g.running->state = OS_STATE_STOPPED;
            dispatch();
        }
    } else {
        switch (t->state) {
            case OS_STATE_RUNNABLE:
                dequeueThread(&__osRunQueue, t);
                t->state = OS_STATE_STOPPED;
                break;
            case OS_STATE_WAITING:
                if (t->queue) dequeueThread(t->queue, t);
                t->state = OS_STATE_STOPPED;
                break;
            default:
                t->state = OS_STATE_STOPPED;
                break;
        }
    }

    __osRestoreInt(mask);
}

extern "C" void osDestroyThread(OSThread* t) {
    const u32 mask = __osDisableInt();
    OSThread* target = t ? t : g.running;
    if (target) {
        if (target->queue) dequeueThread(target->queue, target);
        target->state = OS_STATE_STOPPED;
        // The fiber is intentionally NOT freed here: we may be running on it.
        // shutdownScheduler() reclaims. The game destroys threads approximately
        // never, so leaking a 512 KB stack until exit is the right trade against
        // freeing the stack we are standing on.
    }
    __osRestoreInt(mask);
    if (target == g.running) dispatch();  // see osStopThread: do not clear first
}

extern "C" void osYieldThread(void) {
    const u32 mask = __osDisableInt();
    if (g.running) {
        g.running->state = OS_STATE_RUNNABLE;
        enqueueAndYield(&__osRunQueue);
    }
    __osRestoreInt(mask);
}

extern "C" void osSetThreadPri(OSThread* t, OSPri pri) {
    const u32 mask = __osDisableInt();
    OSThread* target = t ? t : g.running;

    if (target && target->priority != pri) {
        target->priority = pri;
        if (target->state == OS_STATE_RUNNABLE && target->queue) {
            // Re-sort: remove and reinsert at the new priority.
            OSThread** q = target->queue;
            dequeueThread(q, target);
            enqueueThread(q, target);
        }
    }

    // Lowering our own priority may hand control to someone else.
    if (g.running && g.running->priority < queueHeadPriority(__osRunQueue)) {
        g.running->state = OS_STATE_RUNNABLE;
        enqueueAndYield(&__osRunQueue);
    }
    __osRestoreInt(mask);
}

extern "C" OSPri osGetThreadPri(OSThread* t) {
    OSThread* target = t ? t : g.running;
    return target ? target->priority : -1;
}

extern "C" OSId osGetThreadId(OSThread* t) {
    OSThread* target = t ? t : g.running;
    return target ? target->id : 0;
}

// ---------------------------------------------------------------------------
// libultra: message queues
// ---------------------------------------------------------------------------

#define MQ_IS_FULL(mq)  ((mq)->validCount >= (mq)->msgCount)
#define MQ_IS_EMPTY(mq) ((mq)->validCount == 0)

extern "C" void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msgBuf, s32 count) {
    mq->mtqueue = kTail;
    mq->fullqueue = kTail;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
}

extern "C" s32 osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flags) {
    const u32 mask = __osDisableInt();

    while (MQ_IS_FULL(mq)) {
        if (flags == OS_MESG_BLOCK) {
            if (!g.running) {
                // The host cannot block. Dropping is wrong and hanging is worse;
                // report it so the port can size the queue properly.
                __osRestoreInt(mask);
                std::fprintf(stderr,
                             "[ge-ultra] host blocked sending to a full queue; "
                             "message dropped\n");
                return -1;
            }
            g.running->state = OS_STATE_WAITING;
            enqueueAndYield(&mq->fullqueue);
        } else {
            __osRestoreInt(mask);
            return -1;
        }
    }

    const s32 last = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[last] = msg;
    mq->validCount++;
    ++g.stats.mesg_sends;

    if (mq->mtqueue != kTail && mq->mtqueue != nullptr) {
        osStartThread(popThread(&mq->mtqueue));
    }

    __osRestoreInt(mask);
    return 0;
}

extern "C" s32 osJamMesg(OSMesgQueue* mq, OSMesg msg, s32 flags) {
    const u32 mask = __osDisableInt();

    while (MQ_IS_FULL(mq)) {
        if (flags == OS_MESG_BLOCK) {
            if (!g.running) { __osRestoreInt(mask); return -1; }
            g.running->state = OS_STATE_WAITING;
            enqueueAndYield(&mq->fullqueue);
        } else {
            __osRestoreInt(mask);
            return -1;
        }
    }

    // Insert at the front rather than the back.
    mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
    mq->msg[mq->first] = msg;
    mq->validCount++;
    ++g.stats.mesg_sends;

    if (mq->mtqueue != kTail && mq->mtqueue != nullptr) {
        osStartThread(popThread(&mq->mtqueue));
    }

    __osRestoreInt(mask);
    return 0;
}

extern "C" s32 osRecvMesg(OSMesgQueue* mq, OSMesg* msg, s32 flags) {
    const u32 mask = __osDisableInt();

    while (MQ_IS_EMPTY(mq)) {
        if (flags == OS_MESG_NOBLOCK) {
            __osRestoreInt(mask);
            return -1;
        }
        if (!g.running) {
            __osRestoreInt(mask);
            return -1;
        }
        g.running->state = OS_STATE_WAITING;
        enqueueAndYield(&mq->mtqueue);
    }

    if (msg) *msg = mq->msg[mq->first];
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    ++g.stats.mesg_recvs;

    if (mq->fullqueue != kTail && mq->fullqueue != nullptr) {
        osStartThread(popThread(&mq->fullqueue));
    }

    __osRestoreInt(mask);
    return 0;
}

extern "C" void osSetEventMesg(OSEvent e, OSMesgQueue* mq, OSMesg msg) {
    if (int(e) < 0 || int(e) >= Sched::kMaxEvents) return;
    const u32 mask = __osDisableInt();
    g.event_mq[e] = mq;
    g.event_msg[e] = msg;
    __osRestoreInt(mask);
}

namespace ge_ultra {
bool sendEvent(OSEvent e) {
    if (int(e) < 0 || int(e) >= Sched::kMaxEvents) return false;
    if (!g.event_mq[e]) {
        // Counted, not ignored. "The port ran 10 frames and drew nothing" reads
        // the same whether the game never asked for retrace events or asked and
        // we failed to deliver them; this distinguishes the two.
        ++g.stats.events_undelivered;
        return false;
    }
    ++g.stats.events_sent;
    // NOBLOCK: an event whose queue is full is dropped, exactly as the hardware
    // interrupt handler would have. Blocking here would deadlock the frame loop.
    return osSendMesg(g.event_mq[e], g.event_msg[e], OS_MESG_NOBLOCK) == 0;
}
}  // namespace ge_ultra

// ---------------------------------------------------------------------------
// libultra: time and timers
// ---------------------------------------------------------------------------

extern "C" OSTime osGetTime(void) { return g.now; }
extern "C" u32 osGetCount(void) { return u32(g.now); }

extern "C" int osSetTimer(OSTimer* t, OSTime countdown, OSTime interval,
                          OSMesgQueue* mq, OSMesg msg) {
    const u32 mask = __osDisableInt();
    t->interval = interval;
    t->value = g.now + (countdown ? countdown : interval);
    t->mq = mq;
    t->msg = msg;
    t->next = g.timers;
    t->prev = nullptr;
    if (g.timers) g.timers->prev = t;
    g.timers = t;
    __osRestoreInt(mask);
    return 0;
}

extern "C" int osStopTimer(OSTimer* t) {
    const u32 mask = __osDisableInt();
    if (t->prev) t->prev->next = t->next;
    else if (g.timers == t) g.timers = t->next;
    if (t->next) t->next->prev = t->prev;
    t->next = t->prev = nullptr;
    __osRestoreInt(mask);
    return 0;
}

namespace ge_ultra {
void advanceTime(OSTime cycles) {
    g.now += cycles;

    // Fire expired timers. Collected first, then dispatched, because a timer's
    // message can unblock a thread that calls osSetTimer and mutates the list
    // we would otherwise be iterating.
    std::vector<OSTimer*> fired;
    for (OSTimer* t = g.timers; t; t = t->next)
        if (t->value <= g.now) fired.push_back(t);

    for (OSTimer* t : fired) {
        if (t->mq) osSendMesg(t->mq, t->msg, OS_MESG_NOBLOCK);
        ++g.stats.timers_fired;
        if (t->interval) t->value = g.now + t->interval;
        else osStopTimer(t);
    }
}
}  // namespace ge_ultra

// ---------------------------------------------------------------------------
// libultra: interrupt masking
// ---------------------------------------------------------------------------
//
// On N64 these wrote the interrupt controller. Here they are a nesting counter.
// Because we are cooperatively scheduled on one host thread, we cannot actually
// be preempted mid-critical-section — the counter exists so that the game's
// __osDisableInt/__osRestoreInt pairs compile and so the port can assert that
// nothing blocks inside a critical section.

extern "C" u32 __osDisableInt(void) {
    return u32(g.int_disable_depth++);
}

extern "C" void __osRestoreInt(u32 mask) {
    g.int_disable_depth = int(mask);
}

extern "C" OSIntMask osSetIntMask(OSIntMask m) {
    const OSIntMask prev = OSIntMask(g.int_disable_depth);
    g.int_disable_depth = int(m);
    return prev;
}

extern "C" OSIntMask osGetIntMask(void) { return OSIntMask(g.int_disable_depth); }


// ---------------------------------------------------------------------------
// libultra scheduler internals the game reaches into directly.
//
// src/crash.c's TLB-miss recovery path does its own enqueue:
//
//     __osEnqueueThread(&__osRunQueue, curr);
//
// On this port that path cannot trigger -- there is no TLB and no miss handler
// (see src/ultra/os_tlb.c) -- but the symbols still have to exist, and if
// anything ever does call them they must do the real thing rather than nothing.
// Both forward to the scheduler's own queue primitives, which follow libultra's
// ordering rules exactly.
// ---------------------------------------------------------------------------

extern "C" void __osEnqueueThread(OSThread **queue, OSThread *t)
{
    if (queue == nullptr || t == nullptr) {
        return;
    }
    enqueueThread(queue, t);
}


/*
 * C-callable wrapper for sendEvent().
 *
 * os_sp.c is compiled as C (it is included in the game's own build path), and
 * sendEvent lives in namespace ge_ultra. Rather than move the scheduler's
 * internals into a C header, expose the one entry point the task dispatcher
 * needs. See the completion-event note in os_sp.c.
 */
extern "C" int geSpSendEventC(int event) {
    return ge_ultra::sendEvent(static_cast<OSEvent>(event)) ? 1 : 0;
}

