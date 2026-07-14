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


#define MULTI_TRACES_A    "__multi_traces_a"
#define MULTI_TRACES_B    "__multi_traces_b"
#define MULTI_METRICS_A   "__multi_metrics_a"
#define MULTI_METRICS_B   "__multi_metrics_b"
#define MULTI_LOGS_A      "__multi_logs_a"
#define MULTI_LOGS_B      "__multi_logs_b"
#ifdef USE_THREADS
#  define MULTI_THREADS   4
#  define MULTI_UPDATES   20000
#endif


/***
 * NAME
 *   test_file_read - reads a whole file into an allocated buffer
 *
 * SYNOPSIS
 *   static char *test_file_read(const char *path)
 *
 * ARGUMENTS
 *   path - name of the file to read
 *
 * DESCRIPTION
 *   Reads the complete contents of the named file into a newly allocated,
 *   NUL-terminated buffer.  The tests use this to inspect the files written
 *   by the per-instance ostream exporters after an instance has been
 *   destroyed, at which point the file is flushed and closed.  The caller
 *   owns the returned buffer and releases it with OTELC_FREE().
 *
 * RETURN VALUE
 *   Returns the allocated buffer with the file contents, or NULL if the file
 *   cannot be read or memory cannot be allocated.
 */
static char *test_file_read(const char *path)
{
	char *retptr = NULL;
	FILE *file;
	long  size;

	file = fopen(path, "rb");
	if (_NULL(file))
		return NULL;

	if ((fseek(file, 0, SEEK_END) == 0) && ((size = ftell(file)) >= 0) && (fseek(file, 0, SEEK_SET) == 0)) {
		retptr = OTELC_MALLOC(__func__, __LINE__, (size_t)size + 1);
		if (_nNULL(retptr)) {
			if (fread(retptr, 1, (size_t)size, file) == (size_t)size) {
				retptr[size] = '\0';
			} else {
				OTELC_FREE(__func__, __LINE__, retptr);

				retptr = NULL;
			}
		}
	}

	(void)fclose(file);

	return retptr;
}


/***
 * NAME
 *   test_file_contains - checks whether a file contains a string
 *
 * SYNOPSIS
 *   static int test_file_contains(const char *path, const char *needle)
 *
 * ARGUMENTS
 *   path   - name of the file to search
 *   needle - string to look for
 *
 * DESCRIPTION
 *   Reads the named file and searches its contents for the given string.
 *   A file that cannot be read is treated as not containing the string.
 *
 * RETURN VALUE
 *   Returns 1 if the file contains the string, 0 otherwise.
 */
static int test_file_contains(const char *path, const char *needle)
{
	char *content;
	int   retval = 0;

	content = test_file_read(path);
	if (_nNULL(content)) {
		retval = _nNULL(strstr(content, needle));

		OTELC_FREE(__func__, __LINE__, content);
	}

	return retval;
}


/***
 * NAME
 *   test_metric_value - extracts an exported counter value from a file
 *
 * SYNOPSIS
 *   static int64_t test_metric_value(const char *path, const char *instrument)
 *
 * ARGUMENTS
 *   path       - name of the file written by the ostream metric exporter
 *   instrument - name of the instrument whose value is extracted
 *
 * DESCRIPTION
 *   Scans the output of the ostream metric exporter for the named instrument
 *   and returns the number found on the next "value" line.  When the file
 *   holds several exports of the same instrument, the value of the last one
 *   wins, which for a cumulative counter is the final total.
 *
 * RETURN VALUE
 *   Returns the extracted value, or -1 if the file cannot be read or the
 *   instrument does not appear in it.
 */
static int64_t test_metric_value(const char *path, const char *instrument)
{
	char       *content, *line, *next;
	const char *sep;
	int64_t     retval = -1;
	int         in_block = 0;

	content = test_file_read(path);
	if (_NULL(content))
		return retval;

	for (line = content; _nNULL(line); line = next) {
		next = strchr(line, '\n');
		if (_nNULL(next))
			*next++ = '\0';

		if (_nNULL(strstr(line, "instrument name"))) {
			in_block = (strstr(line, instrument) != NULL);
		}
		else if ((in_block != 0) && _nNULL(strstr(line, "value"))) {
			sep = strchr(line, ':');
			if (_nNULL(sep))
				retval = (int64_t)strtoll(sep + 1, NULL, 10);

			in_block = 0;
		}
	}

	OTELC_FREE(__func__, __LINE__, content);

	return retval;
}


/***
 * NAME
 *   test_two_tracers_coexist - tests that two tracer instances coexist
 *
 * SYNOPSIS
 *   static void test_two_tracers_coexist(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
 *
 * ARGUMENTS
 *   ctx1 - first library context
 *   ctx2 - second library context
 *
 * DESCRIPTION
 *   Creates two tracer instances against distinct library contexts and verifies
 *   that both ended up with non-null implementation state, with distinct scope
 *   names, distinct yaml prefixes, and distinct implementation pointers (each
 *   impl owns its provider and propagator, so distinct impls imply distinct
 *   providers and propagators).  Starts each tracer, emits one span on each,
 *   and exercises a per-tracer text-map inject/extract round-trip to confirm
 *   that each tracer's propagator works fully on its own, with no process-wide
 *   propagator installed.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_two_tracers_coexist(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
{
	struct otelc_tracer          *primary = NULL, *secondary = NULL;
	struct otelc_span            *span_a = NULL, *span_b = NULL;
	struct otelc_span_context    *sctx_a = NULL, *sctx_b = NULL;
	struct otelc_text_map_writer  tm_wr_a, tm_wr_b;
	struct otelc_text_map_reader  tm_rd_a, tm_rd_b;
	struct otelc_text_map        *map_a = NULL, *map_b = NULL;
	char                         *err_a = NULL, *err_b = NULL;
	int                           result = TEST_FAIL;

	primary   = otelc_tracer_create(ctx1, &err_a);
	secondary = otelc_tracer_create(ctx2, &err_b);

	if (_nNULL(primary) && _nNULL(secondary)
	    && (primary != secondary)
	    && _nNULL(primary->impl) && _nNULL(secondary->impl)
	    && (primary->impl != secondary->impl)
	    && _nNULL(primary->yaml_prefix) && _nNULL(secondary->yaml_prefix)
	    && (strcmp(primary->yaml_prefix, secondary->yaml_prefix) != 0)) {
		if ((OTELC_OPS(primary, start) == OTELC_RET_OK)
		    && (OTELC_OPS(secondary, start) == OTELC_RET_OK)
		    && _nNULL(primary->scope_name)
		    && _nNULL(secondary->scope_name)
		    && (strcmp(primary->scope_name, secondary->scope_name) != 0)) {
			span_a = OTELC_OPS(primary,   start_span, "primary-op");
			span_b = OTELC_OPS(secondary, start_span, "secondary-op");

			if (_nNULL(span_a) && _nNULL(span_b)
			    && (span_a->tracer == primary)
			    && (span_b->tracer == secondary)) {
				/* Round-trip via each tracer's per-instance propagator. */
				if ((otelc_span_inject_text_map(span_a, &tm_wr_a) == OTELC_RET_OK)
				    && (otelc_span_inject_text_map(span_b, &tm_wr_b) == OTELC_RET_OK)) {
					map_a  = &(tm_wr_a.text_map);
					map_b  = &(tm_wr_b.text_map);
					sctx_a = otelc_tracer_extract_text_map(primary,   &tm_rd_a, map_a);
					sctx_b = otelc_tracer_extract_text_map(secondary, &tm_rd_b, map_b);

					if (_nNULL(sctx_a) && _nNULL(sctx_b))
						result = TEST_PASS;
				}
			}

			if (_nNULL(sctx_a))
				OTELC_OPSR(sctx_a, destroy);
			if (_nNULL(sctx_b))
				OTELC_OPSR(sctx_b, destroy);
			if (_nNULL(map_a))
				otelc_text_map_destroy(&map_a);
			if (_nNULL(map_b))
				otelc_text_map_destroy(&map_b);

			if (_nNULL(span_a))
				OTELC_OPSR(span_a, end);
			if (_nNULL(span_b))
				OTELC_OPSR(span_b, end);
		}
	}

	otelc_deinit(NULL, &primary,   NULL, NULL);
	otelc_deinit(NULL, &secondary, NULL, NULL);

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("two tracers coexist with independent providers and propagators", result);
}


/***
 * NAME
 *   test_tracer_destroy_order - tests destroying tracers in either order
 *
 * SYNOPSIS
 *   static void test_tracer_destroy_order(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
 *
 * ARGUMENTS
 *   ctx1 - first library context
 *   ctx2 - second library context
 *
 * DESCRIPTION
 *   Creates two tracers against distinct library contexts, starts both, opens a
 *   span on each, then destroys the FIRST tracer while the second is still in
 *   use.  After the first destroy the second tracer must still be able to start
 *   and end spans without crashing -- the shared span and span-context handle
 *   maps stay alive until the last tracer is destroyed thanks to the tracer
 *   refcount that guards them.  The README documents destroying tracers in
 *   either order as safe; this test pins that contract down.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_tracer_destroy_order(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
{
	struct otelc_tracer *primary = NULL, *secondary = NULL;
	struct otelc_span   *span_a = NULL, *span_b = NULL;
	char                *err_a = NULL, *err_b = NULL;
	int                  result = TEST_FAIL;

	primary   = otelc_tracer_create(ctx1, &err_a);
	secondary = otelc_tracer_create(ctx2, &err_b);

	if (_nNULL(primary) && _nNULL(secondary)
	    && (OTELC_OPS(primary, start) == OTELC_RET_OK)
	    && (OTELC_OPS(secondary, start) == OTELC_RET_OK)) {
		span_a = OTELC_OPS(primary,   start_span, "primary-a");
		span_b = OTELC_OPS(secondary, start_span, "secondary-a");
		if (_nNULL(span_a))
			OTELC_OPSR(span_a, end);

		/* Destroy the FIRST tracer while the second is still alive. */
		OTELC_OPSR(primary, destroy);

		/* Second tracer must still work. */
		struct otelc_span *span_c = OTELC_OPS(secondary, start_span, "secondary-b");
		if (_nNULL(span_b) && _nNULL(span_c)) {
			OTELC_OPSR(span_b, end);
			OTELC_OPSR(span_c, end);
			result = TEST_PASS;
		}
	}

	if (_nNULL(secondary))
		OTELC_OPSR(secondary, destroy);
	if (_nNULL(primary))
		OTELC_OPSR(primary, destroy);

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("first tracer destroy keeps second alive", result);
}


/***
 * NAME
 *   test_two_meters_coexist - tests that two meter instances coexist
 *
 * SYNOPSIS
 *   static void test_two_meters_coexist(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
 *
 * ARGUMENTS
 *   ctx1 - first library context
 *   ctx2 - second library context
 *
 * DESCRIPTION
 *   Creates two meter instances against distinct library contexts and verifies
 *   that both ended up with non-null implementation state, with distinct scope
 *   names, distinct yaml prefixes, and distinct implementation pointers (so
 *   each meter owns its own provider).  Starts each meter and creates a counter
 *   instrument on each to confirm that both are functional.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_two_meters_coexist(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
{
	struct otelc_meter *primary = NULL, *secondary = NULL;
	char               *err_a = NULL, *err_b = NULL;
	int64_t             id_a = -1, id_b = -1;
	int                 result = TEST_FAIL;

	primary   = otelc_meter_create(ctx1, &err_a);
	secondary = otelc_meter_create(ctx2, &err_b);

	if (_nNULL(primary) && _nNULL(secondary)
	    && (primary != secondary)
	    && _nNULL(primary->impl) && _nNULL(secondary->impl)
	    && (primary->impl != secondary->impl)
	    && _nNULL(primary->yaml_prefix) && _nNULL(secondary->yaml_prefix)
	    && (strcmp(primary->yaml_prefix, secondary->yaml_prefix) != 0)) {
		if ((OTELC_OPS(primary, start) == OTELC_RET_OK)
		    && (OTELC_OPS(secondary, start) == OTELC_RET_OK)
		    && _nNULL(primary->scope_name)
		    && _nNULL(secondary->scope_name)
		    && (strcmp(primary->scope_name, secondary->scope_name) != 0)) {
			id_a = OTELC_OPS(primary,   create_instrument, "primary_counter",   "primary counter",   "items", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
			id_b = OTELC_OPS(secondary, create_instrument, "secondary_counter", "secondary counter", "items", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
			if ((id_a >= 0) && (id_b >= 0))
				result = TEST_PASS;
		}
	}

	otelc_deinit(NULL, NULL, &primary,   NULL);
	otelc_deinit(NULL, NULL, &secondary, NULL);

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("two meters coexist with independent providers", result);
}


/***
 * NAME
 *   test_two_loggers_coexist - tests that two logger instances coexist
 *
 * SYNOPSIS
 *   static void test_two_loggers_coexist(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
 *
 * ARGUMENTS
 *   ctx1 - first library context
 *   ctx2 - second library context
 *
 * DESCRIPTION
 *   Creates two logger instances against distinct library contexts and verifies
 *   that both ended up with non-null implementation state, with distinct scope
 *   names, distinct yaml prefixes, and distinct implementation pointers (so
 *   each logger owns its own provider).  Starts each logger and emits one INFO
 *   record on each to confirm that both are functional.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_two_loggers_coexist(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
{
	struct otelc_logger *primary = NULL, *secondary = NULL;
	char                *err_a = NULL, *err_b = NULL;
	int                  result = TEST_FAIL;

	primary   = otelc_logger_create(ctx1, &err_a);
	secondary = otelc_logger_create(ctx2, &err_b);

	if (_nNULL(primary) && _nNULL(secondary)
	    && (primary != secondary)
	    && _nNULL(primary->impl) && _nNULL(secondary->impl)
	    && (primary->impl != secondary->impl)
	    && _nNULL(primary->yaml_prefix) && _nNULL(secondary->yaml_prefix)
	    && (strcmp(primary->yaml_prefix, secondary->yaml_prefix) != 0)) {
		if ((OTELC_OPS(primary, start) == OTELC_RET_OK)
		    && (OTELC_OPS(secondary, start) == OTELC_RET_OK)
		    && _nNULL(primary->scope_name)
		    && _nNULL(secondary->scope_name)
		    && (strcmp(primary->scope_name, secondary->scope_name) != 0)) {
			if ((OTELC_OPS(primary,   log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, NULL, 0, "primary log")   >= 0)
			    && (OTELC_OPS(secondary, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, NULL, 0, "secondary log") >= 0))
				result = TEST_PASS;
		}
	}

	otelc_deinit(NULL, NULL, NULL, &primary);
	otelc_deinit(NULL, NULL, NULL, &secondary);

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("two loggers coexist with independent providers", result);
}


/***
 * NAME
 *   test_two_meters_distinct_instrument_maps - tests per-meter instrument maps
 *
 * SYNOPSIS
 *   static void test_two_meters_distinct_instrument_maps(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
 *
 * ARGUMENTS
 *   ctx1 - first library context
 *   ctx2 - second library context
 *
 * DESCRIPTION
 *   Creates two meters against distinct library contexts and on each one
 *   creates an instrument with the same name and type.  The instrument and
 *   view handle maps live inside each meter's per-instance state, so the
 *   two create_instrument calls populate independent maps.  After both
 *   creations, the test asserts via otelc_statistics_check() that each
 *   meter's instrument map holds exactly one entry with its id counter at
 *   1, proving the maps are not shared between meters.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_two_meters_distinct_instrument_maps(const struct otelc_ctx *ctx1, const struct otelc_ctx *ctx2)
{
	struct otelc_meter *primary = NULL, *secondary = NULL;
	char               *err_a = NULL, *err_b = NULL;
	int64_t             id_a = OTELC_RET_ERROR, id_b = OTELC_RET_ERROR;
	int                 result = TEST_FAIL;

	primary   = otelc_meter_create(ctx1, &err_a);
	secondary = otelc_meter_create(ctx2, &err_b);

	if (_nNULL(primary) && _nNULL(secondary)
	    && (OTELC_OPS(primary, start) == OTELC_RET_OK)
	    && (OTELC_OPS(secondary, start) == OTELC_RET_OK)) {
		id_a = OTELC_OPS(primary,   create_instrument, "shared_counter", "shared", "1",
		                 OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
		id_b = OTELC_OPS(secondary, create_instrument, "shared_counter", "shared", "1",
		                 OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);

		if ((id_a != OTELC_RET_ERROR) && (id_b != OTELC_RET_ERROR)
		    && (otelc_statistics_check(primary,   2, 1, 1, 0, 0, 0) == 0)
		    && (otelc_statistics_check(secondary, 2, 1, 1, 0, 0, 0) == 0))
			result = TEST_PASS;
	}

	otelc_deinit(NULL, NULL, &primary,   NULL);
	otelc_deinit(NULL, NULL, &secondary, NULL);

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("two meters have distinct instrument maps", result);
}


/***
 * NAME
 *   test_tracers_span_isolation - tests per-instance span export isolation
 *
 * SYNOPSIS
 *   static void test_tracers_span_isolation(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
 *
 * ARGUMENTS
 *   ctx_a - library context exporting traces to the file MULTI_TRACES_A
 *   ctx_b - library context exporting traces to the file MULTI_TRACES_B
 *
 * DESCRIPTION
 *   Creates a tracer on each context, emits a span with a distinctive name on
 *   each, and destroys both tracers so the simple processors export the spans
 *   and the exporter files are flushed and closed.  Each file must contain
 *   the span emitted by its own instance and must not contain the span of the
 *   other instance, which proves that spans never leak across instances.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_tracers_span_isolation(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
{
	struct otelc_tracer *tracer_a = NULL, *tracer_b = NULL;
	struct otelc_span   *span_a = NULL, *span_b = NULL;
	char                *err_a = NULL, *err_b = NULL;
	int                  result = TEST_FAIL;

	tracer_a = otelc_tracer_create(ctx_a, &err_a);
	tracer_b = otelc_tracer_create(ctx_b, &err_b);

	if (_nNULL(tracer_a) && _nNULL(tracer_b)
	    && (OTELC_OPS(tracer_a, start) == OTELC_RET_OK)
	    && (OTELC_OPS(tracer_b, start) == OTELC_RET_OK)) {
		span_a = OTELC_OPS(tracer_a, start_span, "multi-span-alpha");
		span_b = OTELC_OPS(tracer_b, start_span, "multi-span-bravo");

		if (_nNULL(span_a) && _nNULL(span_b)) {
			OTELC_OPSR(span_a, end);
			OTELC_OPSR(span_b, end);

			result = TEST_PASS;
		}
	}

	otelc_deinit(NULL, &tracer_a, NULL, NULL);
	otelc_deinit(NULL, &tracer_b, NULL, NULL);

	if ((result == TEST_PASS)
	    && (test_file_contains(MULTI_TRACES_A, "multi-span-alpha") == 1)
	    && (test_file_contains(MULTI_TRACES_A, "multi-span-bravo") == 0)
	    && (test_file_contains(MULTI_TRACES_B, "multi-span-bravo") == 1)
	    && (test_file_contains(MULTI_TRACES_B, "multi-span-alpha") == 0))
		result = TEST_PASS;
	else
		result = TEST_FAIL;

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("spans are exported only by their own instance", result);
}


/***
 * NAME
 *   test_tracer_destroy_leftover_spans - tests teardown with spans left open
 *
 * SYNOPSIS
 *   static void test_tracer_destroy_leftover_spans(const struct otelc_ctx *ctx_a)
 *
 * ARGUMENTS
 *   ctx_a - library context exporting traces to the file MULTI_TRACES_A
 *
 * DESCRIPTION
 *   Creates a tracer -- the only one alive -- starts spans on it, and then
 *   destroys the tracer while those spans are still open.  The teardown must
 *   end the leftover spans implicitly and release their span handles, and the
 *   per-instance exporter file must contain the span names after the destroy.
 *   A late destroy or end of the leftover span structures after the teardown
 *   must be safe: both release only the C structure and clear the caller's
 *   pointer.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_tracer_destroy_leftover_spans(const struct otelc_ctx *ctx_a)
{
	struct otelc_tracer *tracer = NULL;
	struct otelc_span   *span_a = NULL, *span_b = NULL;
	char                *err = NULL;
	int                  result = TEST_FAIL;

	tracer = otelc_tracer_create(ctx_a, &err);

	if (_nNULL(tracer) && (OTELC_OPS(tracer, start) == OTELC_RET_OK)) {
		span_a = OTELC_OPS(tracer, start_span, "leftover-alpha");
		span_b = OTELC_OPS(tracer, start_span, "leftover-bravo");

		if (_nNULL(span_a) && _nNULL(span_b)) {
			/* Destroy the LAST tracer while both spans are still open. */
			otelc_deinit(NULL, &tracer, NULL, NULL);

			/* Late destroy and late end must only release the C structures. */
			OTELC_OPSR(span_a, destroy);
			OTELC_OPSR(span_b, end);

			if (_NULL(tracer) && _NULL(span_a) && _NULL(span_b))
				result = TEST_PASS;
		} else {
			if (_nNULL(span_a))
				OTELC_OPSR(span_a, destroy);
			if (_nNULL(span_b))
				OTELC_OPSR(span_b, destroy);
		}
	}

	otelc_deinit(NULL, &tracer, NULL, NULL);

	if ((result == TEST_PASS)
	    && (test_file_contains(MULTI_TRACES_A, "leftover-alpha") == 1)
	    && (test_file_contains(MULTI_TRACES_A, "leftover-bravo") == 1))
		result = TEST_PASS;
	else
		result = TEST_FAIL;

	OTELC_SFREE(err);

	test_report("last tracer destroy exports and releases leftover spans", result);
}


/***
 * NAME
 *   test_loggers_record_isolation - tests per-instance log export isolation
 *
 * SYNOPSIS
 *   static void test_loggers_record_isolation(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
 *
 * ARGUMENTS
 *   ctx_a - library context exporting logs to the file MULTI_LOGS_A
 *   ctx_b - library context exporting logs to the file MULTI_LOGS_B
 *
 * DESCRIPTION
 *   Creates a logger on each context, emits a record with a distinctive body
 *   on each, and destroys both loggers so the simple processors export the
 *   records and the exporter files are flushed and closed.  Each file must
 *   contain the record emitted by its own instance and must not contain the
 *   record of the other instance.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_loggers_record_isolation(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
{
	struct otelc_logger *logger_a = NULL, *logger_b = NULL;
	char                *err_a = NULL, *err_b = NULL;
	int                  result = TEST_FAIL;

	logger_a = otelc_logger_create(ctx_a, &err_a);
	logger_b = otelc_logger_create(ctx_b, &err_b);

	if (_nNULL(logger_a) && _nNULL(logger_b)
	    && (OTELC_OPS(logger_a, start) == OTELC_RET_OK)
	    && (OTELC_OPS(logger_b, start) == OTELC_RET_OK)
	    && (OTELC_OPS(logger_a, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, NULL, 0, "multi-log-alpha") >= 0)
	    && (OTELC_OPS(logger_b, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, NULL, 0, "multi-log-bravo") >= 0))
		result = TEST_PASS;

	otelc_deinit(NULL, NULL, NULL, &logger_a);
	otelc_deinit(NULL, NULL, NULL, &logger_b);

	if ((result == TEST_PASS)
	    && (test_file_contains(MULTI_LOGS_A, "multi-log-alpha") == 1)
	    && (test_file_contains(MULTI_LOGS_A, "multi-log-bravo") == 0)
	    && (test_file_contains(MULTI_LOGS_B, "multi-log-bravo") == 1)
	    && (test_file_contains(MULTI_LOGS_B, "multi-log-alpha") == 0))
		result = TEST_PASS;
	else
		result = TEST_FAIL;

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("log records are exported only by their own instance", result);
}


/***
 * NAME
 *   test_meters_value_isolation - tests per-instance metric value isolation
 *
 * SYNOPSIS
 *   static void test_meters_value_isolation(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
 *
 * ARGUMENTS
 *   ctx_a - library context exporting metrics to the file MULTI_METRICS_A
 *   ctx_b - library context exporting metrics to the file MULTI_METRICS_B
 *
 * DESCRIPTION
 *   Creates a meter on each context and on both of them a counter with the
 *   same name, plus one counter that exists only on the first meter.  Each
 *   counter is updated with a value unique to its instance, and the meters
 *   are destroyed, which force-flushes the readers and closes the exporter
 *   files.  The exported files must show exactly the per-instance totals for
 *   the shared name, the first-instance-only counter must not appear in the
 *   second file, and no value may leak from one instance to the other.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meters_value_isolation(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
{
	struct otelc_meter *meter_a = NULL, *meter_b = NULL;
	struct otelc_value  value;
	char               *err_a = NULL, *err_b = NULL;
	int64_t             id_a, id_a2, id_b;
	int                 result = TEST_FAIL;

	value.u_type = OTELC_VALUE_UINT64;

	meter_a = otelc_meter_create(ctx_a, &err_a);
	meter_b = otelc_meter_create(ctx_b, &err_b);

	if (_nNULL(meter_a) && _nNULL(meter_b)
	    && (OTELC_OPS(meter_a, start) == OTELC_RET_OK)
	    && (OTELC_OPS(meter_b, start) == OTELC_RET_OK)) {
		id_a  = OTELC_OPS(meter_a, create_instrument, "multi_counter",  "shared name", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
		id_a2 = OTELC_OPS(meter_a, create_instrument, "only_a_counter", "only on A",   "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
		id_b  = OTELC_OPS(meter_b, create_instrument, "multi_counter",  "shared name", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);

		if ((id_a != OTELC_RET_ERROR) && (id_a2 != OTELC_RET_ERROR) && (id_b != OTELC_RET_ERROR)) {
			value.u.value_uint64 = 31415;
			if (OTELC_OPS(meter_a, update_instrument, id_a, &value) != OTELC_RET_ERROR) {
				value.u.value_uint64 = 111;
				if (OTELC_OPS(meter_a, update_instrument, id_a2, &value) != OTELC_RET_ERROR) {
					value.u.value_uint64 = 27182;
					if (OTELC_OPS(meter_b, update_instrument, id_b, &value) != OTELC_RET_ERROR)
						result = TEST_PASS;
				}
			}
		}
	}

	otelc_deinit(NULL, NULL, &meter_a, NULL);
	otelc_deinit(NULL, NULL, &meter_b, NULL);

	if ((result == TEST_PASS)
	    && (test_metric_value(MULTI_METRICS_A, "multi_counter") == 31415)
	    && (test_metric_value(MULTI_METRICS_A, "only_a_counter") == 111)
	    && (test_metric_value(MULTI_METRICS_B, "multi_counter") == 27182)
	    && (test_file_contains(MULTI_METRICS_B, "only_a_counter") == 0))
		result = TEST_PASS;
	else
		result = TEST_FAIL;

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("same-name counters keep per-instance values", result);
}


/***
 * NAME
 *   test_meters_instrument_id_spaces - tests per-instance instrument ID spaces
 *
 * SYNOPSIS
 *   static void test_meters_instrument_id_spaces(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
 *
 * ARGUMENTS
 *   ctx_a - first library context
 *   ctx_b - second library context
 *
 * DESCRIPTION
 *   Creates two meters and registers a different number of instruments on
 *   each, so that an instrument ID valid on the first meter has no meaning on
 *   the second one.  Updating the second meter with the foreign ID must fail,
 *   updating the first meter with it must succeed, and get_instrument() must
 *   resolve each name only on the meter that owns it.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meters_instrument_id_spaces(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
{
	struct otelc_meter *meter_a = NULL, *meter_b = NULL;
	struct otelc_value  value;
	char               *err_a = NULL, *err_b = NULL;
	int64_t             id_alpha, id_beta, id_gamma;
	int                 result = TEST_FAIL;

	value.u_type         = OTELC_VALUE_UINT64;
	value.u.value_uint64 = 1;

	meter_a = otelc_meter_create(ctx_a, &err_a);
	meter_b = otelc_meter_create(ctx_b, &err_b);

	if (_nNULL(meter_a) && _nNULL(meter_b)
	    && (OTELC_OPS(meter_a, start) == OTELC_RET_OK)
	    && (OTELC_OPS(meter_b, start) == OTELC_RET_OK)) {
		id_alpha = OTELC_OPS(meter_a, create_instrument, "alpha_counter", "A first",  "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
		id_beta  = OTELC_OPS(meter_a, create_instrument, "beta_counter",  "A second", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
		id_gamma = OTELC_OPS(meter_b, create_instrument, "gamma_counter", "B first",  "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);

		if ((id_alpha != OTELC_RET_ERROR) && (id_beta != OTELC_RET_ERROR) && (id_gamma != OTELC_RET_ERROR)
		    && (id_beta != id_gamma)
		    && (OTELC_OPS(meter_a, get_instrument, "beta_counter",  OTELC_METRIC_INSTRUMENT_COUNTER_UINT64) == id_beta)
		    && (OTELC_OPS(meter_b, get_instrument, "gamma_counter", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64) == id_gamma)
		    && (OTELC_OPS(meter_b, get_instrument, "beta_counter",  OTELC_METRIC_INSTRUMENT_COUNTER_UINT64) == OTELC_RET_ERROR)
		    && (OTELC_OPS(meter_b, update_instrument, id_beta, &value) == OTELC_RET_ERROR)
		    && (OTELC_OPS(meter_a, update_instrument, id_beta, &value) != OTELC_RET_ERROR))
			result = TEST_PASS;
	}

	otelc_deinit(NULL, NULL, &meter_a, NULL);
	otelc_deinit(NULL, NULL, &meter_b, NULL);

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("instrument ids are meaningless on a foreign meter", result);
}


/***
 * NAME
 *   test_meter_destroy_order - tests destroying meters in either order
 *
 * SYNOPSIS
 *   static void test_meter_destroy_order(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
 *
 * ARGUMENTS
 *   ctx_a - first library context
 *   ctx_b - second library context
 *
 * DESCRIPTION
 *   Creates two meters, updates a counter on each, then destroys the FIRST
 *   meter while the second is still in use.  After the first destroy the
 *   second meter must still create instruments, record values and flush.
 *   The exported file of the second instance must show the totals recorded
 *   both before and after the first destroy.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_destroy_order(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
{
	struct otelc_meter *meter_a = NULL, *meter_b = NULL;
	struct otelc_value  value;
	char               *err_a = NULL, *err_b = NULL;
	int64_t             id_a, id_b, id_b2;
	int                 result = TEST_FAIL;

	value.u_type = OTELC_VALUE_UINT64;

	meter_a = otelc_meter_create(ctx_a, &err_a);
	meter_b = otelc_meter_create(ctx_b, &err_b);

	if (_nNULL(meter_a) && _nNULL(meter_b)
	    && (OTELC_OPS(meter_a, start) == OTELC_RET_OK)
	    && (OTELC_OPS(meter_b, start) == OTELC_RET_OK)) {
		id_a = OTELC_OPS(meter_a, create_instrument, "doomed_counter",   "on A", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
		id_b = OTELC_OPS(meter_b, create_instrument, "survivor_counter", "on B", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);

		if ((id_a != OTELC_RET_ERROR) && (id_b != OTELC_RET_ERROR)) {
			value.u.value_uint64 = 5;
			if ((OTELC_OPS(meter_a, update_instrument, id_a, &value) != OTELC_RET_ERROR)
			    && (OTELC_OPS(meter_b, update_instrument, id_b, &value) != OTELC_RET_ERROR)) {
				/* Destroy the FIRST meter while the second is still alive. */
				otelc_deinit(NULL, NULL, &meter_a, NULL);

				/* Second meter must still record, register and flush. */
				id_b2 = OTELC_OPS(meter_b, create_instrument, "late_counter", "after destroy", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);

				value.u.value_uint64 = 7;
				if ((id_b2 != OTELC_RET_ERROR)
				    && (OTELC_OPS(meter_b, update_instrument, id_b, &value) != OTELC_RET_ERROR)
				    && (OTELC_OPS(meter_b, update_instrument, id_b2, &value) != OTELC_RET_ERROR)
				    && (OTELC_OPS(meter_b, force_flush, NULL) == OTELC_RET_OK))
					result = TEST_PASS;
			}
		}
	}

	otelc_deinit(NULL, NULL, &meter_a, NULL);
	otelc_deinit(NULL, NULL, &meter_b, NULL);

	if ((result == TEST_PASS)
	    && (test_metric_value(MULTI_METRICS_B, "survivor_counter") == 12)
	    && (test_metric_value(MULTI_METRICS_B, "late_counter") == 7))
		result = TEST_PASS;
	else
		result = TEST_FAIL;

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("first meter destroy keeps second alive", result);
}


#ifdef USE_THREADS

/* Work order and result of one concurrent metric-update worker. */
struct multi_update_worker {
	struct otelc_meter *meter;  /* Meter that receives the updates. */
	pthread_t           thread; /* Thread executing the worker. */
	uint64_t            add;    /* Value added by every update. */
	int                 rc;     /* Worker result: TEST_PASS or TEST_FAIL. */
};


/***
 * NAME
 *   test_multi_update_worker - concurrently registers and updates a counter
 *
 * SYNOPSIS
 *   static void *test_multi_update_worker(void *data)
 *
 * ARGUMENTS
 *   data - pointer to the worker's multi_update_worker structure
 *
 * DESCRIPTION
 *   Calls create_instrument() for the shared counter name, so all workers of
 *   a meter race the creation path and all but one resolve the existing
 *   instrument, then applies MULTI_UPDATES updates of the worker's value.
 *   Any failed call marks the worker as failed.
 *
 * RETURN VALUE
 *   This function always returns NULL; the outcome is stored in the rc
 *   member of the worker structure.
 */
static void *test_multi_update_worker(void *data)
{
	struct multi_update_worker *worker = data;
	struct otelc_value          value;
	int64_t                     id;
	int                         i;

	worker->rc = TEST_FAIL;

	value.u_type         = OTELC_VALUE_UINT64;
	value.u.value_uint64 = worker->add;

	id = OTELC_OPS(worker->meter, create_instrument, "herd_counter", "herd", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
	if (id == OTELC_RET_ERROR)
		return NULL;

	for (i = 0; i < MULTI_UPDATES; i++)
		if (OTELC_OPS(worker->meter, update_instrument, id, &value) == OTELC_RET_ERROR)
			return NULL;

	worker->rc = TEST_PASS;

	return NULL;
}


/***
 * NAME
 *   test_meters_concurrent_updates - tests concurrent updates on two meters
 *
 * SYNOPSIS
 *   static void test_meters_concurrent_updates(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
 *
 * ARGUMENTS
 *   ctx_a - library context exporting metrics to the file MULTI_METRICS_A
 *   ctx_b - library context exporting metrics to the file MULTI_METRICS_B
 *
 * DESCRIPTION
 *   Starts MULTI_THREADS workers on each of two meters.  Every worker first
 *   races create_instrument() for the same counter name and then applies
 *   MULTI_UPDATES updates, with all workers of the first meter adding 1 and
 *   all workers of the second meter adding 2.  After the workers join and
 *   the meters are destroyed, each exported file must show exactly the total
 *   of its own meter, which proves that no update was lost, that the racing
 *   registrations resolved to a single instrument per meter, and that no
 *   value crossed between the instances.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meters_concurrent_updates(const struct otelc_ctx *ctx_a, const struct otelc_ctx *ctx_b)
{
	struct multi_update_worker  worker[2 * MULTI_THREADS];
	struct otelc_meter         *meter_a = NULL, *meter_b = NULL;
	char                       *err_a = NULL, *err_b = NULL;
	int                         i, started = 0, result = TEST_FAIL;

	meter_a = otelc_meter_create(ctx_a, &err_a);
	meter_b = otelc_meter_create(ctx_b, &err_b);

	if (_nNULL(meter_a) && _nNULL(meter_b)
	    && (OTELC_OPS(meter_a, start) == OTELC_RET_OK)
	    && (OTELC_OPS(meter_b, start) == OTELC_RET_OK)) {
		for (i = 0; i < (2 * MULTI_THREADS); i++) {
			worker[i].meter = ((i % 2) == 0) ? meter_a : meter_b;
			worker[i].add   = ((i % 2) == 0) ? 1 : 2;
			worker[i].rc    = TEST_FAIL;

			if (pthread_create(&(worker[i].thread), NULL, test_multi_update_worker, worker + i) != 0)
				break;

			started++;
		}

		result = (started == (2 * MULTI_THREADS)) ? TEST_PASS : TEST_FAIL;

		for (i = 0; i < started; i++)
			if ((pthread_join(worker[i].thread, NULL) != 0) || (worker[i].rc != TEST_PASS))
				result = TEST_FAIL;
	}

	otelc_deinit(NULL, NULL, &meter_a, NULL);
	otelc_deinit(NULL, NULL, &meter_b, NULL);

	if ((result == TEST_PASS)
	    && (test_metric_value(MULTI_METRICS_A, "herd_counter") == (int64_t)(MULTI_THREADS * MULTI_UPDATES))
	    && (test_metric_value(MULTI_METRICS_B, "herd_counter") == (int64_t)(2 * MULTI_THREADS * MULTI_UPDATES)))
		result = TEST_PASS;
	else
		result = TEST_FAIL;

	OTELC_SFREE(err_a);
	OTELC_SFREE(err_b);

	test_report("concurrent updates keep exact per-instance totals", result);
}

#endif /* USE_THREADS */


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
 *   Loads the configuration file, opens two library contexts ("default" and
 *   "secondary"), runs the multi-instance tracer, meter and logger tests
 *   against them, and then opens the "multi_a" and "multi_b" contexts, whose
 *   ostream exporters write to per-instance files, and runs the isolation
 *   tests that verify the exported data against those files.  Finally the
 *   contexts are released and the results are reported.
 *
 * RETURN VALUE
 *   Returns EX_OK if all tests pass, or EX_SOFTWARE if any test fails.
 */
int main(int argc, char **argv)
{
	struct otelc_ctx *ctx[2] = { NULL, NULL };
	struct otelc_ctx *ctx_multi[2] = { NULL, NULL };
	const char       *cfg_file;
	char             *otel_err = NULL;
	int               retval;

	retval = test_init(argc, argv, "multi-instance tests", &cfg_file);
	if (retval >= 0)
		return retval;

	retval = EX_OK;
	OTELC_LOG(stdout, "");

	ctx[0] = otelc_init(cfg_file, "default", &otel_err);
	if (_NULL(ctx[0])) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init library" : otel_err);

		return test_done(EX_SOFTWARE, otel_err);
	}
	ctx[1] = otelc_init(cfg_file, "secondary", &otel_err);
	if (_NULL(ctx[1])) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init library" : otel_err);

		otelc_deinit(&(ctx[0]), NULL, NULL, NULL);

		return test_done(EX_SOFTWARE, otel_err);
	}
	ctx_multi[0] = otelc_init(cfg_file, "multi_a", &otel_err);
	if (_NULL(ctx_multi[0])) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init library" : otel_err);

		otelc_deinit(&(ctx[0]), NULL, NULL, NULL);
		otelc_deinit(&(ctx[1]), NULL, NULL, NULL);

		return test_done(EX_SOFTWARE, otel_err);
	}
	ctx_multi[1] = otelc_init(cfg_file, "multi_b", &otel_err);
	if (_NULL(ctx_multi[1])) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init library" : otel_err);

		otelc_deinit(&(ctx[0]), NULL, NULL, NULL);
		otelc_deinit(&(ctx[1]), NULL, NULL, NULL);
		otelc_deinit(&(ctx_multi[0]), NULL, NULL, NULL);

		return test_done(EX_SOFTWARE, otel_err);
	}

	OTELC_LOG(stdout, "[multi-tracer]");
	test_two_tracers_coexist(ctx[0], ctx[1]);
	test_tracer_destroy_order(ctx[0], ctx[1]);

	OTELC_LOG(stdout, "[multi-meter]");
	test_two_meters_coexist(ctx[0], ctx[1]);
	test_two_meters_distinct_instrument_maps(ctx[0], ctx[1]);

	OTELC_LOG(stdout, "[multi-logger]");
	test_two_loggers_coexist(ctx[0], ctx[1]);

	OTELC_LOG(stdout, "[multi-isolation]");
	test_tracers_span_isolation(ctx_multi[0], ctx_multi[1]);
	test_tracer_destroy_leftover_spans(ctx_multi[0]);
	test_loggers_record_isolation(ctx_multi[0], ctx_multi[1]);
	test_meters_value_isolation(ctx_multi[0], ctx_multi[1]);
	test_meters_instrument_id_spaces(ctx_multi[0], ctx_multi[1]);
	test_meter_destroy_order(ctx_multi[0], ctx_multi[1]);
#ifdef USE_THREADS
	test_meters_concurrent_updates(ctx_multi[0], ctx_multi[1]);
#endif

	otelc_deinit(&(ctx[0]), NULL, NULL, NULL);
	otelc_deinit(&(ctx[1]), NULL, NULL, NULL);
	otelc_deinit(&(ctx_multi[0]), NULL, NULL, NULL);
	otelc_deinit(&(ctx_multi[1]), NULL, NULL, NULL);

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
