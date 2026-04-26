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
 *   test_tracer_create_destroy - tests tracer creation and destruction
 *
 * SYNOPSIS
 *   static void test_tracer_create_destroy(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context returned by otelc_init()
 *
 * DESCRIPTION
 *   Verifies that a tracer can be created and destroyed without errors.  Also
 *   verifies that the function pointers are properly initialized.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_tracer_create_destroy(struct otelc_ctx *ctx)
{
	struct otelc_tracer *tracer;
	char                *err = NULL;
	int                  retval = TEST_FAIL;

	tracer = otelc_tracer_create(ctx, &err);
	if (_nNULL(tracer)) {
		if (_nNULL(tracer->ops))
			retval = TEST_PASS;

		OTELC_OPSR(tracer, destroy);

		if (_nNULL(tracer))
			retval = TEST_FAIL;
	}

	OTELC_SFREE(err);

	test_report("tracer create/destroy", retval);
}


/***
 * NAME
 *   test_tracer_create_err_null - tests tracer creation with NULL err pointer
 *
 * SYNOPSIS
 *   static void test_tracer_create_err_null(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context returned by otelc_init()
 *
 * DESCRIPTION
 *   Verifies that a tracer can be created when the err argument is NULL.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_tracer_create_err_null(struct otelc_ctx *ctx)
{
	struct otelc_tracer *tracer;
	int                  retval = TEST_FAIL;

	tracer = otelc_tracer_create(ctx, NULL);
	if (_nNULL(tracer)) {
		OTELC_OPSR(tracer, destroy);

		retval = TEST_PASS;
	}

	test_report("tracer create with NULL err", retval);
}


/***
 * NAME
 *   test_tracer_start - tests tracer initialization and startup
 *
 * SYNOPSIS
 *   static void test_tracer_start(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance to start
 *
 * DESCRIPTION
 *   Verifies that a tracer can be started using the YAML configuration.  After
 *   a successful start, the tracer's scope_name should be set.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_tracer_start(struct otelc_tracer *tracer)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(tracer, start) == OTELC_RET_OK)
		if (_nNULL(tracer->scope_name))
			retval = TEST_PASS;

	test_report("tracer start", retval);
}


/***
 * NAME
 *   test_span_start_end - tests basic span creation and termination
 *
 * SYNOPSIS
 *   static void test_span_start_end(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a span can be started using the simple start_span() interface
 *   and ended using end().
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_start_end(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	int                retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "test span");
	if (_nNULL(span)) {
		OTELC_OPSR(span, end);

		if (_NULL(span))
			retval = TEST_PASS;
	}

	test_report("span start/end", retval);
}


/***
 * NAME
 *   test_span_with_timestamps - tests span creation with explicit timestamps
 *
 * SYNOPSIS
 *   static void test_span_with_timestamps(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a span can be started with explicit steady and system clock
 *   timestamps via start_span_with_options().
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_with_timestamps(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	struct timespec    ts_steady, ts_system;
	int                retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	span = OTELC_OPS(tracer, start_span_with_options, "timestamped span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_INTERNAL, NULL, 0);
	if (_nNULL(span)) {
		OTELC_OPSR(span, end);

		retval = TEST_PASS;
	}

	test_report("span with timestamps", retval);
}


/***
 * NAME
 *   test_span_kinds - tests all supported span kinds
 *
 * SYNOPSIS
 *   static void test_span_kinds(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that spans can be created with each of the supported span kinds:
 *   UNSPECIFIED, INTERNAL, SERVER, CLIENT, PRODUCER, CONSUMER.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_kinds(struct otelc_tracer *tracer)
{
	static const struct {
		otelc_span_kind_t  kind;
		const char        *name;
	} kinds[] = {
		{ OTELC_SPAN_KIND_UNSPECIFIED, "UNSPECIFIED" },
		{ OTELC_SPAN_KIND_INTERNAL,    "INTERNAL"    },
		{ OTELC_SPAN_KIND_SERVER,      "SERVER"      },
		{ OTELC_SPAN_KIND_CLIENT,      "CLIENT"      },
		{ OTELC_SPAN_KIND_PRODUCER,    "PRODUCER"    },
		{ OTELC_SPAN_KIND_CONSUMER,    "CONSUMER"    },
	};
	struct otelc_span *span;
	struct timespec    ts_steady, ts_system;
	char               test_name[64];
	int                retval = TEST_PASS;
	size_t             i;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	for (i = 0; i < OTELC_TABLESIZE(kinds); i++) {
		span = OTELC_OPS(tracer, start_span_with_options, "kind test span", NULL, NULL, &ts_steady, &ts_system, kinds[i].kind, NULL, 0);
		if (_NULL(span)) {
			(void)snprintf(test_name, sizeof(test_name), "span kind %s", kinds[i].name);
			test_report(test_name, TEST_FAIL);

			retval = TEST_FAIL;
		} else {
			OTELC_OPSR(span, end);
		}
	}

	test_report("span kinds (all 6)", retval);
}


/***
 * NAME
 *   test_span_parent_child - tests parent-child span relationship
 *
 * SYNOPSIS
 *   static void test_span_parent_child(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a child span can be created with a parent span specified via
 *   start_span_with_options().  Both spans are ended in reverse order.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_parent_child(struct otelc_tracer *tracer)
{
	struct otelc_span *parent, *child;
	struct timespec    ts_steady, ts_system;
	int                retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	parent = OTELC_OPS(tracer, start_span_with_options, "parent span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0);
	if (_nNULL(parent)) {
		child = OTELC_OPS(tracer, start_span_with_options, "child span", parent, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_INTERNAL, NULL, 0);
		if (_nNULL(child)) {
			OTELC_OPSR(child, end);

			retval = TEST_PASS;
		}

		OTELC_OPSR(parent, end);
	}

	test_report("span parent-child", retval);
}


/***
 * NAME
 *   test_span_get_id - tests retrieval of span identifiers
 *
 * SYNOPSIS
 *   static void test_span_get_id(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that span_id, trace_id, and trace_flags can be retrieved from an
 *   active span.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_get_id(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	uint8_t            span_id[OTELC_SPAN_ID_SIZE] = { 0 }, trace_id[OTELC_TRACE_ID_SIZE] = { 0 }, trace_flags = 0;
	int                retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "get id span");
	if (_nNULL(span)) {
		if (OTELC_OPS(span, get_id, span_id, sizeof(span_id), trace_id, sizeof(trace_id), &trace_flags) == OTELC_RET_OK)
			retval = TEST_PASS;

		OTELC_OPSR(span, end);
	}

	test_report("span get_id", retval);
}


/***
 * NAME
 *   test_span_is_recording - tests whether a span reports recording state
 *
 * SYNOPSIS
 *   static void test_span_is_recording(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that is_recording() returns true for an active span created with
 *   the default always_on sampler.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_is_recording(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	int                retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "is recording span");
	if (_nNULL(span)) {
		if (OTELC_OPS(span, is_recording) == true)
			retval = TEST_PASS;

		OTELC_OPSR(span, end);
	}

	test_report("span is_recording", retval);
}


/***
 * NAME
 *   test_span_set_attribute - tests setting attributes on a span
 *
 * SYNOPSIS
 *   static void test_span_set_attribute(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that attributes of various types can be set on a span using the
 *   set_attribute_var(), set_attribute_kv_var(), and set_attribute_kv_n()
 *   interfaces.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_set_attribute(struct otelc_tracer *tracer)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"attr_bool",   .value = { .u_type = OTELC_VALUE_BOOL,   .u.value_bool   = true              } },
		{ .key = (char *)"attr_int32",  .value = { .u_type = OTELC_VALUE_INT32,  .u.value_int32  = INT32_C(42)       } },
		{ .key = (char *)"attr_int64",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(1234567)  } },
		{ .key = (char *)"attr_uint64", .value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(9876543) } },
		{ .key = (char *)"attr_double", .value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 3.14              } },
		{ .key = (char *)"attr_string", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "test value"      } },
	};
	struct otelc_span *span;
	int                rc, retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "attribute span");
	if (_nNULL(span)) {
		rc = OTELC_OPS(span, set_attribute_var, attr[0].key, &(attr[0].value), attr[1].key, &(attr[1].value), NULL);
		if (rc >= 0) {
			rc = OTELC_OPS(span, set_attribute_kv_var, attr + 2, attr + 3, NULL);
			if (rc >= 0) {
				rc = OTELC_OPS(span, set_attribute_kv_n, attr + 4, 2);
				if (rc >= 0)
					retval = TEST_PASS;
			}
		}

		OTELC_OPSR(span, end);
	}

	test_report("span set_attribute", retval);
}


/***
 * NAME
 *   test_span_add_event - tests adding events to a span
 *
 * SYNOPSIS
 *   static void test_span_add_event(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that events with attributes can be added to a span using the
 *   add_event_var(), add_event_kv_var(), and add_event_kv_n() interfaces.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_add_event(struct otelc_tracer *tracer)
{
	static const struct otelc_kv event_attr[] = {
		{ .key = (char *)"event_key_string", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "event data" } },
		{ .key = (char *)"event_key_int64",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(999) } },
		{ .key = (char *)"event_key_double", .value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 2.718        } },
		{ .key = (char *)"event_key_bool",   .value = { .u_type = OTELC_VALUE_BOOL,   .u.value_bool   = false        } },
	};
	struct otelc_span *span;
	struct timespec    ts_system;
	int                rc, retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	span = OTELC_OPS(tracer, start_span, "event span");
	if (_nNULL(span)) {
		rc = OTELC_OPS(span, add_event_var, "test_event_1", &ts_system, event_attr[0].key, &(event_attr[0].value), event_attr[1].key, &(event_attr[1].value), NULL);
		if (rc >= 0) {
			rc = OTELC_OPS(span, add_event_kv_var, "test_event_2", &ts_system, event_attr + 2, NULL);
			if (rc >= 0) {
				rc = OTELC_OPS(span, add_event_kv_n, "test_event_3", &ts_system, event_attr, OTELC_TABLESIZE(event_attr));
				if (rc >= 0)
					retval = TEST_PASS;
			}
		}

		OTELC_OPSR(span, end);
	}

	test_report("span add_event", retval);
}


/***
 * NAME
 *   test_span_add_event_null_timestamp - tests adding events without an explicit timestamp
 *
 * SYNOPSIS
 *   static void test_span_add_event_null_timestamp(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that events can be added to a span with a NULL timestamp argument
 *   using the add_event_var(), add_event_kv_var(), and add_event_kv_n()
 *   interfaces.  When the timestamp is NULL, the SDK uses the current time.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_add_event_null_timestamp(struct otelc_tracer *tracer)
{
	static const struct otelc_kv event_attr[] = {
		{ .key = (char *)"ev_key", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "ev_data" } },
	};
	struct otelc_span *span;
	int                rc, retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "event null ts span");
	if (_nNULL(span)) {
		rc = OTELC_OPS(span, add_event_var, "ev_null_ts_1", NULL, event_attr[0].key, &(event_attr[0].value), NULL);
		if (rc >= 0) {
			rc = OTELC_OPS(span, add_event_kv_var, "ev_null_ts_2", NULL, event_attr, NULL);
			if (rc >= 0) {
				rc = OTELC_OPS(span, add_event_kv_n, "ev_null_ts_3", NULL, event_attr, OTELC_TABLESIZE(event_attr));
				if (rc >= 0)
					retval = TEST_PASS;
			}
		}

		OTELC_OPSR(span, end);
	}

	test_report("span add_event NULL timestamp", retval);
}


/***
 * NAME
 *   test_span_set_status - tests setting span status
 *
 * SYNOPSIS
 *   static void test_span_set_status(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that span status can be set using the set_status() interface for
 *   the OK and ERROR status codes.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_set_status(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	int                retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "status span ok");
	if (_nNULL(span)) {
		if (OTELC_OPS(span, set_status, OTELC_SPAN_STATUS_OK, NULL) == OTELC_RET_OK)
			retval = TEST_PASS;

		OTELC_OPSR(span, end);
	}

	test_report("span set_status OK", retval);

	retval = TEST_FAIL;
	span = OTELC_OPS(tracer, start_span, "status span error");
	if (_nNULL(span)) {
		if (OTELC_OPS(span, set_status, OTELC_SPAN_STATUS_ERROR, "test error") == OTELC_RET_OK)
			retval = TEST_PASS;

		OTELC_OPSR(span, end);
	}

	test_report("span set_status ERROR", retval);
}


/***
 * NAME
 *   test_span_set_operation_name - tests updating the span name
 *
 * SYNOPSIS
 *   static void test_span_set_operation_name(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a span's operation name can be changed after creation using
 *   the set_operation_name() interface.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_set_operation_name(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	int                retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "original name");
	if (_nNULL(span)) {
		OTELC_OPS(span, set_operation_name, "updated name");
		OTELC_OPSR(span, end);

		retval = TEST_PASS;
	}

	test_report("span set_operation_name", retval);
}


/***
 * NAME
 *   test_span_end_with_options - tests span termination with options
 *
 * SYNOPSIS
 *   static void test_span_end_with_options(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a span can be ended with an explicit monotonic timestamp and
 *   status code.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_end_with_options(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	struct timespec    ts_steady;
	int                retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "end options span");
	if (_nNULL(span)) {
		(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
		OTELC_OPSR(span, end_with_options, &ts_steady, OTELC_SPAN_STATUS_OK, "completed");

		if (_NULL(span))
			retval = TEST_PASS;
	}

	test_report("span end_with_options", retval);
}


/***
 * NAME
 *   test_span_end_with_status_ignore - tests span termination with IGNORE status
 *
 * SYNOPSIS
 *   static void test_span_end_with_status_ignore(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a span can be ended with OTELC_SPAN_STATUS_IGNORE, which
 *   instructs the library to skip setting any status on the span.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_end_with_status_ignore(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	int                retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "end ignore span");
	if (_nNULL(span)) {
		OTELC_OPSR(span, end_with_options, NULL, OTELC_SPAN_STATUS_IGNORE, NULL);

		if (_NULL(span))
			retval = TEST_PASS;
	}

	test_report("span end_with_options IGNORE", retval);
}


/***
 * NAME
 *   test_span_baggage - tests baggage set and get operations
 *
 * SYNOPSIS
 *   static void test_span_baggage(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that baggage key-value pairs can be set on a span and later
 *   retrieved using both get_baggage() and get_baggage_var().
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_baggage(struct otelc_tracer *tracer)
{
	struct otelc_span     *span;
	struct otelc_text_map *baggage;
	char                  *value;
	int                    rc, retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "baggage span");
	if (_nNULL(span)) {
		rc = OTELC_OPS(span, set_baggage_var, "bag_key_1", "bag_value_1", "bag_key_2", "bag_value_2", NULL);
		if (rc >= 0) {
			value = OTELC_OPS(span, get_baggage, "bag_key_1");
			if (_nNULL(value)) {
				if (strcmp(value, "bag_value_1") == 0)
					retval = TEST_PASS;

				OTELC_SFREE(value);
			}
		}

		baggage = OTELC_OPS(span, get_baggage_var, "bag_key_1", "bag_key_2", NULL);
		if (_nNULL(baggage)) {
			if ((retval == TEST_PASS) && (baggage->count < 2))
				retval = TEST_FAIL;

			otelc_text_map_destroy(&baggage);
		}

		OTELC_OPSR(span, end);
	}

	test_report("span baggage set/get", retval);
}


/***
 * NAME
 *   test_span_baggage_kv - tests baggage set via key-value struct interfaces
 *
 * SYNOPSIS
 *   static void test_span_baggage_kv(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that baggage key-value pairs can be set on a parent span using
 *   the set_baggage_kv_var() and set_baggage_kv_n() interfaces, and that the
 *   baggage is propagated to a child span created through context injection
 *   and extraction.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_baggage_kv(struct otelc_tracer *tracer)
{
	static const struct otelc_kv bag[] = {
		{ .key = (char *)"kv_bag_1", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "kv_value_1" } },
		{ .key = (char *)"kv_bag_2", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "kv_value_2" } },
		{ .key = (char *)"kv_bag_3", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "kv_value_3" } },
	};
	struct otelc_text_map_writer  tm_wr;
	struct otelc_text_map_reader  tm_rd;
	struct otelc_span_context    *context;
	struct otelc_span            *parent, *child;
	struct otelc_text_map        *text_map;
	struct timespec               ts_steady, ts_system;
	char                         *value;
	int                           rc, retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	parent = OTELC_OPS(tracer, start_span_with_options, "baggage kv parent", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_CLIENT, NULL, 0);
	if (_nNULL(parent)) {
		rc = OTELC_OPS(parent, set_baggage_kv_var, bag, bag + 1, NULL);
		if (rc >= 0)
			rc = OTELC_OPS(parent, set_baggage_kv_n, bag + 2, 1);

		if (rc >= 0) {
			otelc_span_inject_text_map(parent, &tm_wr);
			text_map = &(tm_wr.text_map);

			context = otelc_tracer_extract_text_map(tracer, &tm_rd, text_map);
			if (_nNULL(context)) {
				child = OTELC_OPS(tracer, start_span_with_options, "baggage kv child", NULL, context, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0);
				if (_nNULL(child)) {
					value = OTELC_OPS(child, get_baggage, "kv_bag_3");
					if (_nNULL(value)) {
						if (strcmp(value, "kv_value_3") == 0)
							retval = TEST_PASS;

						OTELC_SFREE(value);
					}

					OTELC_OPSR(child, end);
				}

				OTELC_OPSR(context, destroy);
			}

			otelc_text_map_destroy(&text_map);
		}

		OTELC_OPSR(parent, end);
	}

	test_report("span baggage kv propagation", retval);
}


/***
 * NAME
 *   test_context_propagation_text_map - tests context propagation via text map
 *
 * SYNOPSIS
 *   static void test_context_propagation_text_map(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies the full context propagation cycle using text map carriers.
 *   A span is created, its context is injected into a text map writer, and
 *   then extracted using a text map reader to create a new child span.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_context_propagation_text_map(struct otelc_tracer *tracer)
{
	struct otelc_text_map_writer  tm_wr;
	struct otelc_text_map_reader  tm_rd;
	struct otelc_span_context    *context;
	struct otelc_span            *span, *prop_span;
	struct otelc_text_map        *text_map;
	struct timespec               ts_steady, ts_system;
	int                           retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	span = OTELC_OPS(tracer, start_span_with_options, "tm inject span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_CLIENT, NULL, 0);
	if (_nNULL(span)) {
		otelc_span_inject_text_map(span, &tm_wr);
		text_map = &(tm_wr.text_map);

		context = otelc_tracer_extract_text_map(tracer, &tm_rd, text_map);
		if (_nNULL(context)) {
			prop_span = OTELC_OPS(tracer, start_span_with_options, "tm propagated span", NULL, context, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0);
			if (_nNULL(prop_span)) {
				OTELC_OPSR(prop_span, end);

				retval = TEST_PASS;
			}

			OTELC_OPSR(context, destroy);
		}

		otelc_text_map_destroy(&text_map);
		OTELC_OPSR(span, end);
	}

	test_report("context propagation text map", retval);
}


/***
 * NAME
 *   test_context_propagation_http_headers - tests context propagation via HTTP headers
 *
 * SYNOPSIS
 *   static void test_context_propagation_http_headers(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies the full context propagation cycle using HTTP headers carriers.
 *   A span is created, its context is injected into an HTTP headers writer,
 *   and then extracted using an HTTP headers reader to create a new child span.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_context_propagation_http_headers(struct otelc_tracer *tracer)
{
	struct otelc_http_headers_writer  hh_wr;
	struct otelc_http_headers_reader  hh_rd;
	struct otelc_span_context        *context;
	struct otelc_span                *span, *prop_span;
	struct otelc_text_map            *text_map;
	struct timespec                   ts_steady, ts_system;
	int                               retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	span = OTELC_OPS(tracer, start_span_with_options, "hh inject span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_CLIENT, NULL, 0);
	if (_nNULL(span)) {
		otelc_span_inject_http_headers(span, &hh_wr);
		text_map = &(hh_wr.text_map);

		context = otelc_tracer_extract_http_headers(tracer, &hh_rd, text_map);
		if (_nNULL(context)) {
			prop_span = OTELC_OPS(tracer, start_span_with_options, "hh propagated span", NULL, context, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0);
			if (_nNULL(prop_span)) {
				OTELC_OPSR(prop_span, end);

				retval = TEST_PASS;
			}

			OTELC_OPSR(context, destroy);
		}

		otelc_text_map_destroy(&text_map);
		OTELC_OPSR(span, end);
	}

	test_report("context propagation http headers", retval);
}


/***
 * NAME
 *   test_span_context_queries - tests span context query functions
 *
 * SYNOPSIS
 *   static void test_span_context_queries(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that the span context query functions is_valid(), is_sampled(),
 *   and is_remote() return the expected values for a context extracted from a
 *   text map carrier.  An extracted context should be valid, sampled (always_on
 *   sampler), and remote.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_context_queries(struct otelc_tracer *tracer)
{
	struct otelc_text_map_writer  tm_wr;
	struct otelc_text_map_reader  tm_rd;
	struct otelc_span_context    *context;
	struct otelc_span            *span;
	struct otelc_text_map        *text_map;
	struct timespec               ts_steady, ts_system;
	int                           retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	span = OTELC_OPS(tracer, start_span_with_options, "context query span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_CLIENT, NULL, 0);
	if (_nNULL(span)) {
		otelc_span_inject_text_map(span, &tm_wr);
		text_map = &(tm_wr.text_map);

		context = otelc_tracer_extract_text_map(tracer, &tm_rd, text_map);
		if (_nNULL(context)) {
			if ((OTELC_OPS(context, is_valid) == true) &&
			    (OTELC_OPS(context, is_sampled) == true) &&
			    (OTELC_OPS(context, is_remote) == true))
				retval = TEST_PASS;

			OTELC_OPSR(context, destroy);
		}

		otelc_text_map_destroy(&text_map);
		OTELC_OPSR(span, end);
	}

	test_report("span context queries", retval);
}


/***
 * NAME
 *   test_span_context_get_id - tests span context identifier retrieval
 *
 * SYNOPSIS
 *   static void test_span_context_get_id(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that the span context get_id() function returns the same
 *   identifiers as the original span.  A span is created, its IDs are captured,
 *   then the context is injected into a text map and extracted.  The extracted
 *   context's get_id() results are compared with the original span's
 *   identifiers.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_context_get_id(struct otelc_tracer *tracer)
{
	struct otelc_text_map_writer  tm_wr;
	struct otelc_text_map_reader  tm_rd;
	struct otelc_span_context    *context;
	struct otelc_span            *span;
	struct otelc_text_map        *text_map;
	struct timespec               ts_steady, ts_system;
	uint8_t                       span_id_orig[OTELC_SPAN_ID_SIZE] = { 0 };
	uint8_t                       trace_id_orig[OTELC_TRACE_ID_SIZE] = { 0 };
	uint8_t                       trace_flags_orig = 0;
	uint8_t                       span_id_ctx[OTELC_SPAN_ID_SIZE] = { 0 };
	uint8_t                       trace_id_ctx[OTELC_TRACE_ID_SIZE] = { 0 };
	uint8_t                       trace_flags_ctx = 0;
	int                           retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	span = OTELC_OPS(tracer, start_span_with_options, "context get_id span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_CLIENT, NULL, 0);
	if (_nNULL(span)) {
		if (OTELC_OPS(span, get_id, span_id_orig, sizeof(span_id_orig), trace_id_orig, sizeof(trace_id_orig), &trace_flags_orig) == OTELC_RET_OK) {
			otelc_span_inject_text_map(span, &tm_wr);
			text_map = &(tm_wr.text_map);

			context = otelc_tracer_extract_text_map(tracer, &tm_rd, text_map);
			if (_nNULL(context)) {
				if (OTELC_OPS(context, get_id, span_id_ctx, sizeof(span_id_ctx), trace_id_ctx, sizeof(trace_id_ctx), &trace_flags_ctx) == OTELC_RET_OK) {
					if ((memcmp(span_id_orig, span_id_ctx, OTELC_SPAN_ID_SIZE) == 0) &&
					    (memcmp(trace_id_orig, trace_id_ctx, OTELC_TRACE_ID_SIZE) == 0) &&
					    (trace_flags_orig == trace_flags_ctx))
						retval = TEST_PASS;
				}

				OTELC_OPSR(context, destroy);
			}

			otelc_text_map_destroy(&text_map);
		}

		OTELC_OPSR(span, end);
	}

	test_report("span context get_id", retval);
}


/***
 * NAME
 *   test_span_context_trace_state - tests span context trace state operations
 *
 * SYNOPSIS
 *   static void test_span_context_trace_state(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that the trace state functions work correctly for both non-empty
 *   and empty trace states.  A span is created, its context is injected into
 *   a text map, a tracestate entry is added, and the context is extracted.
 *   The trace state query functions are then tested.  A second extraction
 *   without tracestate verifies the empty case.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_context_trace_state(struct otelc_tracer *tracer)
{
	struct otelc_text_map_writer  tm_wr;
	struct otelc_text_map_reader  tm_rd;
	struct otelc_span_context    *context;
	struct otelc_span            *span;
	struct otelc_text_map        *text_map;
	struct otelc_text_map        *ts_entries;
	struct timespec               ts_steady, ts_system;
	char                          buf[256];
	int                           retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	/* Part 1: non-empty trace state. */
	span = OTELC_OPS(tracer, start_span_with_options, "trace state span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_CLIENT, NULL, 0);
	if (_nNULL(span)) {
		otelc_span_inject_text_map(span, &tm_wr);
		text_map = &(tm_wr.text_map);
		OTELC_TEXT_MAP_ADD(text_map, "tracestate", 0, "vendor1=val1", 0, OTELC_TEXT_MAP_AUTO);

		context = otelc_tracer_extract_text_map(tracer, &tm_rd, text_map);
		if (_nNULL(context)) {
			(void)memset(buf, 0, sizeof(buf));

			ts_entries = OTELC_TEXT_MAP_NEW(NULL, 4);
			if (_nNULL(ts_entries)) {
				if ((OTELC_OPS(context, trace_state_empty) == false) &&
				    (OTELC_OPS(context, trace_state_get, "vendor1", buf, sizeof(buf)) == 4) &&
				    (strcmp(buf, "val1") == 0) &&
				    (OTELC_OPS(context, trace_state_get, "nosuch", buf, sizeof(buf)) == 0) &&
				    (OTELC_OPS(context, trace_state_header, buf, sizeof(buf)) > 0) &&
				    _nNULL(strstr(buf, "vendor1=val1")) &&
				    (OTELC_OPS(context, trace_state_entries, ts_entries) == 1) &&
				    (OTELC_OPS(context, trace_state_set, "vendor2", "val2", buf, sizeof(buf)) > 0) &&
				    _nNULL(strstr(buf, "vendor1=val1")) &&
				    _nNULL(strstr(buf, "vendor2=val2")) &&
				    (OTELC_OPS(context, trace_state_delete, "vendor1", buf, sizeof(buf)) >= 0) &&
				    _NULL(strstr(buf, "vendor1")))
					retval = TEST_PASS;

				otelc_text_map_destroy(&ts_entries);
			}

			OTELC_OPSR(context, destroy);
		}

		otelc_text_map_destroy(&text_map);
		OTELC_OPSR(span, end);
	}

	/* Part 2: empty trace state. */
	if (retval == TEST_PASS) {
		retval = TEST_FAIL;

		span = OTELC_OPS(tracer, start_span_with_options, "empty trace state span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_CLIENT, NULL, 0);
		if (_nNULL(span)) {
			otelc_span_inject_text_map(span, &tm_wr);
			text_map = &(tm_wr.text_map);

			context = otelc_tracer_extract_text_map(tracer, &tm_rd, text_map);
			if (_nNULL(context)) {
				ts_entries = OTELC_TEXT_MAP_NEW(NULL, 4);
				if (_nNULL(ts_entries)) {
					if ((OTELC_OPS(context, trace_state_empty) == true) &&
					    (OTELC_OPS(context, trace_state_get, "vendor1", buf, sizeof(buf)) == 0) &&
					    (OTELC_OPS(context, trace_state_header, buf, sizeof(buf)) == 0) &&
					    (OTELC_OPS(context, trace_state_entries, ts_entries) == 0))
						retval = TEST_PASS;

					otelc_text_map_destroy(&ts_entries);
				}

				OTELC_OPSR(context, destroy);
			}

			otelc_text_map_destroy(&text_map);
			OTELC_OPSR(span, end);
		}
	}

	test_report("span context trace state", retval);
}


/***
 * NAME
 *   test_span_context_invalid_handle - tests span context operations with a
 *                                      corrupted idx
 *
 * SYNOPSIS
 *   static void test_span_context_invalid_handle(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Creates a span context via injection and extraction, saves its idx,
 *   replaces it with a large bogus value that does not exist in the
 *   internal handle map, then attempts all query operations (is_valid,
 *   is_sampled, is_remote, get_id, trace_state_get, trace_state_entries,
 *   trace_state_header, trace_state_empty).  All of these must return
 *   OTELC_RET_ERROR because the handle lookup fails.  The original idx
 *   is restored and the context is destroyed normally.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_context_invalid_handle(struct otelc_tracer *tracer)
{
	struct otelc_text_map_writer  tm_wr;
	struct otelc_text_map_reader  tm_rd;
	struct otelc_span_context    *context;
	struct otelc_span            *span;
	struct otelc_text_map        *text_map;
	struct otelc_text_map        *ts_entries;
	uint8_t                       span_id[OTELC_SPAN_ID_SIZE];
	uint8_t                       trace_id[OTELC_TRACE_ID_SIZE];
	uint8_t                       trace_flags;
	char                          buf[64];
	int64_t                       saved_idx;
	int                           rc, retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "context invalid handle span");
	if (_nNULL(span)) {
		otelc_span_inject_text_map(span, &tm_wr);
		text_map = &(tm_wr.text_map);

		context = otelc_tracer_extract_text_map(tracer, &tm_rd, text_map);
		if (_nNULL(context)) {
			saved_idx = context->idx;
			context->idx = INT64_C(0x7FFFFFFFFFFFFFFF);

			retval = TEST_PASS;

			/* Try is_valid. */
			rc = OTELC_OPS(context, is_valid);
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try is_sampled. */
			rc = OTELC_OPS(context, is_sampled);
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try is_remote. */
			rc = OTELC_OPS(context, is_remote);
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try get_id. */
			rc = OTELC_OPS(context, get_id, span_id, sizeof(span_id), trace_id, sizeof(trace_id), &trace_flags);
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try trace_state_get. */
			rc = OTELC_OPS(context, trace_state_get, "vendor1", buf, sizeof(buf));
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try trace_state_header. */
			rc = OTELC_OPS(context, trace_state_header, buf, sizeof(buf));
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try trace_state_empty. */
			rc = OTELC_OPS(context, trace_state_empty);
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try trace_state_set. */
			rc = OTELC_OPS(context, trace_state_set, "k", "v", buf, sizeof(buf));
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try trace_state_delete. */
			rc = OTELC_OPS(context, trace_state_delete, "k", buf, sizeof(buf));
			if (rc != OTELC_RET_ERROR)
				retval = TEST_FAIL;

			/* Try trace_state_entries. */
			ts_entries = OTELC_TEXT_MAP_NEW(NULL, 4);
			if (_nNULL(ts_entries)) {
				rc = OTELC_OPS(context, trace_state_entries, ts_entries);
				if (rc != OTELC_RET_ERROR)
					retval = TEST_FAIL;

				otelc_text_map_destroy(&ts_entries);
			}

			/* Restore the original idx and destroy normally. */
			context->idx = saved_idx;
			OTELC_OPSR(context, destroy);
		}

		otelc_text_map_destroy(&text_map);
		OTELC_OPSR(span, end);
	}

	test_report("span context invalid handle", retval);
}


/***
 * NAME
 *   test_span_baggage_propagation - tests baggage propagation across contexts
 *
 * SYNOPSIS
 *   static void test_span_baggage_propagation(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that baggage set on a parent span is propagated to a child span
 *   created through context injection and extraction.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_baggage_propagation(struct otelc_tracer *tracer)
{
	struct otelc_text_map_writer  tm_wr;
	struct otelc_text_map_reader  tm_rd;
	struct otelc_span_context    *context;
	struct otelc_span            *parent, *child;
	struct otelc_text_map        *text_map;
	struct timespec               ts_steady, ts_system;
	char                         *value;
	int                           retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	parent = OTELC_OPS(tracer, start_span_with_options, "baggage parent", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_CLIENT, NULL, 0);
	if (_nNULL(parent)) {
		(void)OTELC_OPS(parent, set_baggage_var, "propagated_key", "propagated_value", NULL);

		otelc_span_inject_text_map(parent, &tm_wr);
		text_map = &(tm_wr.text_map);

		context = otelc_tracer_extract_text_map(tracer, &tm_rd, text_map);
		if (_nNULL(context)) {
			child = OTELC_OPS(tracer, start_span_with_options, "baggage child", NULL, context, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0);
			if (_nNULL(child)) {
				value = OTELC_OPS(child, get_baggage, "propagated_key");
				if (_nNULL(value)) {
					if (strcmp(value, "propagated_value") == 0)
						retval = TEST_PASS;

					OTELC_SFREE(value);
				}

				OTELC_OPSR(child, end);
			}

			OTELC_OPSR(context, destroy);
		}

		otelc_text_map_destroy(&text_map);
		OTELC_OPSR(parent, end);
	}

	test_report("baggage propagation", retval);
}


/***
 * NAME
 *   test_span_add_link - tests adding a link between spans
 *
 * SYNOPSIS
 *   static void test_span_add_link(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a link can be added from one span to another using the
 *   add_link() interface with optional link attributes.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_add_link(struct otelc_tracer *tracer)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"link_attr_string", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "linked" }    },
		{ .key = (char *)"link_attr_int64",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(42) } },
	};
	struct otelc_span *span_a, *span_b;
	int                retval = TEST_FAIL;

	span_a = OTELC_OPS(tracer, start_span, "link span a");
	span_b = OTELC_OPS(tracer, start_span, "link span b");

	if (_nNULL(span_a) && _nNULL(span_b)) {
		if (OTELC_OPS(span_b, add_link, span_a, NULL, attr, OTELC_TABLESIZE(attr)) == OTELC_RET_OK)
			retval = TEST_PASS;
	}

	if (_nNULL(span_b))
		OTELC_OPSR(span_b, end);
	if (_nNULL(span_a))
		OTELC_OPSR(span_a, end);

	test_report("span add_link", retval);
}


/***
 * NAME
 *   test_span_add_link_context - tests adding a link via span context
 *
 * SYNOPSIS
 *   static void test_span_add_link_context(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a link can be added from a span to an external span context
 *   created from raw identifiers, using the add_link() interface with a
 *   link_context argument instead of a link_span.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_add_link_context(struct otelc_tracer *tracer)
{
	static const uint8_t trace_id[OTELC_TRACE_ID_SIZE] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
	};
	static const uint8_t span_id[OTELC_SPAN_ID_SIZE] = {
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22
	};
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"link_ctx_attr", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "linked_ctx" } },
	};
	struct otelc_span_context *ctx;
	struct otelc_span         *span;
	char                      *err = NULL;
	int                        retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "link context span");
	if (_nNULL(span)) {
		ctx = otelc_span_context_create(trace_id, sizeof(trace_id), span_id, sizeof(span_id), 0x01, true, NULL, &err);
		if (_nNULL(ctx)) {
			if (OTELC_OPS(span, add_link, NULL, ctx, attr, OTELC_TABLESIZE(attr)) == OTELC_RET_OK)
				retval = TEST_PASS;

			OTELC_OPSR(ctx, destroy);
		}

		OTELC_OPSR(span, end);
	}

	OTELC_SFREE(err);
	test_report("span add_link via context", retval);
}


/***
 * NAME
 *   test_span_record_exception - tests recording an exception event on a span
 *
 * SYNOPSIS
 *   static void test_span_record_exception(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that an exception event can be recorded on a span using the
 *   record_exception() interface, including the standard exception attributes
 *   (type, message, stacktrace) and additional user-defined attributes.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_record_exception(struct otelc_tracer *tracer)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"exception.escaped", .value = { .u_type = OTELC_VALUE_BOOL, .u.value_bool = true } },
	};
	struct otelc_span *span;
	struct timespec    ts_system;
	int                retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	span = OTELC_OPS(tracer, start_span, "exception span");
	if (_nNULL(span)) {
		if (OTELC_OPS(span, record_exception, "RuntimeError", "something broke", "main:42\ninit:10", &ts_system, attr, OTELC_TABLESIZE(attr)) == OTELC_RET_OK)
			retval = TEST_PASS;

		OTELC_OPSR(span, end);
	}

	test_report("span record_exception", retval);
}


/***
 * NAME
 *   test_span_creation_time_links - tests span creation with links
 *
 * SYNOPSIS
 *   static void test_span_creation_time_links(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that spans can be created with links established at creation
 *   time via start_span_with_options().  Tests linking to both a live span
 *   and an externally constructed span context, each carrying its own set
 *   of link attributes.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_creation_time_links(struct otelc_tracer *tracer)
{
	static const uint8_t raw_trace_id[OTELC_TRACE_ID_SIZE] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
	};
	static const uint8_t raw_span_id[OTELC_SPAN_ID_SIZE] = {
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22
	};
	static const struct otelc_kv link_attr_span[] = {
		{ .key = (char *)"link.source", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "span_a" } },
	};
	static const struct otelc_kv link_attr_ctx[] = {
		{ .key = (char *)"link.source", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "external" } },
	};
	struct otelc_span_context *ext_ctx;
	struct otelc_span         *span_a, *span_b, *span_c;
	struct otelc_span_link     links[2];
	struct timespec            ts_steady, ts_system;
	char                      *err = NULL;
	int                        retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	/* Create a span to use as a link target. */
	span_a = OTELC_OPS(tracer, start_span, "link target span");
	if (_NULL(span_a))
		goto out;

	/* Test 1: create a span with a single link to span_a. */
	links[0].span    = span_a;
	links[0].context = NULL;
	links[0].kv      = link_attr_span;
	links[0].kv_len  = OTELC_TABLESIZE(link_attr_span);

	span_b = OTELC_OPS(tracer, start_span_with_options, "linked span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_INTERNAL, links, 1);
	if (_nNULL(span_b)) {
		OTELC_OPSR(span_b, end);

		retval = TEST_PASS;
	}

	/* Test 2: create a span with a link to an external span context. */
	ext_ctx = otelc_span_context_create(raw_trace_id, sizeof(raw_trace_id), raw_span_id, sizeof(raw_span_id), 0x01, true, NULL, &err);
	if (_nNULL(ext_ctx)) {
		links[0].span    = NULL;
		links[0].context = ext_ctx;
		links[0].kv      = link_attr_ctx;
		links[0].kv_len  = OTELC_TABLESIZE(link_attr_ctx);

		span_c = OTELC_OPS(tracer, start_span_with_options, "ctx linked span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_INTERNAL, links, 1);
		if (_nNULL(span_c)) {
			OTELC_OPSR(span_c, end);
		} else {
			retval = TEST_FAIL;
		}

		OTELC_OPSR(ext_ctx, destroy);
	} else {
		retval = TEST_FAIL;
	}

out:
	if (_nNULL(span_a))
		OTELC_OPSR(span_a, end);

	OTELC_SFREE(err);
	test_report("span creation-time links", retval);
}


/***
 * NAME
 *   test_span_set_baggage_single - tests setting a single baggage entry
 *
 * SYNOPSIS
 *   static void test_span_set_baggage_single(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that a single baggage entry can be set and retrieved using the
 *   set_baggage() and get_baggage() interfaces.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_set_baggage_single(struct otelc_tracer *tracer)
{
	struct otelc_span *span;
	char              *value;
	int                rc, retval = TEST_FAIL;

	span = OTELC_OPS(tracer, start_span, "baggage single span");
	if (_nNULL(span)) {
		rc = OTELC_OPS(span, set_baggage, "single_key", "single_value");
		if (rc >= 0) {
			value = OTELC_OPS(span, get_baggage, "single_key");
			if (_nNULL(value)) {
				if (strcmp(value, "single_value") == 0)
					retval = TEST_PASS;

				OTELC_SFREE(value);
			}
		}

		OTELC_OPSR(span, end);
	}

	test_report("span set_baggage single", retval);
}


/***
 * NAME
 *   test_span_invalid_handle - tests operations on a span with a corrupted idx
 *
 * SYNOPSIS
 *   static void test_span_invalid_handle(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Creates a valid span, saves its idx, replaces it with a large bogus value
 *   that does not exist in the internal handle map, then attempts several
 *   operations (child span creation, set_attribute, set_baggage, get_baggage,
 *   add_event, set_status, is_recording).  All of these must fail because the
 *   handle lookup returns null.  The original idx is restored and the span is
 *   ended normally.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_invalid_handle(struct otelc_tracer *tracer)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"inv_attr", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "invalid" } },
	};
	struct otelc_span *span, *child;
	struct timespec    ts_steady, ts_system;
	int64_t            saved_idx;
	char              *value;
	int                rc, retval = TEST_FAIL;

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);
	(void)clock_gettime(CLOCK_REALTIME, &ts_system);

	span = OTELC_OPS(tracer, start_span, "invalid handle span");
	if (_nNULL(span)) {
		saved_idx = span->idx;
		span->idx = INT64_C(0x7FFFFFFFFFFFFFFF);

		retval = TEST_PASS;

		/* Try to create a child span with the corrupted parent. */
		child = OTELC_OPS(tracer, start_span_with_options, "child of invalid", span, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_INTERNAL, NULL, 0);
		if (_nNULL(child)) {
			OTELC_OPSR(child, end);

			retval = TEST_FAIL;
		}

		/* Try to set an attribute. */
		rc = OTELC_OPS(span, set_attribute_var, attr[0].key, &(attr[0].value), NULL);
		if (rc != OTELC_RET_ERROR)
			retval = TEST_FAIL;

		/* Try to set baggage. */
		rc = OTELC_OPS(span, set_baggage_var, "inv_key", "inv_val", NULL);
		if (rc != OTELC_RET_ERROR)
			retval = TEST_FAIL;

		/* Try to get baggage. */
		value = OTELC_OPS(span, get_baggage, "inv_key");
		if (_nNULL(value)) {
			OTELC_SFREE(value);
			retval = TEST_FAIL;
		}

		/* Try to add an event. */
		rc = OTELC_OPS(span, add_event_var, "inv_event", &ts_system, "ev_key", &(attr[0].value), NULL);
		if (rc != OTELC_RET_ERROR)
			retval = TEST_FAIL;

		/* Try to set status. */
		rc = OTELC_OPS(span, set_status, OTELC_SPAN_STATUS_ERROR, "bogus");
		if (rc != OTELC_RET_ERROR)
			retval = TEST_FAIL;

		/* Try to check recording state. */
		rc = OTELC_OPS(span, is_recording);
		if (rc != OTELC_RET_ERROR)
			retval = TEST_FAIL;

		/* Restore the original idx and end the span normally. */
		span->idx = saved_idx;
		OTELC_OPSR(span, end);
	}

	test_report("span invalid handle", retval);
}


/***
 * NAME
 *   test_tracer_enabled - tests whether the tracer reports enabled state
 *
 * SYNOPSIS
 *   static void test_tracer_enabled(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that the enabled() function returns true or false
 *   (not OTELC_RET_ERROR) for a started tracer.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_tracer_enabled(struct otelc_tracer *tracer)
{
	int rc, retval = TEST_PASS;

	rc = OTELC_OPS(tracer, enabled);
	if (rc == OTELC_RET_ERROR)
		retval = TEST_FAIL;

	test_report("tracer enabled", retval);
}


/***
 * NAME
 *   test_span_context_create_basic - tests span context creation from raw IDs
 *
 * SYNOPSIS
 *   static void test_span_context_create_basic(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that a span context can be created from raw trace_id, span_id,
 *   and trace_flags bytes, and that the resulting context reports the correct
 *   validity, remote, sampled, and identifier values.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_context_create_basic(void)
{
	static const uint8_t trace_id[OTELC_TRACE_ID_SIZE] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
	};
	static const uint8_t span_id[OTELC_SPAN_ID_SIZE] = {
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22
	};
	struct otelc_span_context *ctx;
	uint8_t                    out_tid[OTELC_TRACE_ID_SIZE];
	uint8_t                    out_sid[OTELC_SPAN_ID_SIZE];
	uint8_t                    out_flags;
	char                      *err = NULL;
	int                        retval = TEST_FAIL;

	ctx = otelc_span_context_create(trace_id, sizeof(trace_id), span_id, sizeof(span_id), 0x01, true, NULL, &err);
	if (_nNULL(ctx)) {
		if ((OTELC_OPS(ctx, is_valid) == true) &&
		    (OTELC_OPS(ctx, is_remote) == true) &&
		    (OTELC_OPS(ctx, is_sampled) == true))
			retval = TEST_PASS;

		if (OTELC_OPS(ctx, get_id, out_sid, sizeof(out_sid), out_tid, sizeof(out_tid), &out_flags) == OTELC_RET_OK) {
			if ((memcmp(trace_id, out_tid, OTELC_TRACE_ID_SIZE) != 0) ||
			    (memcmp(span_id, out_sid, OTELC_SPAN_ID_SIZE) != 0) ||
			    (out_flags != 0x01))
				retval = TEST_FAIL;
		} else {
			retval = TEST_FAIL;
		}

		OTELC_OPSR(ctx, destroy);
	}

	OTELC_SFREE(err);
	test_report("span_context create basic", retval);
}


/***
 * NAME
 *   test_span_context_create_with_trace_state - tests span context creation with trace state
 *
 * SYNOPSIS
 *   static void test_span_context_create_with_trace_state(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that a span context created with a trace_state_header argument
 *   returns the expected trace state key-value pair and reports the trace
 *   state as non-empty.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_context_create_with_trace_state(void)
{
	static const uint8_t trace_id[OTELC_TRACE_ID_SIZE] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
	};
	static const uint8_t span_id[OTELC_SPAN_ID_SIZE] = {
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22
	};
	struct otelc_span_context *ctx;
	char                       buf[64], *err = NULL;
	int                        retval = TEST_FAIL;

	ctx = otelc_span_context_create(trace_id, sizeof(trace_id), span_id, sizeof(span_id), 0x01, true, "vendor=value", &err);
	if (_nNULL(ctx)) {
		(void)memset(buf, 0, sizeof(buf));

		if ((OTELC_OPS(ctx, trace_state_empty) == false) &&
		    (OTELC_OPS(ctx, trace_state_get, "vendor", buf, sizeof(buf)) == 5) &&
		    (strcmp(buf, "value") == 0))
			retval = TEST_PASS;

		OTELC_OPSR(ctx, destroy);
	}

	OTELC_SFREE(err);
	test_report("span_context create with trace state", retval);
}


/***
 * NAME
 *   test_span_context_create_not_remote - tests span context creation with is_remote false
 *
 * SYNOPSIS
 *   static void test_span_context_create_not_remote(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that a span context created with is_remote set to false reports
 *   is_remote() as false.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_context_create_not_remote(void)
{
	static const uint8_t trace_id[OTELC_TRACE_ID_SIZE] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
	};
	static const uint8_t span_id[OTELC_SPAN_ID_SIZE] = {
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22
	};
	struct otelc_span_context *ctx;
	char                      *err = NULL;
	int                        retval = TEST_FAIL;

	ctx = otelc_span_context_create(trace_id, sizeof(trace_id), span_id, sizeof(span_id), 0x01, false, NULL, &err);
	if (_nNULL(ctx)) {
		if (OTELC_OPS(ctx, is_remote) == false)
			retval = TEST_PASS;

		OTELC_OPSR(ctx, destroy);
	}

	OTELC_SFREE(err);
	test_report("span_context create not remote", retval);
}


/***
 * NAME
 *   test_span_context_create_null_err - tests span context creation with NULL err pointer
 *
 * SYNOPSIS
 *   static void test_span_context_create_null_err(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that otelc_span_context_create() works correctly when the err
 *   argument is NULL.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_span_context_create_null_err(void)
{
	static const uint8_t trace_id[OTELC_TRACE_ID_SIZE] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
	};
	static const uint8_t span_id[OTELC_SPAN_ID_SIZE] = {
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22
	};
	struct otelc_span_context *ctx;
	int                        retval = TEST_FAIL;

	ctx = otelc_span_context_create(trace_id, sizeof(trace_id), span_id, sizeof(span_id), 0x01, true, NULL, NULL);
	if (_nNULL(ctx)) {
		OTELC_OPSR(ctx, destroy);

		retval = TEST_PASS;
	}

	test_report("span_context create with NULL err", retval);
}


/***
 * NAME
 *   test_force_flush - tests forced export of buffered spans
 *
 * SYNOPSIS
 *   static void test_force_flush(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that the force_flush() function completes without errors.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_force_flush(struct otelc_tracer *tracer)
{
	const struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
	int                   retval = TEST_FAIL;

	if ((OTELC_OPS(tracer, force_flush, NULL) == OTELC_RET_OK) &&
	    (OTELC_OPS(tracer, force_flush, &timeout) == OTELC_RET_OK))
		retval = TEST_PASS;

	test_report("force_flush", retval);
}


/***
 * NAME
 *   test_shutdown - tests shutdown of the tracer provider
 *
 * SYNOPSIS
 *   static void test_shutdown(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that the shutdown() function completes without errors using both
 *   an unlimited timeout (NULL) and a specific timeout value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_shutdown(struct otelc_tracer *tracer)
{
	const struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
	int                   retval = TEST_FAIL;

	if ((OTELC_OPS(tracer, shutdown, NULL) == OTELC_RET_OK) &&
	    (OTELC_OPS(tracer, shutdown, &timeout) == OTELC_RET_OK))
		retval = TEST_PASS;

	test_report("shutdown", retval);
}


/***
 * NAME
 *   test_multiple_spans - tests creation of many spans concurrently
 *
 * SYNOPSIS
 *   static void test_multiple_spans(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   Verifies that many spans can be created, held open simultaneously, and
 *   then ended in reverse order without errors.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_multiple_spans(struct otelc_tracer *tracer)
{
#define MULTI_SPAN_COUNT 32
	struct otelc_span *spans[MULTI_SPAN_COUNT];
	char               name[32];
	int                retval = TEST_PASS;
	int                i;

	(void)memset(spans, 0, sizeof(spans));

	for (i = 0; i < MULTI_SPAN_COUNT; i++) {
		(void)snprintf(name, sizeof(name), "multi span %d", i);
		spans[i] = OTELC_OPS(tracer, start_span, name);
		if (_NULL(spans[i])) {
			retval = TEST_FAIL;
			break;
		}
	}

	for (i = MULTI_SPAN_COUNT - 1; i >= 0; i--)
		if (_nNULL(spans[i]))
			OTELC_OPSR(spans[i], end);

	test_report("multiple spans (32)", retval);
#undef MULTI_SPAN_COUNT
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
 *   Initializes the OpenTelemetry library, creates a tracer, runs all tracer
 *   tests, and reports the results.
 *
 * RETURN VALUE
 *   Returns EX_OK if all tests pass, or EX_SOFTWARE if any test fails.
 */
int main(int argc, char **argv)
{
	struct otelc_ctx    *ctx    = NULL;
	struct otelc_tracer *tracer = NULL;
	const char          *cfg_file;
	char                *otel_err = NULL;
	int                  retval;

	retval = test_init(argc, argv, "tracer tests", &cfg_file);
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
	 * Tests that create and destroy tracers in isolation.  These must run
	 * before the main tracer is created because tracer destruction tears
	 * down global state (span/context maps).
	 */
	OTELC_LOG(stdout, "[tracer lifecycle]");
	test_tracer_create_destroy(ctx);
	test_tracer_create_err_null(ctx);

	/***
	 * Create and start the main tracer for the remaining tests.
	 */
	tracer = otelc_tracer_create(ctx, &otel_err);
	if (_NULL(tracer)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to create tracer" : otel_err);

		return test_done(EX_SOFTWARE, otel_err);
	}

	test_set_tracer(&tracer);
	test_tracer_start(tracer);

	/***
	 * Span lifecycle tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[span lifecycle]");
	test_span_start_end(tracer);
	test_span_with_timestamps(tracer);
	test_span_end_with_options(tracer);
	test_span_end_with_status_ignore(tracer);
	test_span_kinds(tracer);
	test_span_parent_child(tracer);
	test_multiple_spans(tracer);

	/***
	 * Span metadata tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[span metadata]");
	test_span_get_id(tracer);
	test_span_is_recording(tracer);
	test_span_set_attribute(tracer);
	test_span_add_event(tracer);
	test_span_add_event_null_timestamp(tracer);
	test_span_set_status(tracer);
	test_span_set_operation_name(tracer);
	test_span_add_link(tracer);
	test_span_add_link_context(tracer);
	test_span_record_exception(tracer);
	test_span_creation_time_links(tracer);
	test_span_invalid_handle(tracer);

	/***
	 * Baggage tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[baggage]");
	test_span_baggage(tracer);
	test_span_baggage_kv(tracer);
	test_span_set_baggage_single(tracer);
	test_span_baggage_propagation(tracer);

	/***
	 * Context propagation tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[context propagation]");
	test_context_propagation_text_map(tracer);
	test_context_propagation_http_headers(tracer);
	test_span_context_queries(tracer);
	test_span_context_get_id(tracer);
	test_span_context_trace_state(tracer);
	test_span_context_invalid_handle(tracer);

	/***
	 * Span context creation.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[span context creation]");
	test_span_context_create_basic();
	test_span_context_create_with_trace_state();
	test_span_context_create_not_remote();
	test_span_context_create_null_err();

	/***
	 * Tracer operations.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[tracer operations]");
	test_tracer_enabled(tracer);
	test_force_flush(tracer);
	test_shutdown(tracer);

	/***
	 * Handle statistics verification.  All spans and span contexts
	 * created during the tests must have been properly destroyed.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[handle statistics]");

	if (otelc_statistics_check(0, 0, 75, 0, 75, 75) != 0)
		retval = TEST_FAIL;
	if (otelc_statistics_check(1, 0, 15, 0, 15, 15) != 0)
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
