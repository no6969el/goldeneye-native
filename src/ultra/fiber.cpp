#include "fiber.h"

#include "rdram.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <csignal>
#  include <sys/mman.h>
#  include <ucontext.h>
// glibc 2.34+ made SIGSTKSZ a runtime sysconf() call rather than a constant, so
// it can no longer initialise a constexpr. Pin a floor instead.
#  ifndef GE_FIBER_MIN_STACK
#    define GE_FIBER_MIN_STACK (128u * 1024u)
#  endif
#endif

namespace ge_ultra {

namespace {
// The fiber being started. makecontext() cannot portably pass a pointer-sized
// argument (its varargs are ints), so the trampoline picks the target up from
// here. Safe because a start is never interrupted: we set it and switch
// immediately, on one host thread.
Fiber* g_starting = nullptr;
}  // namespace

#if defined(_WIN32)

struct Fiber::Impl {
    LPVOID handle = nullptr;
    Fiber::EntryFn entry = nullptr;
    void* arg = nullptr;
};

static VOID CALLBACK winFiberProc(PVOID param) {
    auto* f = static_cast<Fiber*>(param);
    f->impl_->entry(f->impl_->arg);
    f->finished_ = true;
    // A fiber's entry must never return without something switching away. If we
    // get here the scheduler has a bug; spin back to the host rather than
    // corrupting the stack.
    std::fprintf(stderr, "[ge-ultra] fiber entry returned; scheduler bug\n");
    std::abort();
}

Fiber* Fiber::createForCurrentContext() {
    auto* f = new Fiber();
    f->impl_ = new Impl();
    f->is_host_context_ = true;
    f->impl_->handle = ConvertThreadToFiber(nullptr);
    if (!f->impl_->handle) {
        // Already a fiber (e.g. nested init) — GetCurrentFiber is then valid.
        f->impl_->handle = GetCurrentFiber();
    }
    return f;
}

Fiber* Fiber::create(EntryFn entry, void* arg, size_t stack_size) {
    auto* f = new Fiber();
    f->impl_ = new Impl();
    f->impl_->entry = entry;
    f->impl_->arg = arg;
    f->impl_->handle = CreateFiber(stack_size, winFiberProc, f);
    if (!f->impl_->handle) { delete f->impl_; delete f; return nullptr; }
    return f;
}

void Fiber::switchTo(Fiber* from, Fiber* to) {
    (void)from;
    SwitchToFiber(to->impl_->handle);
}

Fiber::~Fiber() {
    if (impl_) {
        if (!is_host_context_ && impl_->handle) DeleteFiber(impl_->handle);
        delete impl_;
    }
}

#else  // POSIX

struct Fiber::Impl {
    ucontext_t ctx{};
    char* stack = nullptr;
    size_t stack_size = 0;
    Fiber::EntryFn entry = nullptr;
    void* arg = nullptr;
};

/*
 * Thread stacks live IN RDRAM.
 *
 * Two problems, one answer.
 *
 * 1. DETERMINISM. malloc put the stacks wherever ASLR felt like, and the same
 *    binary on the same ROM died at frame 7490, 4960 and 4930 on three
 *    consecutive runs. Disabling ASLR made it identical twice, which is how the
 *    address was identified as the variable.
 *
 * 2. CORRECTNESS, which is the real reason. The renderer takes `Mtxf` and
 *    `ModelRenderData` off the stack and writes their addresses into display
 *    lists -- `gSPMatrix(gdl++, osVirtualToPhysical(mtx))` with mtx a local.
 *    That works on the N64 because the thread stacks ARE in RDRAM. A stack
 *    outside it makes osVirtualToPhysical return 0 and the RSP read physical
 *    zero; worse, the game also feeds such addresses through segment
 *    resolution, which is how a stack pointer came back as 0x3507FEB7 -- 0x05
 *    in the segment nibble and the rest intact.
 *
 * The game's own allocator runs from 0x8008E360 to 0x807FE000, so there is no
 * room inside the 8 MB it knows about. ge_ultra::kStackRegionBase is four extra
 * megabytes mapped ABOVE that, inside the same RDRAM allocation and therefore
 * inside every bounds check, but above anything the game's heap can reach.
 *
 * Bump-allocated with a guard page between stacks, and never reused: an address
 * identifies one stack for the life of the process, so a dangling pointer to a
 * dead thread's frame stays diagnosable instead of quietly naming a live one.
 */
namespace {
uint32_t g_stack_next = kStackRegionBase;

void* allocStack(size_t size) {
    constexpr size_t kPage = 4096;
    const size_t rounded = (size + kPage - 1) & ~(kPage - 1);

    if (uint64_t(g_stack_next) + rounded + kPage > kRdramSize) {
        std::fprintf(stderr,
                     "[ge-ultra] fiber stacks: the %u KB stack region is full "
                     "at %zu bytes.\n  Raise kStackRegionSize in "
                     "src/ultra/rdram.h.\n",
                     unsigned(kStackRegionSize / 1024), rounded);
        return nullptr;
    }
    uint8_t* base = rdramBase();
    if (base == nullptr) {
        /*
         * No RDRAM: the scheduler unit tests exercise threads on their own,
         * without mapping the game's memory. Fall back to an ordinary
         * allocation and SAY SO, once.
         *
         * The fallback is only safe because nothing in those tests puts a stack
         * address into a display list -- which is the whole reason the real
         * stacks have to be inside RDRAM. Failing outright here would make the
         * scheduler untestable in isolation; failing silently would let the
         * port itself run on host stacks again if rdramInit were ever skipped,
         * so the warning is not optional.
         */
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr,
                         "[ge-ultra] fiber stacks: RDRAM is not mapped, using "
                         "host memory.\n  Fine for the scheduler tests; in the "
                         "port it means rdramInit() did not run first, and\n"
                         "  stack addresses will not survive a display list.\n");
        }
        void* h = std::malloc(rounded);
        return h;
    }
    void* p = base + g_stack_next;
    g_stack_next += uint32_t(rounded + kPage);   /* the gap is the guard */
    return p;
}
}  // namespace

void fiberTrampoline() {
    Fiber* f = g_starting;
    g_starting = nullptr;
    f->impl_->entry(f->impl_->arg);
    f->finished_ = true;
    std::fprintf(stderr, "[ge-ultra] fiber entry returned; scheduler bug\n");
    std::abort();
}

Fiber* Fiber::createForCurrentContext() {
    auto* f = new Fiber();
    f->impl_ = new Impl();
    f->is_host_context_ = true;
    getcontext(&f->impl_->ctx);
    return f;
}

Fiber* Fiber::create(EntryFn entry, void* arg, size_t stack_size) {
    if (stack_size < GE_FIBER_MIN_STACK) stack_size = GE_FIBER_MIN_STACK;

    auto* f = new Fiber();
    f->impl_ = new Impl();
    f->impl_->entry = entry;
    f->impl_->arg = arg;
    f->impl_->stack_size = stack_size;
    f->impl_->stack = static_cast<char*>(allocStack(stack_size));
    if (!f->impl_->stack) { delete f->impl_; delete f; return nullptr; }

    // Poison the stack. The game's threads have fixed stack sizes chosen for a
    // 1997 compiler; a modern one can use meaningfully more. Poisoning lets
    // stackHighWater() report actual usage instead of us discovering an
    // overflow as memory corruption three subsystems away.
    std::memset(f->impl_->stack, 0xCD, stack_size);

    getcontext(&f->impl_->ctx);
    f->impl_->ctx.uc_stack.ss_sp = f->impl_->stack;
    f->impl_->ctx.uc_stack.ss_size = stack_size;
    f->impl_->ctx.uc_link = nullptr;  // returning is a bug; trampoline aborts
    makecontext(&f->impl_->ctx, reinterpret_cast<void (*)()>(fiberTrampoline), 0);
    return f;
}

void Fiber::switchTo(Fiber* from, Fiber* to) {
    g_starting = to;
    swapcontext(&from->impl_->ctx, &to->impl_->ctx);
}

Fiber::~Fiber() {
    if (impl_) {
        // Nothing to release: the stack is a slice of the RDRAM mapping, and
        // the slice is deliberately not recycled. See allocStack.
        delete impl_;
    }
}

#endif

uintptr_t Fiber::resumePc() const {
#if defined(_WIN32)
    // No portable read of a suspended fiber's IP; the Win32 fiber context is
    // opaque. Reported as unavailable rather than guessed.
    return 0;
#elif defined(__x86_64__) && defined(REG_RIP)
    if (impl_ == nullptr) return 0;
    return uintptr_t(impl_->ctx.uc_mcontext.gregs[REG_RIP]);
#elif defined(__aarch64__)
    if (impl_ == nullptr) return 0;
    return uintptr_t(impl_->ctx.uc_mcontext.pc);
#else
    return 0;
#endif
}

void Fiber::destroy(Fiber* f) { delete f; }

}  // namespace ge_ultra
