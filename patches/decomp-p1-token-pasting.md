# P1 — `include/CPPLib.h`: emptiness test pastes its argument

**340 of ~1,400 host-compile errors** (275 `"_" and "0.1"`, 65 `"_" and "1.0"`).

## The problem

`include/CPPLib.h:256-258`:

```c
#define IS_EMPTY(x)  _IS_EMPTY(x)
#define _IS_EMPTY(x) IS_PROBE(CAT(_IS_EMPTY, _##x##_))
#define _IS_EMPTY__  PROBE(~) /*NULL*/
```

The emptiness test works by pasting the argument between underscores and seeing
whether the result matches `_IS_EMPTY__`. That requires `_##x##_` to be a valid
preprocessing token.

It is not, whenever the argument is a float literal:

```c
PROPFILERECORD(alarm1, 0.1)     /* -> _ ## 0.1 ## _  */
```

`_0.1` is not a valid preprocessing token — the `.` terminates the identifier —
so a conforming preprocessor rejects it. IDO was lenient and accepted it, which
is why this never surfaced in the original build.

GCC and Clang both refuse. This is not a compiler quirk to work around; the
original construct was always outside the standard.

## The fix

Use an emptiness test that never pastes the argument. This is the well-known
portable idiom (Jens Gustedt's): it probes the argument by trying to apply it as
a function-like macro and by testing for commas, and only ever pastes the
resulting `0`/`1` flags — never the argument itself.

```c
#define _ARG16(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,...) _15
#define HAS_COMMA(...) _ARG16(__VA_ARGS__,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0)
#define _TRIGGER_PARENTHESIS_(...) ,
#define PASTE5(_0,_1,_2,_3,_4) _0##_1##_2##_3##_4
#define _IS_EMPTY_CASE(_0,_1,_2,_3) HAS_COMMA(PASTE5(_IS_EMPTY_CASE_,_0,_1,_2,_3))
#define _IS_EMPTY_CASE_0001 ,

#define IS_EMPTY(...)                                                  \
    _IS_EMPTY_CASE(                                                    \
        HAS_COMMA(__VA_ARGS__),                                        \
        HAS_COMMA(_TRIGGER_PARENTHESIS_ __VA_ARGS__),                  \
        HAS_COMMA(__VA_ARGS__ (/*empty*/)),                            \
        HAS_COMMA(_TRIGGER_PARENTHESIS_ __VA_ARGS__ (/*empty*/)))
```

## Verified

Compiled and run against the cases that matter, including the two that break the
current macro:

| argument | result | expected |
|---|---|---|
| *(empty)* | 1 | 1 |
| `0.1` | 0 | 0 |
| `1.0` | 0 | 0 |
| `alarm1` | 0 | 0 |
| `0` | 0 | 0 |
| `a,b` | 0 | 0 |

## Caution before applying

`IS_EMPTY` feeds symbol-name construction elsewhere in `CPPLib.h`. The
replacement returns the same `0`/`1` values for every case above, so downstream
`CAT` chains should be unaffected — but the decomp is a **matching**
decompilation, and anything that changes generated symbol names breaks the ROM
build even while the port compiles fine.

Recommended: guard it so the N64 build is untouched.

```c
#ifdef TARGET_N64
    /* original definition */
#else
    /* portable definition above */
#endif
```

That keeps `make` byte-identical and only changes behaviour for the host port —
which is the same containment strategy `hostcompat/` uses for `stddef.h` and the
`Gfx` accessors.
