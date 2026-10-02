#pragma once

// MSVC ignores GCC/Clang visibility attributes. The CP4 module exports its
// production ABI symbols explicitly through __declspec(dllexport) or /EXPORT.
#if defined(_MSC_VER) && !defined(__clang__)
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif
