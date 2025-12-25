// -*- C++ -*-

/**
 * @file   platformspecific.h
 * @brief  Platform-specific compiler directives.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */


#ifndef DH_PLATFORMSPECIFIC_H
#define DH_PLATFORMSPECIFIC_H

// C++23 provides std::unreachable()
#if __cplusplus >= 202302L
#include <utility>
#define DH_UNREACHABLE() std::unreachable()
#elif defined(__GNUC__) || defined(__clang__)
#define DH_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define DH_UNREACHABLE() __assume(false)
#else
#define DH_UNREACHABLE() ((void)0)
#endif

#if defined(_WIN32)

// Turn inlining hints into requirements.
#pragma inline_depth(255)
#define inline __forceinline
#pragma warning(disable: 4530)
#pragma warning(disable:4273)
#define NO_INLINE __declspec(noinline)
#define ATTRIBUTE_ALWAYS_INLINE __forceinline
#define ASSUME(x) __assume(x)

#elif defined(__GNUC__) || defined(__clang__)

#define NO_INLINE __attribute__ ((noinline))
#define ATTRIBUTE_ALWAYS_INLINE __attribute__((always_inline)) inline

// ASSUME tells the compiler a condition is always true, enabling optimizations.
// C++23 has [[assume(x)]] but it's an attribute, not usable as expression.
// Use compiler intrinsics for now.
#if defined(__clang__)
#define ASSUME(x) __builtin_assume(x)
#else
#define ASSUME(x) do { if (!(x)) DH_UNREACHABLE(); } while (0)
#endif

#else

#define NO_INLINE
#define ATTRIBUTE_ALWAYS_INLINE inline
#define ASSUME(x) ((void)0)

#endif

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

#endif
