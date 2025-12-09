/***
 * Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _OPENTELEMETRY_C_WRAPPER_COMPILER_H_
#define _OPENTELEMETRY_C_WRAPPER_COMPILER_H_

#ifdef __GNUC__
#  define __maybe_unused            __attribute__((unused))
#  define __fmt(a,b,c)              __attribute__((format(a, b, c)))

#  define PRAGMA_DO(x)              _Pragma(#x)
#  define PRAGMA_DIAG(c,x)          PRAGMA_DO(c diagnostic x)
#  define PRAGMA_DIAG_IGNORE(w)     PRAGMA_DIAG(GCC, push) PRAGMA_DIAG(GCC, ignored w)
#  define PRAGMA_DIAG_RESTORE       PRAGMA_DIAG(GCC, pop)
#elif defined(__clang__)
#  define __maybe_unused            __attribute__((unused))
#  define __fmt(a,b,c)              __attribute__((format(a, b, c)))

#  define PRAGMA_DO(...)
#  define PRAGMA_DIAG(...)
#  define PRAGMA_DIAG_IGNORE(...)
#  define PRAGMA_DIAG_RESTORE
#else
#  define __maybe_unused
#  define __fmt(...)

#  define PRAGMA_DO(...)
#  define PRAGMA_DIAG(...)
#  define PRAGMA_DIAG_IGNORE(...)
#  define PRAGMA_DIAG_RESTORE
#endif

#endif /* _OPENTELEMETRY_C_WRAPPER_COMPILER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
