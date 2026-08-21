// test_ultra.cpp — scheduler and memory tests for the libultra shim.
//
// These assert EXACT execution orders, not "it didn't crash". That is only
// possible because the scheduler is cooperative and the clock is driven by the
// test rather than by wall time — which is the main practical argument for the
// fiber design over host threads, beyond correctness: the scheduling becomes
// testable.
//
// The behaviours checked here are the ones a "reasonable" scheduler gets wrong:
// equal priority must NOT preempt, blocked threads must wake in priority order,
// and FIFO must hold within a priority level.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gbi/gbi.h"
#include "gbi/gbi_interp.h"
#include "ultra/os.h"
#include "ultra/rdram.h"

using namespace ge_ultra;

static int g_failures = 0;
static std::vector<std::string> g_trace;

static void check(bool cond, const char* what) {
    if (!cond) { std::printf("  FAIL: %s\n", what); ++g_failures; }
    else std::printf("  ok:   %s\n", what);
}

static void checkTrace(const char* expected, const char* what) {
    std::string got;
    for (size_t i = 0; i < g_trace.size(); ++i) {
        if (i) got += ",";
        got += g_trace[i];
    }
    if (got != expected) {
        std::printf("  FAIL: %s\n        expected [%s]\n        got      [%s]\n",
                    what, expected, got.c_str());
        ++g_failures;
    } else {
        std::printf("  ok:   %s [%s]\n", what, got.c_str());
    }
}

static void trace(const char* s) { g_trace.emplace_back(s); }

namespace {
OSThread t_a, t_b, t_c;
OSMesg mbuf[8];
OSMesgQueue mq;
}  // namespace

// ---------------------------------------------------------------------------
// 1. Equal priority must NOT preempt.
//
// libultra's osStartThread compares with a strict `<`. A scheduler that used
// `<=` — or that round-robined for fairness — would interleave these two
// threads. The game relies on the non-preemption: threads at the same priority
// run to their next blocking point.
// ---------------------------------------------------------------------------
static void bodyEqualA(void*) {
    trace("A1");
    osStartThread(&t_b);   // same priority: must NOT switch
    trace("A2");
    osYieldThread();       // explicit yield: NOW b runs
    trace("A3");
    osStopThread(nullptr);
}
static void bodyEqualB(void*) {
    trace("B1");
    osYieldThread();
    trace("B2");
    osStopThread(nullptr);
}

static void testEqualPriorityNoPreempt() {
    std::printf("[equal priority does not preempt]\n");
    g_trace.clear();
    bootScheduler();

    osCreateThread(&t_a, 1, bodyEqualA, nullptr, nullptr, 10);
    osCreateThread(&t_b, 2, bodyEqualB, nullptr, nullptr, 10);
    osStartThread(&t_a);
    runUntilIdle();

    // A runs to its yield without B interrupting, then they alternate.
    checkTrace("A1,A2,B1,A3,B2", "equal-priority threads do not preempt");
    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 2. Higher priority DOES preempt, immediately, inside osStartThread.
// ---------------------------------------------------------------------------
static void bodyLow(void*) {
    trace("low1");
    osStartThread(&t_b);   // higher priority: must switch right here
    trace("low2");
    osStopThread(nullptr);
}
static void bodyHigh(void*) {
    trace("high1");
    osStopThread(nullptr);
}

static void testHigherPriorityPreempts() {
    std::printf("[higher priority preempts on start]\n");
    g_trace.clear();
    bootScheduler();

    osCreateThread(&t_a, 1, bodyLow, nullptr, nullptr, 10);
    osCreateThread(&t_b, 2, bodyHigh, nullptr, nullptr, 20);
    osStartThread(&t_a);
    runUntilIdle();

    checkTrace("low1,high1,low2", "high-priority thread runs before low resumes");
    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 3. Blocked receivers wake in priority order, FIFO within a priority.
// ---------------------------------------------------------------------------
static void bodyRecv(void* arg) {
    const char* name = static_cast<const char*>(arg);
    OSMesg m = nullptr;
    osRecvMesg(&mq, &m, OS_MESG_BLOCK);
    trace(name);
    osStopThread(nullptr);
}

static void testRecvWakeOrder() {
    std::printf("[blocked receivers wake in priority order]\n");
    g_trace.clear();
    bootScheduler();
    osCreateMesgQueue(&mq, mbuf, 8);

    // Started low-to-high so that arrival order and priority order disagree —
    // if the queue were FIFO-only, the trace would come out "lo,mid,hi".
    static char n_lo[] = "lo", n_mid[] = "mid", n_hi[] = "hi";
    osCreateThread(&t_a, 1, bodyRecv, n_lo, nullptr, 10);
    osCreateThread(&t_b, 2, bodyRecv, n_mid, nullptr, 20);
    osCreateThread(&t_c, 3, bodyRecv, n_hi, nullptr, 30);
    osStartThread(&t_a); runUntilIdle();
    osStartThread(&t_b); runUntilIdle();
    osStartThread(&t_c); runUntilIdle();

    check(g_trace.empty(), "all three receivers are blocked");

    // Three sends from the host context; each should wake exactly one, highest
    // priority first.
    osSendMesg(&mq, (OSMesg)1, OS_MESG_NOBLOCK); runUntilIdle();
    osSendMesg(&mq, (OSMesg)2, OS_MESG_NOBLOCK); runUntilIdle();
    osSendMesg(&mq, (OSMesg)3, OS_MESG_NOBLOCK); runUntilIdle();

    checkTrace("hi,mid,lo", "receivers wake highest-priority-first");
    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 4. Message queue mechanics: ring wrap, NOBLOCK failure, jam ordering.
// ---------------------------------------------------------------------------
static void testMesgQueueMechanics() {
    std::printf("[message queue mechanics]\n");
    bootScheduler();

    OSMesg buf[3];
    OSMesgQueue q;
    osCreateMesgQueue(&q, buf, 3);

    check(osSendMesg(&q, (OSMesg)1, OS_MESG_NOBLOCK) == 0, "send 1 of 3");
    check(osSendMesg(&q, (OSMesg)2, OS_MESG_NOBLOCK) == 0, "send 2 of 3");
    check(osSendMesg(&q, (OSMesg)3, OS_MESG_NOBLOCK) == 0, "send 3 of 3");
    check(osSendMesg(&q, (OSMesg)4, OS_MESG_NOBLOCK) == -1,
          "send to full queue returns -1 under NOBLOCK");

    OSMesg m = nullptr;
    osRecvMesg(&q, &m, OS_MESG_NOBLOCK);
    check(m == (OSMesg)1, "FIFO order preserved");

    // Ring wrap: with one slot freed, the next send must land at index 0.
    check(osSendMesg(&q, (OSMesg)4, OS_MESG_NOBLOCK) == 0, "send after recv wraps");
    osRecvMesg(&q, &m, OS_MESG_NOBLOCK); check(m == (OSMesg)2, "wrap: got 2");
    osRecvMesg(&q, &m, OS_MESG_NOBLOCK); check(m == (OSMesg)3, "wrap: got 3");
    osRecvMesg(&q, &m, OS_MESG_NOBLOCK); check(m == (OSMesg)4, "wrap: got 4");
    check(osRecvMesg(&q, &m, OS_MESG_NOBLOCK) == -1,
          "recv from empty queue returns -1 under NOBLOCK");

    // osJamMesg inserts at the FRONT. The game uses it for priority events.
    osSendMesg(&q, (OSMesg)10, OS_MESG_NOBLOCK);
    osJamMesg(&q, (OSMesg)99, OS_MESG_NOBLOCK);
    osRecvMesg(&q, &m, OS_MESG_NOBLOCK);
    check(m == (OSMesg)99, "osJamMesg jumps the queue");

    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 5. Timers fire on the virtual clock, and repeat.
// ---------------------------------------------------------------------------
static void testTimers() {
    std::printf("[timers on the virtual clock]\n");
    bootScheduler();

    OSMesg buf[8];
    OSMesgQueue q;
    osCreateMesgQueue(&q, buf, 8);

    OSTimer one_shot, repeating;
    osSetTimer(&one_shot, 100, 0, &q, (OSMesg)1);
    osSetTimer(&repeating, 50, 50, &q, (OSMesg)2);

    OSMesg m = nullptr;
    advanceTime(49);
    check(osRecvMesg(&q, &m, OS_MESG_NOBLOCK) == -1, "nothing fires before its time");

    advanceTime(1);  // t=50
    check(osRecvMesg(&q, &m, OS_MESG_NOBLOCK) == 0 && m == (OSMesg)2,
          "repeating timer fires at t=50");

    advanceTime(50); // t=100: both
    int got1 = 0, got2 = 0;
    while (osRecvMesg(&q, &m, OS_MESG_NOBLOCK) == 0) {
        if (m == (OSMesg)1) ++got1;
        if (m == (OSMesg)2) ++got2;
    }
    check(got1 == 1 && got2 == 1, "both timers fire at t=100");

    advanceTime(50); // t=150
    got1 = got2 = 0;
    while (osRecvMesg(&q, &m, OS_MESG_NOBLOCK) == 0) {
        if (m == (OSMesg)1) ++got1;
        if (m == (OSMesg)2) ++got2;
    }
    check(got1 == 0, "one-shot timer does not repeat");
    check(got2 == 1, "repeating timer repeats");

    osStopTimer(&repeating);
    advanceTime(100);
    check(osRecvMesg(&q, &m, OS_MESG_NOBLOCK) == -1, "osStopTimer stops it");

    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 6. Events — how the port injects "retrace happened".
// ---------------------------------------------------------------------------
static void bodyWaitVi(void*) {
    OSMesg m = nullptr;
    for (int i = 0; i < 3; ++i) {
        osRecvMesg(&mq, &m, OS_MESG_BLOCK);
        trace("frame");
    }
    osStopThread(nullptr);
}

static void testEventDelivery() {
    std::printf("[event delivery drives the frame loop]\n");
    g_trace.clear();
    bootScheduler();
    osCreateMesgQueue(&mq, mbuf, 8);

    osSetEventMesg(7 /* OS_EVENT_VI */, &mq, (OSMesg)0x5649);
    osCreateThread(&t_a, 1, bodyWaitVi, nullptr, nullptr, 10);
    osStartThread(&t_a);
    runUntilIdle();
    check(g_trace.empty(), "thread blocks waiting for retrace");

    for (int i = 0; i < 3; ++i) { sendEvent(7); runUntilIdle(); }
    checkTrace("frame,frame,frame", "three retraces produce three frames");

    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 7. Priority changes re-sort the run queue.
// ---------------------------------------------------------------------------
static void bodyPriA(void*) {
    trace("a1");
    osSetThreadPri(nullptr, 5);   // drop below B: must yield to it
    trace("a2");
    osStopThread(nullptr);
}
static void bodyPriB(void*) { trace("b1"); osStopThread(nullptr); }

static void testPriorityChange() {
    std::printf("[osSetThreadPri re-sorts]\n");
    g_trace.clear();
    bootScheduler();

    osCreateThread(&t_a, 1, bodyPriA, nullptr, nullptr, 20);
    osCreateThread(&t_b, 2, bodyPriB, nullptr, nullptr, 10);
    osStartThread(&t_b);   // runnable, lower priority, does not run yet
    osStartThread(&t_a);
    runUntilIdle();

    checkTrace("a1,b1,a2", "lowering own priority yields to the higher thread");
    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 8. RDRAM: the physical-address contract the interpreter depends on.
// ---------------------------------------------------------------------------
static void testRdram() {
    std::printf("[flat RDRAM and physical addressing]\n");
    check(rdramInit(), "RDRAM allocated");
    rdramResetStats();

    uint8_t* base = rdramBase();
    check(base != nullptr, "base pointer valid");
    // 8 MB for the game plus the stack region above it. The game's own
    // allocator runs to 0x807FE000, so the thread stacks -- which must be in
    // RDRAM for osVirtualToPhysical to translate a stack matrix into a display
    // list -- are mapped above that rather than carved out of it. See
    // kStackRegionBase in src/ultra/rdram.h.
    check(rdramSize() == kGameRdramSize + kStackRegionSize,
          "size is the game's 8 MB plus the stack region");
    check(kStackRegionBase == 8u * 1024u * 1024u,
          "the stack region starts above everything the game can allocate");

    // Round trip.
    void* p = base + 0x12340;
    const uint32_t phys = virtualToPhysical(p);
    check(phys == 0x12340, "virtual -> physical is an offset");
    check(physicalToVirtual(phys) == p, "physical -> virtual round-trips");

    // Out-of-range pointers must fail loudly, not produce a plausible address.
    int stack_var = 0;
    check(virtualToPhysical(&stack_var) == 0, "non-RDRAM pointer yields 0");
    check(virtualToPhysical(nullptr) == 0, "null yields 0");
    check(physicalToVirtual(0) == nullptr, "physical 0 is not resolvable");
    check(physicalToVirtual(rdramSize() - 4, 8) == nullptr,
          "read straddling the end is refused");
    // Exactly two: the stack pointer and the straddling read. Translating null
    // is legitimate — game code does it for absent optional pointers — so it is
    // deliberately not counted, otherwise the real failures drown in noise.
    check(rdramBadResolveCount() == 2, "bad translations counted, null excluded");

    // The whole point: a display list built in RDRAM, addressed by physical
    // address, must be walkable by the interpreter. This is the contract that
    // fails silently if RDRAM is not one flat buffer.
    rdramResetStats();
    struct Sink : ge_gbi::IDrawSink {
        int tris = 0;
        void drawTriangle(const ge_gbi::Tri&,
                          const ge_gbi::CachedVtx[ge_gbi::kVertexCacheSize]) override {
            ++tris;
        }
        void setProjection(const float[4][4]) override {}
        void setModelview(const float[4][4]) override {}
        void setGeometryMode(uint32_t) override {}
        void setOtherModeH(uint32_t) override {}
        void setOtherModeL(uint32_t) override {}
        void setTexture(const ge_gbi::TextureState&) override {}
        void rdpCommand(uint32_t, uint32_t) override {}
    } sink;

    // Four vertices, then the real glass2.c quad, then G_ENDDL.
    auto* verts = reinterpret_cast<ge_gbi::Vtx*>(base + 0x20000);
    for (int i = 0; i < 4; ++i) { verts[i] = {}; verts[i].x = int16_t(i); }
    const uint32_t vphys = virtualToPhysical(verts);

    auto* dl = reinterpret_cast<ge_gbi::Gfx*>(base + 0x30000);
    dl[0] = {(uint32_t(ge_gbi::G_VTX) << 24) | (uint32_t((3 << 4) | 0) << 16) | 64, vphys};
    dl[1] = {0xB1000032u, 0x00002110u};
    dl[2] = {uint32_t(ge_gbi::G_ENDDL) << 24, 0};

    ge_gbi::Interpreter interp(rdramResolver(), &sink);
    const bool ok = interp.run(virtualToPhysical(dl));

    check(ok, "interpreter walks a list living in RDRAM");
    check(sink.tris == 2, "the quad rendered through real physical addresses");
    check(rdramBadResolveCount() == 0, "no failed resolves");

    rdramShutdown();
}

// ---------------------------------------------------------------------------
// 9. Six threads, the way the game actually starts them.
// ---------------------------------------------------------------------------
static void testGameLikeStartup() {
    std::printf("[game-like startup: idle + main + sched + audio]\n");
    g_trace.clear();
    bootScheduler();
    osCreateMesgQueue(&mq, mbuf, 8);

    static OSThread idle, main_t, sched, audio;

    // Priorities mirror src/init.c and friends: idle at the bottom, VI manager
    // territory at the top.
    osCreateThread(&idle, 1, [](void*) {
        // The idle thread must never block. On N64 it spins at priority 0.
        for (int i = 0; i < 3; ++i) { trace("idle"); osYieldThread(); }
        osStopThread(nullptr);
    }, nullptr, nullptr, OS_PRIORITY_IDLE);

    osCreateThread(&main_t, 2, [](void*) {
        OSMesg m = nullptr;
        for (int i = 0; i < 2; ++i) { osRecvMesg(&mq, &m, OS_MESG_BLOCK); trace("main"); }
        osStopThread(nullptr);
    }, nullptr, nullptr, 10);

    osCreateThread(&sched, 3, [](void*) {
        for (int i = 0; i < 2; ++i) { trace("sched"); osYieldThread(); }
        osStopThread(nullptr);
    }, nullptr, nullptr, OS_PRIORITY_VIMGR);

    osCreateThread(&audio, 4, [](void*) {
        for (int i = 0; i < 2; ++i) { trace("audio"); osYieldThread(); }
        osStopThread(nullptr);
    }, nullptr, nullptr, 12);

    osStartThread(&idle);
    osStartThread(&main_t);
    osStartThread(&audio);
    osStartThread(&sched);
    runUntilIdle();

    // sched (254) > audio (12) > idle (0); main is blocked on the queue
    // throughout.
    //
    // Note this is NOT round-robin. osYieldThread re-enqueues the caller at the
    // tail of its own priority level — and sched is alone at 254, so yielding
    // puts it straight back at the head of the run queue and it runs again.
    // Only when sched stops does audio get the CPU at all.
    //
    // That is correct N64 behaviour and it is the whole reason the game's
    // threads are structured around blocking on message queues rather than
    // yielding: a yield between unequal priorities accomplishes nothing.
    checkTrace("sched,sched,audio,audio,idle,idle,idle",
               "yield does not hand off across priority levels");

    g_trace.clear();
    osSendMesg(&mq, (OSMesg)1, OS_MESG_NOBLOCK); runUntilIdle();
    osSendMesg(&mq, (OSMesg)2, OS_MESG_NOBLOCK); runUntilIdle();
    checkTrace("main,main", "blocked main thread wakes on each message");

    check(schedStats().context_switches > 0, "context switches recorded");
    shutdownScheduler();
}

int main() {
    testEqualPriorityNoPreempt();
    testHigherPriorityPreempts();
    testRecvWakeOrder();
    testMesgQueueMechanics();
    testTimers();
    testEventDelivery();
    testPriorityChange();
    testRdram();
    testGameLikeStartup();

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nall ultra tests passed\n");
    return 0;
}
