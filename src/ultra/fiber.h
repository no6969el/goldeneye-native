// fiber.h — cooperative context switching.
//
// Why fibers and not host threads:
//
// GoldenEye runs six threads under N64 priority scheduling, which was
// effectively deterministic — a thread ran until it blocked or something
// higher-priority became runnable, and switches happened only at known points.
// Nintendo 64 libultra had no time-slicing between equal-priority threads.
//
// Map that onto preemptive host threads and every latent race in a 1997
// codebase becomes live, intermittently, on a machine with 16 cores and a
// different memory model. Those bugs do not reproduce and they do not stay
// fixed.
//
// Fibers reproduce the original scheduling instead of approximating it: one
// host thread, switches at exactly the points libultra would have switched.
// Slower in principle; the game targets 20-30 Hz simulation, so it does not
// matter in practice.

#pragma once

#include <cstddef>
#include <cstdint>

namespace ge_ultra {

class Fiber {
public:
    using EntryFn = void (*)(void* arg);

    Fiber() = default;
    ~Fiber();

    Fiber(const Fiber&) = delete;
    Fiber& operator=(const Fiber&) = delete;

    // Wrap the currently-executing context so it can be switched back to.
    // Exactly one of these must exist per host thread, created before any
    // switchTo().
    static Fiber* createForCurrentContext();

    // Create a fiber that will run entry(arg) when first switched to.
    // stack_size is rounded up to the platform minimum.
    static Fiber* create(EntryFn entry, void* arg, size_t stack_size);

    static void destroy(Fiber* f);

    // Switch execution from `from` to `to`. Returns in `from` when something
    // switches back to it.
    static void switchTo(Fiber* from, Fiber* to);

    // True once the entry function has returned. A fiber whose entry returns is
    // dead; switching to it again is a bug, and the implementation traps rather
    // than running off the end of the stack.
    bool finished() const { return finished_; }

    // The instruction pointer this fiber will resume at, or 0 if unavailable.
    //
    // This is the only way to find out where a blocked thread is. Six libultra
    // threads share one host thread here, so a debugger attached to the process
    // sees exactly one stack -- whichever fiber happens to be running -- and
    // the five that are actually stuck are invisible to it. That is precisely
    // backwards from what a hang needs. Feed the value to addr2line.
    uintptr_t resumePc() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool finished_ = false;
    bool is_host_context_ = false;

    friend void fiberTrampoline();
};

}  // namespace ge_ultra
