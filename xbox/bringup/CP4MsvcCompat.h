#pragma once

// MSVC ignores GCC/Clang visibility attributes. The CP4 module exports its
// production ABI symbols explicitly through __declspec(dllexport) or /EXPORT.
#if defined(_MSC_VER) && !defined(__clang__)
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif


#if defined(_MSC_VER)
// GXRuntime's host-backed DVD layer uses POSIX 64-bit file offsets. MSVC/UWP
// exposes the equivalent CRT operation as _fseeki64.
#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef __int64 off_t;
#endif
#ifndef fseeko
#define fseeko _fseeki64
#endif
#endif
