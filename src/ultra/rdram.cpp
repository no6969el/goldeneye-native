#include "rdram.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <unistd.h>
#  if defined(__linux__)
#    include <sys/syscall.h>
#  endif
#endif

#include <cstdio>

#include <atomic>
#include <cstdlib>
#include <cstring>

namespace ge_ultra {
namespace {
uint8_t* g_base = nullptr;
void* g_kseg1 = nullptr;
void* g_tlb_code = nullptr;
void* g_tlb_game = nullptr;
#if defined(_WIN32)
void* g_mapping = nullptr;
#endif
std::atomic<uint64_t> g_bad_resolves{0};
}  // namespace

/*
 * WHERE RDRAM LIVES, AND WHY THE ADDRESS IS NOT ARBITRARY
 *
 * The original allocation here was an ordinary aligned_alloc: 8 MB, wherever
 * the allocator felt like putting it, with physical addresses computed as
 * offsets from the base. That is enough for the display-list interpreter, which
 * translates every address it reads. It is NOT enough for the game itself.
 *
 * src/init.c, the first function the game runs:
 *
 *     csegmentSegmentVaddrStart = get_csegmentSegmentStart();   // 0x80020D90
 *     dataziprom = csegmentSegmentVaddrStart;
 *     for (j = copylen - 1; j >= 0; j--)
 *         datazipram[j] = dataziprom[j];                        // dereferenced
 *
 * The game takes an N64 KSEG0 address, puts it in a `u8 *`, and dereferences
 * it. There are hundreds of sites like this and no realistic way to find them
 * all -- and each one that is missed is a wild pointer, not a compile error.
 *
 * So the port maps RDRAM AT the address the game expects: KSEG0 (0x80000000).
 * Then an N64 address IS a valid host address, every one of those sites works
 * untouched, and OS_K0_TO_PHYSICAL becomes the mask it was on hardware.
 *
 * KSEG1 (0xA0000000) is the same memory seen uncached. The game uses it for
 * anything the RCP also reads, so it has to ALIAS -- two independent 8 MB
 * blocks would silently give the CPU and the "RSP" different views of the same
 * display list, which is exactly the kind of bug that presents as random
 * corruption. A shared mapping is used so both windows are the same pages.
 *
 * If the fixed mapping cannot be obtained, this fails loudly rather than
 * falling back to a floating allocation. A fallback would run, and would crash
 * later somewhere unrelated.
 */


namespace {
constexpr uintptr_t kKseg0Base = 0x80000000u;
constexpr uintptr_t kKseg1Base = 0xA0000000u;

/*
 * THE TLB WINDOWS
 *
 * GoldenEye does not live entirely in KSEG0. It TLB-maps two more regions, and
 * the very first thing init() does is write into one of them:
 *
 *     datazipram = (u8 *)(RZIPLOADADDR - cdataSegmentRomSize);   // ~0x70200000
 *     for (j = copylen - 1; j >= 0; j--)
 *         datazipram[j] = dataziprom[j];
 *
 * Mapping only KSEG0 got the port as far as init() and then faulted on a byte
 * store to 0x7020159F -- inside the inflate segment. The claim in os_tlb.c that
 * "the port maps every address directly" was therefore incomplete: it mapped
 * every address the game reaches through RDRAM, and none of the ones it reaches
 * through the TLB.
 *
 * Extents come from the segment table (hostcompat/ge_segments.h):
 *
 *     0x70000450 .. 0x702015A0   code, alt_start, inflate
 *     0x7F000000 .. 0x7F0E2D50   game
 *
 * Rounded up to 16 MB each, which costs nothing on a host and leaves room for
 * the segments the table does not name.
 *
 * HOW FAITHFUL IS THIS?
 *
 * Not entirely, and it is worth being precise about where it differs. On
 * hardware these virtual pages are mapped ONTO physical RDRAM, so a write
 * through 0x7F000000 is visible at the corresponding KSEG0 address. Here they
 * are separate memory, so that aliasing does not happen.
 *
 * That is fine as long as the game reaches each segment through one view only,
 * which is how tlb_manage.c uses them. If something turns out to depend on the
 * aliasing, the symptom will be data that was written and then read back as
 * zero -- and the fix is a real page table here, not a bigger window.
 */
constexpr uintptr_t kTlbCodeBase = 0x70000000u;
constexpr uintptr_t kTlbGameBase = 0x7F000000u;
constexpr size_t    kTlbWindow   = 16u * 1024u * 1024u;
}  // namespace

namespace {

// Maps one fixed window. Returns nullptr on failure; the caller decides whether
// that is fatal.
void* mapFixedWindow(uintptr_t at, size_t len) {
#if defined(_WIN32)
    return VirtualAlloc(reinterpret_cast<void*>(at), len,
                        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* p = mmap(reinterpret_cast<void*>(at), len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED || p != reinterpret_cast<void*>(at)) {
        if (p != MAP_FAILED) munmap(p, len);
        return nullptr;
    }
    return p;
#endif
}

}  // namespace

bool rdramInit() {
    if (g_base) return true;

#if defined(_WIN32)
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                        PAGE_READWRITE, 0, kRdramSize, nullptr);
    if (!mapping) {
        std::fprintf(stderr, "[ge-ultra] RDRAM: CreateFileMapping failed\n");
        return false;
    }
    void* k0 = MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, kRdramSize,
                               reinterpret_cast<void*>(kKseg0Base));
    if (!k0) {
        std::fprintf(stderr,
                     "[ge-ultra] RDRAM: could not map 8 MB at 0x%08llX.\n"
                     "  The game dereferences N64 KSEG0 addresses directly, so\n"
                     "  this address is required, not preferred.\n",
                     (unsigned long long)kKseg0Base);
        CloseHandle(mapping);
        return false;
    }
    // KSEG1: the same pages, uncached view. Failure is not fatal -- code that
    // uses it will fault visibly rather than read a stale copy.
    g_kseg1 = MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, kRdramSize,
                              reinterpret_cast<void*>(kKseg1Base));
    g_mapping = mapping;
    g_base = static_cast<uint8_t*>(k0);
#else
    // A shared anonymous mapping, so the KSEG1 alias below is the same memory.
    int fd = -1;
#  if defined(__linux__) && defined(SYS_memfd_create)
    fd = int(syscall(SYS_memfd_create, "ge-rdram", 0u));
    if (fd >= 0 && ftruncate(fd, kRdramSize) != 0) {
        close(fd);
        fd = -1;
    }
#  endif

    const int flags = MAP_SHARED | (fd < 0 ? MAP_ANONYMOUS : 0);
    void* k0 = mmap(reinterpret_cast<void*>(kKseg0Base), kRdramSize,
                    PROT_READ | PROT_WRITE, flags | MAP_FIXED_NOREPLACE, fd, 0);
    if (k0 == MAP_FAILED || k0 != reinterpret_cast<void*>(kKseg0Base)) {
        if (k0 != MAP_FAILED) munmap(k0, kRdramSize);
        std::fprintf(stderr,
                     "[ge-ultra] RDRAM: could not map 8 MB at 0x%08lX.\n"
                     "  The game dereferences N64 KSEG0 addresses directly\n"
                     "  (src/init.c does it on the first line it runs), so this\n"
                     "  address is required, not preferred. Refusing to fall\n"
                     "  back to a floating allocation: it would run, and crash\n"
                     "  later somewhere unrelated.\n",
                     (unsigned long)kKseg0Base);
        if (fd >= 0) close(fd);
        return false;
    }

    if (fd >= 0) {
        void* k1 = mmap(reinterpret_cast<void*>(kKseg1Base), kRdramSize,
                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED_NOREPLACE,
                        fd, 0);
        g_kseg1 = (k1 == MAP_FAILED) ? nullptr : k1;
        close(fd);
    }

    g_base = static_cast<uint8_t*>(k0);
#endif

    if (!g_kseg1) {
        std::fprintf(stderr,
                     "[ge-ultra] RDRAM: no KSEG1 alias at 0x%08lX. Uncached\n"
                     "  accesses will fault rather than read a stale copy.\n",
                     (unsigned long)kKseg1Base);
    }

    // The TLB-mapped segments. Fatal if missing: the game writes into them
    // before it does anything else, so continuing would just crash later with
    // less information.
    g_tlb_code = mapFixedWindow(kTlbCodeBase, kTlbWindow);
    g_tlb_game = mapFixedWindow(kTlbGameBase, kTlbWindow);
    if (!g_tlb_code || !g_tlb_game) {
        std::fprintf(stderr,
                     "[ge-ultra] RDRAM: could not map the TLB windows at\n"
                     "  0x%08lX / 0x%08lX. init() writes into the first of\n"
                     "  these on its very first loop.\n",
                     (unsigned long)kTlbCodeBase, (unsigned long)kTlbGameBase);
        return false;
    }

    // Zero, not poison: the game assumes .bss is zero and carves its heaps out
    // of this. Poisoning would be better for catching use-before-init, but it
    // breaks the zero-initialisation assumption in a way that is hard to
    // distinguish from a real bug.
    std::memset(g_base, 0, kRdramSize);
    std::memset(g_tlb_code, 0, kTlbWindow);
    std::memset(g_tlb_game, 0, kTlbWindow);
    return true;
}

void rdramShutdown() {
    if (!g_base) return;
#if defined(_WIN32)
    UnmapViewOfFile(g_base);
    if (g_kseg1) UnmapViewOfFile(g_kseg1);
    if (g_mapping) CloseHandle(g_mapping);
    g_mapping = nullptr;
#else
    munmap(g_base, kRdramSize);
    if (g_kseg1) munmap(g_kseg1, kRdramSize);
#endif
    g_kseg1 = nullptr;
    g_base = nullptr;
}

uint8_t* rdramBase() { return g_base; }
uint32_t rdramSize() { return kRdramSize; }

uint32_t virtualToPhysical(const void* p) {
    if (!p || !g_base) return 0;
    const auto* c = static_cast<const uint8_t*>(p);
    if (c < g_base || c >= g_base + kRdramSize) {
        // A pointer that isn't in RDRAM cannot be expressed as a physical
        // address. Returning 0 makes it a resolve failure downstream rather
        // than a plausible-looking wrong address.
        g_bad_resolves.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    return uint32_t(c - g_base);
}

void* physicalToVirtual(uint32_t addr, size_t len) {
    if (!g_base) return nullptr;

    // Physical 0 is a translated null pointer. Game code translates null
    // routinely (an absent optional pointer), so this is NOT counted as a bad
    // translation — counting it would bury the real failures in noise.
    if (addr == 0) return nullptr;

    if (addr < kReservedLowPage || uint64_t(addr) + len > kRdramSize) {
        g_bad_resolves.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    return g_base + addr;
}

std::function<const void*(uint32_t, size_t)> rdramResolver() {
    // Counting happens inside physicalToVirtual so every failed translation is
    // recorded exactly once, at the source, regardless of which path reached it.
    return [](uint32_t addr, size_t len) -> const void* {
        return physicalToVirtual(addr, len);
    };
}

uint64_t rdramBadResolveCount() {
    return g_bad_resolves.load(std::memory_order_relaxed);
}

void rdramResetStats() { g_bad_resolves.store(0, std::memory_order_relaxed); }

}  // namespace ge_ultra

// libultra entry point, kept at its original signature.
extern "C" uint32_t osVirtualToPhysical(void* p) {
    return ge_ultra::virtualToPhysical(p);
}
