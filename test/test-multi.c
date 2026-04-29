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
 *   that both ended up with non-null implementation state, distinct scope
 *   names, distinct yaml prefixes, and distinct implementation pointers (each
 *   impl owns its provider and propagator, so distinct impls imply distinct
 *   providers and propagators).  Starts each tracer, emits one span on each,
 *   and exercises a per-tracer text-map inject/extract round-trip to confirm
 *   that each tracer's propagator is wired correctly and works without a
 *   process-wide propagator being installed.
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
 *   that both ended up with non-null implementation state, distinct scope
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
 *   that both ended up with non-null implementation state, distinct scope
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
			if ((OTELC_OPS(primary,   log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "primary log")   >= 0)
			    && (OTELC_OPS(secondary, log, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, 0, "secondary log") >= 0))
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
 *   against them, releases the contexts, and reports the results.
 *
 * RETURN VALUE
 *   Returns EX_OK if all tests pass, or EX_SOFTWARE if any test fails.
 */
int main(int argc, char **argv)
{
	struct otelc_ctx *ctx[2] = { NULL, NULL };
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

	OTELC_LOG(stdout, "[multi-tracer]");
	test_two_tracers_coexist(ctx[0], ctx[1]);

	OTELC_LOG(stdout, "[multi-meter]");
	test_two_meters_coexist(ctx[0], ctx[1]);
	test_two_meters_distinct_instrument_maps(ctx[0], ctx[1]);

	OTELC_LOG(stdout, "[multi-logger]");
	test_two_loggers_coexist(ctx[0], ctx[1]);

	otelc_deinit(&(ctx[0]), NULL, NULL, NULL);
	otelc_deinit(&(ctx[1]), NULL, NULL, NULL);

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
