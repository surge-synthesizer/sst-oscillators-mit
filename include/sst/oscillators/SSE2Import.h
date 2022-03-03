//
// Created by Paul Walker on 3/2/22.
//

#ifndef SST_OSCILLATORS_MIT_SSE2IMPORT_H
#define SST_OSCILLATORS_MIT_SSE2IMPORT_H

/*
 * Our strategy here is:
 * - If we are on X86 use SSE
 * - If we are not use SIMDE
 * - Assume the cmake brings its own simde to the client
 * - Vector everything through one spot so we can use other strategies in the future
 */
#if defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) ||                                   \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#else
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse2.h"
#endif

#endif // SST_OSCILLATORS_MIT_SSE2IMPORT_H
