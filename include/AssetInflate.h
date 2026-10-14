#ifndef WATCH_ASSET_INFLATE_H
#define WATCH_ASSET_INFLATE_H
/* Pinned zlib-licensed tinf1.2.1. Compile its internal invariant assertions out;
 * every public input/output bound check remains active. No allocator or I/O. */
#ifndef NDEBUG
#define WATCH_ASSET_RESTORE_ASSERT
#define NDEBUG
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "../lib/tinf/tinflate.c"
#pragma GCC diagnostic pop
#ifdef WATCH_ASSET_RESTORE_ASSERT
#undef NDEBUG
#undef WATCH_ASSET_RESTORE_ASSERT
#include <assert.h>
#endif
#endif
