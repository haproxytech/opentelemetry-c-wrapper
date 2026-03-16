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
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include "include.h"

__CPLUSPLUS_DECL_BEGIN

#define TEST_PASS          0
#define TEST_FAIL          1
#define DEFAULT_CFG_FILE   "otel-cfg.yml"


extern int tests_run, tests_passed, tests_failed;


void test_report(const char *name, int result);
void test_usage(const char *program_name);
int  test_init(int argc, char **argv, const char *banner, const char **cfg_file);
void test_set_tracer(struct otelc_tracer **tracer);
void test_set_meter(struct otelc_meter **meter);
void test_set_logger(struct otelc_logger **logger);
int  test_done(int retval, char *otel_err);
int  test_summary(int retval);

__CPLUSPLUS_DECL_END
#endif /* TEST_UTIL_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
