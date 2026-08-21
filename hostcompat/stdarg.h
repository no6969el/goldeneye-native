/*
 * hostcompat/stdarg.h — host build shim, same job as hostcompat/stddef.h.
 *
 * The decomp ships include/stdarg.h for the IRIX toolchain. Under GCC it does:
 *
 *     #define va_list __builtin_va_list
 *
 * which covers the game's own use but leaves out everything the host C library
 * expects from a real <stdarg.h>. Because the decomp's include directory is on
 * the search path, that stub SHADOWS the compiler's header — so the moment any
 * translation unit includes <stdio.h>, glibc reaches for `__gnuc_va_list`,
 * finds nothing, and fails:
 *
 *     /usr/include/stdio.h:53: unknown type name '__gnuc_va_list'
 *
 * The symptom is confusing because the failing header is the system's, and
 * nothing in the file being compiled mentions varargs at all. The cause is an
 * include-path collision several headers away.
 *
 * This provides the real thing from compiler builtins — no vendored licensed
 * header, same approach as hostcompat/stddef.h — and sets the guards the C
 * libraries use so they do not then typedef va_list a second time.
 */
#ifndef GE_HOSTCOMPAT_STDARG_H
#define GE_HOSTCOMPAT_STDARG_H

/* What glibc's headers actually ask for. */
#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
typedef __builtin_va_list __gnuc_va_list;
#endif

/*
 * A typedef rather than the decomp's `#define va_list __builtin_va_list`: a
 * macro would rewrite the C library's own `typedef __gnuc_va_list va_list;`
 * into a self-referential typedef and fail there instead. The guards below are
 * the ones glibc, musl and the MSVC CRT test before declaring their own.
 */
#if !defined(_VA_LIST_DEFINED) && !defined(_VA_LIST) && !defined(__va_list__)
#define _VA_LIST_DEFINED
#define _VA_LIST
#define __va_list__
typedef __builtin_va_list va_list;
#endif

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(d, s)      __builtin_va_copy(d, s)

#endif /* GE_HOSTCOMPAT_STDARG_H */
