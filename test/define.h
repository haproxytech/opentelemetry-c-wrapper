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
#ifndef TEST_DEFINE_H
#define TEST_DEFINE_H

#ifdef __linux__
#  define PRI_PIDT       "d"
#  define PRI_PTHREADT   "lu"
#else
#  define PRI_PIDT       "ld"
#  define PRI_PTHREADT   "u"
#endif

#define _NULL(a)         ((a) == NULL)
#define _nNULL(a)        ((a) != NULL)

#endif /* TEST_DEFINE_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
