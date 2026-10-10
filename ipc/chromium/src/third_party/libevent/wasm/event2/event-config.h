/* Native Linux/Wasm musl has ILP32 pointers and 64-bit time/file offsets. */
#ifndef MOZ_NATIVE_WASM_LIBEVENT_CONFIG_H
#define MOZ_NATIVE_WASM_LIBEVENT_CONFIG_H
#if !defined(__wasm__) || !defined(__linux__) || defined(__wasi__)
#  error "Native musl libevent adapter requires Linux/Wasm"
#endif
/* Load the actual libc time64 marker before the upstream config. */
#include <time.h>
#include "../../linux/event2/event-config.h"
#if !defined(__USE_TIME_BITS64) || __USE_TIME_BITS64 != 1
#  error "Native musl libevent adapter requires time64"
#endif
#undef EVENT__SIZEOF_TIME_T
#define EVENT__SIZEOF_TIME_T 8
#undef EVENT__SIZEOF_OFF_T
#define EVENT__SIZEOF_OFF_T 8
#endif
