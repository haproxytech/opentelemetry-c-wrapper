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
#include "test-util.h"


/***
 * NAME
 *   test_logger_create_destroy - tests logger creation and destruction
 *
 * SYNOPSIS
 *   static void test_logger_create_destroy(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context returned by otelc_init()
 *
 * DESCRIPTION
 *   Verifies that a logger can be created and destroyed without errors.  Also
 *   verifies that the function pointers are properly initialized.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_logger_create_destroy(struct otelc_ctx *ctx)
{
	struct otelc_logger *logger;
	char                *err = NULL;
	int                  retval = TEST_FAIL;

	logger = otelc_logger_create(ctx, &err);
	if (_nNULL(logger)) {
		if (_nNULL(logger->ops))
			retval = TEST_PASS;

		OTELC_OPSR(logger, destroy);

		if (_nNULL(logger))
			retval = TEST_FAIL;
	}

	OTELC_SFREE(err);

	test_report("logger create/destroy", retval);
}


/***
 * NAME
 *   test_logger_create_err_null - tests logger creation with NULL err pointer
 *
 * SYNOPSIS
 *   static void test_logger_create_err_null(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context returned by otelc_init()
 *
 * DESCRIPTION
 *   Verifies that a logger can be created when the err argument is NULL.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_logger_create_err_null(struct otelc_ctx *ctx)
{
	struct otelc_logger *logger;
	int                  retval = TEST_FAIL;

	logger = otelc_logger_create(ctx, NULL);
	if (_nNULL(logger)) {
		OTELC_OPSR(logger, destroy);

		retval = TEST_PASS;
	}

	test_report("logger create with NULL err", retval);
}


/***
 * NAME
 *   test_logger_start - tests logger initialization and startup
 *
 * SYNOPSIS
 *   static void test_logger_start(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance to start
 *
 * DESCRIPTION
 *   Verifies that a logger can be started using the YAML configuration.  After
 *   a successful start, the logger's scope_name should be set.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_logger_start(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, start) == OTELC_RET_OK)
		if (_nNULL(logger->scope_name))
			retval = TEST_PASS;

	test_report("logger start", retval);
}


/***
 * NAME
 *   test_log_basic - tests basic logging with INFO severity
 *
 * SYNOPSIS
 *   static void test_log_basic(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a basic log message can be emitted with INFO severity and no
 *   optional parameters (NULL IDs, timestamp, attributes).
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_basic(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "basic log message") >= 0)
		retval = TEST_PASS;

	test_report("log basic INFO", retval);
}


/***
 * NAME
 *   test_log_severity_trace - tests logging with TRACE severity
 *
 * SYNOPSIS
 *   static void test_log_severity_trace(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with TRACE severity.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_severity_trace(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_TRACE, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "trace message") >= 0)
		retval = TEST_PASS;

	test_report("log TRACE severity", retval);
}


/***
 * NAME
 *   test_log_severity_debug - tests logging with DEBUG severity
 *
 * SYNOPSIS
 *   static void test_log_severity_debug(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with DEBUG severity.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_severity_debug(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_DEBUG, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "debug message") >= 0)
		retval = TEST_PASS;

	test_report("log DEBUG severity", retval);
}


/***
 * NAME
 *   test_log_severity_warn - tests logging with WARN severity
 *
 * SYNOPSIS
 *   static void test_log_severity_warn(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with WARN severity.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_severity_warn(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_WARN, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "warning message") >= 0)
		retval = TEST_PASS;

	test_report("log WARN severity", retval);
}


/***
 * NAME
 *   test_log_severity_error - tests logging with ERROR severity
 *
 * SYNOPSIS
 *   static void test_log_severity_error(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with ERROR severity.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_severity_error(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_ERROR, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "error message") >= 0)
		retval = TEST_PASS;

	test_report("log ERROR severity", retval);
}


/***
 * NAME
 *   test_log_severity_fatal - tests logging with FATAL severity
 *
 * SYNOPSIS
 *   static void test_log_severity_fatal(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with FATAL severity.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_severity_fatal(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_FATAL, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "fatal message") >= 0)
		retval = TEST_PASS;

	test_report("log FATAL severity", retval);
}


/***
 * NAME
 *   test_log_with_explicit_ids - tests logging with explicit span and trace identifiers
 *
 * SYNOPSIS
 *   static void test_log_with_explicit_ids(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with explicitly provided span_id
 *   and trace_id byte arrays and trace flags.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_with_explicit_ids(struct otelc_logger *logger)
{
	static const uint8_t span_id[OTELC_SPAN_ID_SIZE]   = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
	static const uint8_t trace_id[OTELC_TRACE_ID_SIZE] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x01 };
	int                  retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, span_id, sizeof(span_id), trace_id, sizeof(trace_id), 0x01, NULL, NULL, 0, "log with explicit IDs") >= 0)
		retval = TEST_PASS;

	test_report("log with explicit span/trace IDs", retval);
}


/***
 * NAME
 *   test_log_with_format_args - tests logging with printf-style format arguments
 *
 * SYNOPSIS
 *   static void test_log_with_format_args(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that the log function correctly handles printf-style format string
 *   with multiple arguments of different types.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_with_format_args(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	const int rc = OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "count=%d pi=%.2f name=%s flag=%s", 42, 3.14, "test", "true");
	if (rc > 0)
		retval = TEST_PASS;

	test_report("log with format arguments", retval);
}


/***
 * NAME
 *   test_log_with_timestamp - tests logging with an explicit timestamp
 *
 * SYNOPSIS
 *   static void test_log_with_timestamp(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with an explicit timestamp
 *   provided via a timespec structure.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_with_timestamp(struct otelc_logger *logger)
{
	struct timespec ts;
	int             retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_REALTIME, &ts);

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, &ts, NULL, 0, "log with timestamp") >= 0)
		retval = TEST_PASS;

	test_report("log with timestamp", retval);
}


/***
 * NAME
 *   test_log_with_attributes - tests logging with key-value attributes
 *
 * SYNOPSIS
 *   static void test_log_with_attributes(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with multiple key-value
 *   attributes of different types (string, int64, double, bool).
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_with_attributes(struct otelc_logger *logger)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"service",    .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "test-logger" }  },
		{ .key = (char *)"request_id", .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(12345) } },
		{ .key = (char *)"latency",    .value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 0.042 }          },
		{ .key = (char *)"success",    .value = { .u_type = OTELC_VALUE_BOOL,   .u.value_bool   = true }           },
	};
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, attr, OTELC_TABLESIZE(attr), "log with attributes") >= 0)
		retval = TEST_PASS;

	test_report("log with attributes", retval);
}


/***
 * NAME
 *   test_log_with_all_options - tests logging with all optional parameters
 *
 * SYNOPSIS
 *   static void test_log_with_all_options(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with all optional parameters
 *   provided simultaneously: explicit IDs, timestamp, and attributes.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_with_all_options(struct otelc_logger *logger)
{
	static const uint8_t span_id[OTELC_SPAN_ID_SIZE]   = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22 };
	static const uint8_t trace_id[OTELC_TRACE_ID_SIZE] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00 };
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"component", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "test" } },
		{ .key = (char *)"count",     .value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(99) } },
	};
	struct timespec ts;
	int             retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_REALTIME, &ts);

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_WARN, 0, NULL, span_id, sizeof(span_id), trace_id, sizeof(trace_id), 0x01, &ts, attr, OTELC_TABLESIZE(attr), "full options: item=%d", 7) >= 0)
		retval = TEST_PASS;

	test_report("log with all options", retval);
}


/***
 * NAME
 *   test_log_with_event_id - tests logging with an event identifier
 *
 * SYNOPSIS
 *   static void test_log_with_event_id(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with an event identifier and
 *   event name via the log() function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_with_event_id(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_INFO, 1001, "user.login", NULL, 0, NULL, 0, 0, NULL, NULL, 0, "User logged in") >= 0)
		retval = TEST_PASS;

	test_report("log with event_id", retval);
}


/***
 * NAME
 *   test_logger_enabled - tests logger severity enabled check
 *
 * SYNOPSIS
 *   static void test_logger_enabled(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that the enabled() function returns a valid result (true or false)
 *   for different severity levels and does not return OTELC_RET_ERROR.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_logger_enabled(struct otelc_logger *logger)
{
	int rc, retval = TEST_PASS;

	rc = OTELC_OPS(logger, enabled, OTELC_LOG_SEVERITY_INFO);
	if (rc == OTELC_RET_ERROR)
		retval = TEST_FAIL;

	rc = OTELC_OPS(logger, enabled, OTELC_LOG_SEVERITY_FATAL);
	if (rc == OTELC_RET_ERROR)
		retval = TEST_FAIL;

	test_report("logger enabled", retval);
}


/***
 * NAME
 *   test_log_span_basic - tests logging with span context
 *
 * SYNOPSIS
 *   static void test_log_span_basic(struct otelc_logger *logger, const struct otelc_span *span)
 *
 * ARGUMENTS
 *   logger - logger instance
 *   span   - active span to associate with the log
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted using the log_span() function,
 *   which extracts span and trace identifiers from the span.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_span_basic(struct otelc_logger *logger, const struct otelc_span *span)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_INFO, 0, NULL, span, NULL, NULL, 0, "log with span context") >= 0)
		retval = TEST_PASS;

	test_report("log_span basic", retval);
}


/***
 * NAME
 *   test_log_span_with_attributes - tests logging with span context and attributes
 *
 * SYNOPSIS
 *   static void test_log_span_with_attributes(struct otelc_logger *logger, const struct otelc_span *span)
 *
 * ARGUMENTS
 *   logger - logger instance
 *   span   - active span to associate with the log
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with span context and key-value
 *   attributes simultaneously.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_span_with_attributes(struct otelc_logger *logger, const struct otelc_span *span)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"operation", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "span-log-test" } },
		{ .key = (char *)"priority",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(5) }      },
	};
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_DEBUG, 0, NULL, span, NULL, attr, OTELC_TABLESIZE(attr), "span log with attrs") >= 0)
		retval = TEST_PASS;

	test_report("log_span with attributes", retval);
}


/***
 * NAME
 *   test_log_span_with_timestamp - tests logging with span context and timestamp
 *
 * SYNOPSIS
 *   static void test_log_span_with_timestamp(struct otelc_logger *logger, const struct otelc_span *span)
 *
 * ARGUMENTS
 *   logger - logger instance
 *   span   - active span to associate with the log
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with span context and an
 *   explicit timestamp.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_span_with_timestamp(struct otelc_logger *logger, const struct otelc_span *span)
{
	struct timespec ts;
	int             retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_REALTIME, &ts);

	if (OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_ERROR, 0, NULL, span, &ts, NULL, 0, "span log with timestamp") >= 0)
		retval = TEST_PASS;

	test_report("log_span with timestamp", retval);
}


/***
 * NAME
 *   test_log_span_null - tests logging with NULL span via log_span
 *
 * SYNOPSIS
 *   static void test_log_span_null(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that the log_span() function handles a NULL span argument
 *   gracefully by using zeroed span and trace identifiers.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_span_null(struct otelc_logger *logger)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, NULL, NULL, 0, "log_span with NULL span") >= 0)
		retval = TEST_PASS;

	test_report("log_span with NULL span", retval);
}


/***
 * NAME
 *   test_log_span_with_format_args - tests logging with span context and format arguments
 *
 * SYNOPSIS
 *   static void test_log_span_with_format_args(struct otelc_logger *logger, const struct otelc_span *span)
 *
 * ARGUMENTS
 *   logger - logger instance
 *   span   - active span to associate with the log
 *
 * DESCRIPTION
 *   Verifies that the log_span() function correctly handles printf-style format
 *   string with multiple arguments of different types.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_span_with_format_args(struct otelc_logger *logger, const struct otelc_span *span)
{
	int retval = TEST_FAIL;

	const int rc = OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_INFO, 0, NULL, span, NULL, NULL, 0, "count=%d pi=%.2f name=%s", 42, 3.14, "test");
	if (rc > 0)
		retval = TEST_PASS;

	test_report("log_span with format arguments", retval);
}


/***
 * NAME
 *   test_log_span_with_all_options - tests logging with span context and all options
 *
 * SYNOPSIS
 *   static void test_log_span_with_all_options(struct otelc_logger *logger, const struct otelc_span *span)
 *
 * ARGUMENTS
 *   logger - logger instance
 *   span   - active span to associate with the log
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted with span context, timestamp,
 *   attributes, and format arguments simultaneously.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_span_with_all_options(struct otelc_logger *logger, const struct otelc_span *span)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"component", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "test" } },
		{ .key = (char *)"count",     .value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(99) } },
	};
	struct timespec ts;
	int             retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_REALTIME, &ts);

	if (OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_WARN, 0, NULL, span, &ts, attr, OTELC_TABLESIZE(attr), "full span options: item=%d", 7) >= 0)
		retval = TEST_PASS;

	test_report("log_span with all options", retval);
}


/***
 * NAME
 *   test_log_span_with_event_id - tests logging with span context and event identifier
 *
 * SYNOPSIS
 *   static void test_log_span_with_event_id(struct otelc_logger *logger, const struct otelc_span *span)
 *
 * ARGUMENTS
 *   logger - logger instance
 *   span   - active span to associate with the log
 *
 * DESCRIPTION
 *   Verifies that a log message can be emitted using the log_span() function
 *   with a non-zero event identifier and event name.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_span_with_event_id(struct otelc_logger *logger, const struct otelc_span *span)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_INFO, 2001, "request.complete", span, NULL, NULL, 0, "Request completed") >= 0)
		retval = TEST_PASS;

	test_report("log_span with event_id", retval);
}


/***
 * NAME
 *   test_log_span_invalid_handle - tests log_span with a corrupted span idx
 *
 * SYNOPSIS
 *   static void test_log_span_invalid_handle(struct otelc_logger *logger, struct otelc_span *span)
 *
 * ARGUMENTS
 *   logger - logger instance
 *   span   - active span whose idx will be temporarily corrupted
 *
 * DESCRIPTION
 *   Saves the span's idx, replaces it with a large bogus value that does not
 *   exist in the internal handle map, then attempts log_span() with and without
 *   attributes.  Both calls must succeed because the implementation emits the
 *   log without span correlation when span context retrieval fails.  The
 *   original idx is restored afterwards.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_span_invalid_handle(struct otelc_logger *logger, struct otelc_span *span)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"inv_attr", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "invalid" } },
	};
	int64_t saved_idx;
	int     rc, retval = TEST_PASS;

	saved_idx = span->idx;
	span->idx = INT64_C(0x7FFFFFFFFFFFFFFF);

	/* Try log_span without attributes -- succeeds without correlation. */
	rc = OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_INFO, 0, NULL, span, NULL, NULL, 0, "should succeed without correlation");
	if (rc < 0)
		retval = TEST_FAIL;

	/* Try log_span with attributes -- succeeds without correlation. */
	rc = OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_ERROR, 0, NULL, span, NULL, attr, OTELC_TABLESIZE(attr), "should also succeed without correlation");
	if (rc < 0)
		retval = TEST_FAIL;

	/* Restore the original idx. */
	span->idx = saved_idx;

	/* Confirm normal log_span still works. */
	rc = OTELC_OPS(logger, log_span, OTELC_LOG_SEVERITY_INFO, 0, NULL, span, NULL, NULL, 0, "after restore");
	if (rc < 0)
		retval = TEST_FAIL;

	test_report("log_span invalid handle", retval);
}


/***
 * NAME
 *   test_set_min_severity - tests runtime minimum severity filtering
 *
 * SYNOPSIS
 *   static void test_set_min_severity(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that set_min_severity() changes the filtering threshold at
 *   runtime.  After raising the minimum to WARN, a DEBUG log must be
 *   suppressed (returns 0 characters) while a WARN log must succeed.
 *   The original severity is restored at the end so that subsequent tests
 *   are not affected.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_set_min_severity(struct otelc_logger *logger)
{
	otelc_log_severity_t saved = logger->min_severity;
	int                  rc, retval = TEST_FAIL;

	if (OTELC_OPS(logger, set_min_severity, OTELC_LOG_SEVERITY_WARN) == OTELC_RET_OK) {
		/* DEBUG is below WARN -- must be suppressed (returns 0). */
		rc = OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_DEBUG, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "should be suppressed");
		if (rc == 0) {
			/* WARN is at threshold -- must succeed. */
			rc = OTELC_OPS(logger, log, OTELC_LOG_SEVERITY_WARN, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "should pass");
			if (rc > 0)
				retval = TEST_PASS;
		}

		/* Restore original severity. */
		(void)OTELC_OPS(logger, set_min_severity, saved);
	}

	test_report("set_min_severity", retval);
}


/***
 * NAME
 *   test_log_body_explicit_ids - tests log_body with explicit IDs
 *
 * SYNOPSIS
 *   static void test_log_body_explicit_ids(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that log_body() can emit log records with non-string body
 *   values (int64, double, bool, string) using explicitly provided span
 *   and trace identifiers.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_body_explicit_ids(struct otelc_logger *logger)
{
	static const uint8_t span_id[OTELC_SPAN_ID_SIZE]   = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
	static const uint8_t trace_id[OTELC_TRACE_ID_SIZE]  = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x01 };
	const struct otelc_value body_i64    = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(42) };
	const struct otelc_value body_dbl    = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 3.14        };
	const struct otelc_value body_bool   = { .u_type = OTELC_VALUE_BOOL,   .u.value_bool   = true        };
	const struct otelc_value body_string = { .u_type = OTELC_VALUE_STRING, .u.value_string = "body text" };
	int                      retval = TEST_FAIL;

	if ((OTELC_OPS(logger, log_body, OTELC_LOG_SEVERITY_INFO, 0, NULL, span_id, sizeof(span_id), trace_id, sizeof(trace_id), 0x01, NULL, NULL, 0, &body_i64) == OTELC_RET_OK) &&
	    (OTELC_OPS(logger, log_body, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, &body_dbl) == OTELC_RET_OK) &&
	    (OTELC_OPS(logger, log_body, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, &body_bool) == OTELC_RET_OK) &&
	    (OTELC_OPS(logger, log_body, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, &body_string) == OTELC_RET_OK))
		retval = TEST_PASS;

	test_report("log_body with explicit IDs", retval);
}


/***
 * NAME
 *   test_log_body_span - tests log_body_span with span context
 *
 * SYNOPSIS
 *   static void test_log_body_span(struct otelc_logger *logger, const struct otelc_span *span)
 *
 * ARGUMENTS
 *   logger - logger instance
 *   span   - active span to associate with the log
 *
 * DESCRIPTION
 *   Verifies that log_body_span() can emit a log record with a non-string
 *   body value associated with a live span, and also with a NULL span.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_log_body_span(struct otelc_logger *logger, const struct otelc_span *span)
{
	const struct otelc_value body = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(100) };
	int                      retval = TEST_FAIL;

	if ((OTELC_OPS(logger, log_body_span, OTELC_LOG_SEVERITY_INFO, 0, NULL, span, NULL, NULL, 0, &body) == OTELC_RET_OK) &&
	    (OTELC_OPS(logger, log_body_span, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, NULL, NULL, 0, &body) == OTELC_RET_OK))
		retval = TEST_PASS;

	test_report("log_body_span", retval);
}


/***
 * NAME
 *   test_severity_parse - tests severity name string parsing
 *
 * SYNOPSIS
 *   static void test_severity_parse(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that otelc_logger_severity_parse() correctly converts known
 *   severity names (including case variations) and returns INVALID for
 *   unknown names and NULL input.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_severity_parse(void)
{
	int retval = TEST_PASS;

	if (otelc_logger_severity_parse("TRACE") != OTELC_LOG_SEVERITY_TRACE)
		retval = TEST_FAIL;
	if (otelc_logger_severity_parse("debug") != OTELC_LOG_SEVERITY_DEBUG)
		retval = TEST_FAIL;
	if (otelc_logger_severity_parse("Info") != OTELC_LOG_SEVERITY_INFO)
		retval = TEST_FAIL;
	if (otelc_logger_severity_parse("WARN") != OTELC_LOG_SEVERITY_WARN)
		retval = TEST_FAIL;
	if (otelc_logger_severity_parse("ERROR") != OTELC_LOG_SEVERITY_ERROR)
		retval = TEST_FAIL;
	if (otelc_logger_severity_parse("FATAL") != OTELC_LOG_SEVERITY_FATAL)
		retval = TEST_FAIL;
	if (otelc_logger_severity_parse("TRACE2") != OTELC_LOG_SEVERITY_TRACE2)
		retval = TEST_FAIL;
	if (otelc_logger_severity_parse("DEBUG4") != OTELC_LOG_SEVERITY_DEBUG4)
		retval = TEST_FAIL;

	/* Invalid inputs. */
	if (otelc_logger_severity_parse("BOGUS") != OTELC_LOG_SEVERITY_INVALID)
		retval = TEST_FAIL;
	if (otelc_logger_severity_parse(NULL) != OTELC_LOG_SEVERITY_INVALID)
		retval = TEST_FAIL;

	test_report("severity_parse", retval);
}


/***
 * NAME
 *   test_force_flush - tests forced export of buffered logs
 *
 * SYNOPSIS
 *   static void test_force_flush(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that the force_flush() function completes without errors.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_force_flush(struct otelc_logger *logger)
{
	const struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
	int                   retval = TEST_FAIL;

	if ((OTELC_OPS(logger, force_flush, NULL) == OTELC_RET_OK) &&
	    (OTELC_OPS(logger, force_flush, &timeout) == OTELC_RET_OK))
		retval = TEST_PASS;

	test_report("force_flush", retval);
}


/***
 * NAME
 *   test_shutdown - tests shutdown of the logger provider
 *
 * SYNOPSIS
 *   static void test_shutdown(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   Verifies that the shutdown() function completes without errors using both
 *   an unlimited timeout (NULL) and a specific timeout value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_shutdown(struct otelc_logger *logger)
{
	const struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
	int                   retval = TEST_FAIL;

	if ((OTELC_OPS(logger, shutdown, NULL) == OTELC_RET_OK) &&
	    (OTELC_OPS(logger, shutdown, &timeout) == OTELC_RET_OK))
		retval = TEST_PASS;

	test_report("shutdown", retval);
}


/***
 * NAME
 *   main - program entry point
 *
 * SYNOPSIS
 *   int main(int argc, char **argv)
 *
 * ARGUMENTS
 *   argc - number of command-line arguments
 *   argv - array of command-line argument strings
 *
 * DESCRIPTION
 *   Initializes the OpenTelemetry library, creates a logger, runs all logger
 *   tests, and reports the results.
 *
 * RETURN VALUE
 *   Returns EX_OK if all tests pass, or EX_SOFTWARE if any test fails.
 */
int main(int argc, char **argv)
{
	struct otelc_ctx    *ctx    = NULL;
	struct otelc_logger *logger = NULL;
	struct otelc_tracer *tracer = NULL;
	struct otelc_span   *span = NULL;
	const char          *cfg_file;
	char                *otel_err = NULL;
	int                  retval;

	retval = test_init(argc, argv, "logger tests", &cfg_file);
	if (retval >= 0)
		return retval;

	retval = EX_OK;
	OTELC_LOG(stdout, "");

	test_set_ctx(&ctx);

	ctx = otelc_init(cfg_file, test_get_ctx_name(), &otel_err);
	if (_NULL(ctx)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init library" : otel_err);

		return test_done(EX_SOFTWARE, otel_err);
	}

	/***
	 * Tests that create and destroy loggers in isolation.  These must run
	 * before the main logger is created because logger destruction clears
	 * the global otel_logger atomic.
	 */
	OTELC_LOG(stdout, "[logger lifecycle]");
	test_logger_create_destroy(ctx);
	test_logger_create_err_null(ctx);

	/***
	 * Create and start the main logger for the remaining tests.
	 */
	logger = otelc_logger_create(ctx, &otel_err);
	if (_NULL(logger)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to create logger" : otel_err);

		return test_done(EX_SOFTWARE, otel_err);
	}

	test_set_logger(&logger);
	test_logger_start(logger);

	/***
	 * Logging with explicit IDs.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[log with explicit IDs]");
	test_log_basic(logger);
	test_log_with_explicit_ids(logger);
	test_log_with_format_args(logger);

	/***
	 * Severity level tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[severity levels]");
	test_log_severity_trace(logger);
	test_log_severity_debug(logger);
	test_log_severity_warn(logger);
	test_log_severity_error(logger);
	test_log_severity_fatal(logger);

	/***
	 * Log options tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[log options]");
	test_log_with_timestamp(logger);
	test_log_with_attributes(logger);
	test_log_with_all_options(logger);
	test_log_with_event_id(logger);
	test_log_body_explicit_ids(logger);

	/***
	 * Logger query tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[logger queries]");
	test_logger_enabled(logger);
	test_set_min_severity(logger);
	test_severity_parse();

	/***
	 * Logging with span context.  Create and start a tracer to obtain an
	 * active span for the log_span() tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[log with span context]");

	tracer = otelc_tracer_create(ctx, &otel_err);
	if (_NULL(tracer)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to create tracer" : otel_err);

		return test_done(EX_SOFTWARE, otel_err);
	}

	test_set_tracer(&tracer);

	if (OTELC_OPS(tracer, start) != OTELC_RET_OK) {
		OTELC_LOG(stderr, "ERROR: Unable to start tracer");

		return test_done(EX_SOFTWARE, otel_err);
	}

	span = OTELC_OPS(tracer, start_span, "test-logger-span");
	if (_NULL(span)) {
		OTELC_LOG(stderr, "ERROR: Unable to start span");

		return test_done(EX_SOFTWARE, otel_err);
	}

	test_log_span_basic(logger, span);
	test_log_span_with_attributes(logger, span);
	test_log_span_with_timestamp(logger, span);
	test_log_span_with_format_args(logger, span);
	test_log_span_with_all_options(logger, span);
	test_log_span_with_event_id(logger, span);
	test_log_span_null(logger);
	test_log_body_span(logger, span);
	test_log_span_invalid_handle(logger, span);

	OTELC_OPSR(span, end);

	/***
	 * Logger operations.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[logger operations]");
	test_force_flush(logger);
	test_shutdown(logger);

	/***
	 * Handle statistics verification.  One span was created and
	 * destroyed during the log_span tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[handle statistics]");

	if (otelc_statistics_check(0, 0, 1, 0, 1, 1) != 0)
		retval = TEST_FAIL;
	if (otelc_statistics_check(1, 0, 0, 0, 0, 0) != 0)
		retval = TEST_FAIL;
	test_report("handle statistics", retval);

	return test_done(retval, otel_err);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
