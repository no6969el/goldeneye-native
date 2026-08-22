# Attaching the Phase 0 harness

Three hosts, three attach mechanisms, one harness. The harness never learns
which one it is on.

## 1. Decomp-native host (`goldeneye-native`)

Not a hook — an edit. The accessors are named, non-static, one line each.
See `adapter_decomp.c`. Ship it through `patches/`, not by hand.

## 2. Static-recompilation host, via N64Recomp's mod hooks

N64Recomp exposes function-level patching, which is the mechanism VR-PLAN §2
is betting on. The shape:

```c
#include "modding.h"
#include "ge_bind_test.h"

RECOMP_PATCH_FUNC("currentPlayerGetProjectionMatrix")
Mtx *currentPlayerGetProjectionMatrix_patched(void) {
    Mtx *m = RECOMP_ORIGINAL();          /* whatever the runtime calls this */
    geBindTestPerturbMtx((GeBindTestMtx *)m);
    return m;
}
```

Two things to confirm against the runtime you actually build, because they are
the parts most likely to differ and the parts that decide the test:

- **Is the symbol named in the recompiled output at all?** If the recompiler
  inlined or merged a one-line accessor, there is nothing to patch, and that is
  a real answer to Phase 0 — it means the four-symbol attachment surface does
  not survive recompilation, and stereo needs a different seam. Check the symbol
  map before concluding the hook "didn't work".
- **Does the pointer you receive point into the runtime's RDRAM image?** It
  should — a recomp host preserves the N64 memory layout, which is exactly why
  mutating in place is safe there.

## 3. Dynamically-linked host, via symbol interposition

For a quick answer on Linux without a mod system, if the accessor is an exported
dynamic symbol:

```c
/* build: cc -shared -fPIC -o bindtest.so interpose.c ge_bind_test.c -ldl -lm */
#define _GNU_SOURCE
#include <dlfcn.h>
#include "ge_bind_test.h"

void *currentPlayerGetProjectionMatrix(void) {
    static void *(*real)(void);
    void *m;
    if (!real) real = dlsym(RTLD_NEXT, "currentPlayerGetProjectionMatrix");
    m = real();
    geBindTestPerturbMtx((GeBindTestMtx *)m);
    return m;
}
```

Then `LD_PRELOAD=./bindtest.so ./the-host`. This only works if the host links
the game code dynamically and does not resolve the call internally first —
static linking or LTO will silently bypass you, which again reads as "the hook
did not work". Verify with `nm -D` before you trust a negative.
