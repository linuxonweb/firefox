/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "InterposerHelper.h"

// The interposers in this file cover all the functions used to access the
// environment (getenv(), putenv(), setenv(), unsetenv() and clearenv()). They
// all use the mutex below for synchronization to prevent races that caused
// startup crashes, see bug 1752703.
#if defined(__linux__) && defined(__wasm__) && !defined(__wasi__)
// Static native links resolve the actual musl entry points at link time.
// musl secure_getenv calls getenv, and putenv without '=' calls unsetenv.
// Those nested interposer entries must share the same recursive lock.
static pthread_mutex_t gEnvLock;
static pthread_once_t gEnvLockOnce = PTHREAD_ONCE_INIT;
static void InitEnvLock() {
  pthread_mutexattr_t attr;
  MOZ_RELEASE_ASSERT(pthread_mutexattr_init(&attr) == 0);
  MOZ_RELEASE_ASSERT(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) == 0);
  MOZ_RELEASE_ASSERT(pthread_mutex_init(&gEnvLock, &attr) == 0);
  MOZ_RELEASE_ASSERT(pthread_mutexattr_destroy(&attr) == 0);
}
extern "C" {
char* __real_secure_getenv(const char*);
int __real_putenv(char*);
int __real_setenv(const char*, const char*, int);
int __real_unsetenv(const char*);
int __real_clearenv(void);
}
#  define ENV_SYMBOL(name) __wrap_##name
#  define REAL_ENV_SYMBOL(name) __real_##name
#else
static pthread_mutex_t gEnvLock = PTHREAD_MUTEX_INITIALIZER;
#  define ENV_SYMBOL(name) name
#  define REAL_ENV_SYMBOL(name) GET_REAL_SYMBOL(name)
#endif

static void LockEnv() {
#if defined(__linux__) && defined(__wasm__) && !defined(__wasi__)
  MOZ_RELEASE_ASSERT(pthread_once(&gEnvLockOnce, InitEnvLock) == 0);
#endif
  pthread_mutex_lock(&gEnvLock);
}

static char* internal_getenv(const char* aName) {
  if (environ == nullptr || aName[0] == '\0') {
    return nullptr;
  }

  size_t len = strlen(aName);
  for (char** env_ptr = environ; *env_ptr != nullptr; ++env_ptr) {
    if ((aName[0] == (*env_ptr)[0]) && (strncmp(aName, *env_ptr, len) == 0) &&
        ((*env_ptr)[len] == '=')) {
      return *env_ptr + len + 1;
    }
  }

  return nullptr;
}

extern "C" {

MFBT_API char* getenv(const char* name) {
  LockEnv();
  char* result = internal_getenv(name);
  pthread_mutex_unlock(&gEnvLock);

  return result;
}

MFBT_API char* ENV_SYMBOL(secure_getenv)(const char* name) {
  static const auto real_secure_getenv = REAL_ENV_SYMBOL(secure_getenv);

  LockEnv();
  char* result = real_secure_getenv(name);
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

MFBT_API char* ENV_SYMBOL(__secure_getenv)(const char* name) {
#if defined(__linux__) && defined(__wasm__) && !defined(__wasi__)
  // musl exports secure_getenv; the compatibility alias uses that same check.
  static const auto real___secure_getenv = __real_secure_getenv;
#else
  static const auto real___secure_getenv = GET_REAL_SYMBOL(__secure_getenv);
#endif

  LockEnv();
  char* result = real___secure_getenv(name);
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

MFBT_API int ENV_SYMBOL(putenv)(char* string) {
  static const auto real_putenv = REAL_ENV_SYMBOL(putenv);

  LockEnv();
  int result = real_putenv(string);
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

MFBT_API int ENV_SYMBOL(setenv)(const char* name, const char* value, int replace) {
  static const auto real_setenv = REAL_ENV_SYMBOL(setenv);

  LockEnv();
  int result = real_setenv(name, value, replace);
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

MFBT_API int ENV_SYMBOL(unsetenv)(const char* name) {
  static const auto real_unsetenv = REAL_ENV_SYMBOL(unsetenv);

  LockEnv();
  int result = real_unsetenv(name);
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

MFBT_API int ENV_SYMBOL(clearenv)(void) {
  static const auto real_clearenv = REAL_ENV_SYMBOL(clearenv);

  LockEnv();
  int result = real_clearenv();
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

}  // extern "C"

#undef ENV_SYMBOL
#undef REAL_ENV_SYMBOL
