#pragma once
#include <cstddef>
#include <cinttypes>

#include "config.h"

extern "C"
{
#if !HAVE_ARC4RANDOM
  uint32_t arc4random(void);
#endif
#if !HAVE_ARC4RANDOM_BUF
  void arc4random_buf(void* buf, size_t nbytes);
#endif
#if !HAVE_ARC4RANDOM_UNIFORM
  uint32_t arc4random_uniform(uint32_t upper_bound);
#endif
#if !HAVE_EXPLICIT_BZERO
  void explicit_bzero(void*, size_t len);
#endif
}
