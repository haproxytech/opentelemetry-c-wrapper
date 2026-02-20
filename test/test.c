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
#include "include.h"


#define DEFAULT_CFG_FILE          "otel-cfg.yml"
#define DEFAULT_DEBUG_LEVEL       0b11101111111
#define DEFAULT_RUNCOUNT          10
#define DEFAULT_THREADS_COUNT     8
#define DEFAULT_SPAN_TIME_DELAY   1


typedef unsigned char bool_t;

enum FLAG_OPT_enum {
	FLAG_OPT_HELP    = UINT8_C(1) << 1,
	FLAG_OPT_USAGE   = UINT8_C(1) << 2,
	FLAG_OPT_VERSION = UINT8_C(1) << 3,
};

static struct {
	uint8_t              opt_flags;
	int                  runcount;
	int                  runtime_ms;
	int                  delay_ms;
	int                  random_seed;
#ifdef USE_THREADS
	int                  threads;
#endif
	const char          *cfg_file;
	struct otelc_tracer *otel_tracer;
	struct otelc_meter  *otel_meter;
	struct otelc_logger *otel_logger;
} cfg = {
	.runcount   = -1,
	.runtime_ms = -1,
	.delay_ms   = DEFAULT_SPAN_TIME_DELAY,
#ifdef USE_THREADS
	.threads    = DEFAULT_THREADS_COUNT,
#endif
	.cfg_file   = DEFAULT_CFG_FILE,
};

enum WORKER_SPAN_enum {
	WORKER_SPAN_ROOT = 0,
	WORKER_SPAN_CHILD,
	WORKER_SPAN_PROP_TM,
	WORKER_SPAN_PROP_HH,
	WORKER_SPAN_LINKED,
	WORKER_SPAN_MAX,
};

struct worker {
#ifdef USE_THREADS
	pthread_t          thread;
#endif
	int                id;
	struct otelc_span *otelc_span[WORKER_SPAN_MAX];
	int                otel_state;
	uint64_t           count;
	int                runtime_ms;
};

static struct {
	const char      *name;
	uint64_t         drop_cnt;
#ifdef USE_THREADS
	pthread_t        main_thread;
	struct worker    worker[8192];
	volatile bool_t  flag_run;
#else
	struct worker    worker;
#endif
} prg;

enum WORKER_STATE_enum {
	WORKER_STATE_LOOP_BEGIN,

	WORKER_STATE_SPAN_ROOT_START,
	WORKER_STATE_SPAN_ROOT_SET_BAGGAGE,
	WORKER_STATE_LOG_SPAN_ROOT,
	WORKER_STATE_SPAN_CHILD_START,
	WORKER_STATE_METER_INSTRUMENTS_CREATE,
	WORKER_STATE_METER_INSTRUMENTS_UPDATE,
	WORKER_STATE_METER_INSTRUMENTS_UPDATE_KV,
	WORKER_STATE_SPAN_CHILD_INJECT_TEXT_MAP,
	WORKER_STATE_SPAN_CHILD_SET_BAGGAGE,
	WORKER_STATE_SPAN_PROP_TM_START,
	WORKER_STATE_SPAN_CHILD_GET_BAGGAGE,
	WORKER_STATE_SPAN_CHILD_INJECT_HTTP_HEADERS,
	WORKER_STATE_SPAN_PROP_HH_START,
	WORKER_STATE_SPAN_PROP_TM_GET_BAGGAGE,
	WORKER_STATE_SPAN_PROP_HH_GET_BAGGAGE,
	WORKER_STATE_SPAN_ROOT_SET_ATTRIBUTE,
	WORKER_STATE_SPAN_PROP_TM_ADD_EVENT,
	WORKER_STATE_SPAN_LINKED_START,

	WORKER_STATE_LOOP_END,
};


#ifdef USE_THREADS

/* Counter for assigning unique IDs to threads not registered as workers. */
static atomic_int thread_id_offset = -1;


/***
 * NAME
 *   next_power_of_10 - computes the smallest power of 10 greater than value
 *
 * SYNOPSIS
 *   static int next_power_of_10(int value)
 *
 * ARGUMENTS
 *   value - the input number to exceed
 *
 * DESCRIPTION
 *   Returns the smallest power of 10 that is strictly greater than value.
 *   This is used to determine the starting offset for thread_id_offset so that
 *   unregistered thread IDs occupy a distinct numeric range from worker IDs.
 *
 * RETURN VALUE
 *   Returns the smallest power of 10 greater than value.
 */
static int next_power_of_10(int value)
{
	int retval = 1;

	while (retval <= value)
		retval *= 10;

	return retval;
}


/***
 * NAME
 *   decimal_width - computes the number of decimal digits needed for value
 *
 * SYNOPSIS
 *   static int decimal_width(int value)
 *
 * ARGUMENTS
 *   value - a positive integer
 *
 * DESCRIPTION
 *   Returns the number of decimal digits required to represent value.  This is
 *   used for field width formatting in log output.
 *
 * RETURN VALUE
 *   Returns the number of decimal digits in value.
 */
static int decimal_width(int value)
{
	int retval;

	for (retval = 1; value >= 10; retval++)
		value /= 10;

	return retval;
}

#endif /* USE_THREADS */


/***
 * NAME
 *   thread_id - retrieves the thread identifier for the current thread
 *
 * SYNOPSIS
 *   static int thread_id(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Returns a numeric identifier for the calling thread.  For the main
 *   program thread, it returns 0.  For worker threads registered in
 *   prg.worker[i].thread, it returns the 1-based index (i + 1).  For all other
 *   threads, it atomically assigns and caches a unique identifier from the
 *   thread_id_offset.
 *
 * RETURN VALUE
 *   Returns 0 for the main thread, (i + 1) for the i-th registered worker
 *   thread, or a value from thread_id_offset for any other thread.
 */
static int thread_id(void)
{
#ifdef USE_THREADS
	static __thread int tid = -1;
	pthread_t           id;
	int                 i;

	id = pthread_self();

	if (pthread_equal(prg.main_thread, id))
		return 0;

	for (i = 0; i < OTELC_MIN(cfg.threads, (int)OTELC_TABLESIZE(prg.worker)); i++)
		if (pthread_equal(prg.worker[i].thread, id))
			return i + 1;

	if ((tid == -1) && (atomic_load(&thread_id_offset) != -1))
		tid = atomic_fetch_add(&thread_id_offset, 1);

	return tid;
#else
	return 0;
#endif /* USE_THREADS */
}


/*
 * NAME
 *   log_handler_cb - counts SDK internal diagnostic messages
 *
 * SYNOPSIS
 *   static void log_handler_cb(otelc_log_level_t level, const char *file, int line, const char *msg, const struct otelc_kv *attr, size_t attr_len, void *ctx)
 *
 * ARGUMENTS
 *   level    - severity of the SDK diagnostic message
 *   file     - source file that emitted the message
 *   line     - source line number
 *   msg      - formatted diagnostic message text
 *   attr     - array of key-value attributes associated with the message
 *   attr_len - number of entries in the attr array
 *   ctx      - opaque context pointer (unused)
 *
 * DESCRIPTION
 *   Custom SDK internal log handler registered via otelc_log_set_handler().
 *   Each invocation atomically increments the prg.drop_cnt counter so the test
 *   can verify how many SDK diagnostic messages were emitted.  The message
 *   content is intentionally ignored.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void log_handler_cb(otelc_log_level_t level __maybe_unused, const char *file __maybe_unused, int line __maybe_unused, const char *msg __maybe_unused, const struct otelc_kv *attr __maybe_unused, size_t attr_len __maybe_unused, void *ctx __maybe_unused)
{
	OTELC_FUNC("%d, \"%s\", %d, \"%s\", %p, %zu, %p", level, OTELC_STR_ARG(file), line, OTELC_STR_ARG(msg), attr, attr_len, ctx);

	__atomic_add_fetch(&prg.drop_cnt, 1, __ATOMIC_RELAXED);

	OTELC_RETURN();
}


/***
 * NAME
 *   worker_end_all_spans - ends all active spans for a worker
 *
 * SYNOPSIS
 *   static void worker_end_all_spans(struct worker *worker)
 *
 * ARGUMENTS
 *   worker - pointer to the worker structure whose spans should be ended
 *
 * DESCRIPTION
 *   Iterates through all spans associated with the given worker and ends them
 *   using a monotonic timestamp.  This ensures that all spans are properly
 *   closed before the worker completes its current iteration or terminates.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void worker_end_all_spans(struct worker *worker)
{
	struct timespec ts_steady;
	int             i;

	OTELC_FUNC("%p", worker);

	(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);

	for (i = OTELC_TABLESIZE(worker->otelc_span) - 1; i >= 0; i--)
		if (_nNULL(worker->otelc_span[i]))
			worker->otelc_span[i]->end_with_options(worker->otelc_span + i, &ts_steady, random() % (OTELC_SPAN_STATUS_ERROR + 1), NULL);

	OTELC_RETURN();
}


/***
 * NAME
 *   update_instrument_int64_cb - callback to update an int64 metric instrument
 *
 * SYNOPSIS
 *   static void update_instrument_int64_cb(struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   data - observable callback descriptor
 *
 * DESCRIPTION
 *   This function is an asynchronous callback that increments the value of
 *   an int64 metric instrument by a random amount.  It is used to simulate
 *   metric updates during the test run.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void update_instrument_int64_cb(struct otelc_metric_observable_cb *data)
{
	OTELC_FUNC("%p", data);

	OTELC_DBG_METRIC_OBSERVABLE_CB(WORKER, "callback data", data);

	if (_NULL(data) || _NULL(data->value))
		OTELC_RETURN();
	else if (data->value->u_type != OTELC_VALUE_INT64)
		OTELC_RETURN();

	data->value->u.value_int64 += random() % 1000;

	OTELC_DBG(DEBUG, "value_int64 = %" PRId64, data->value->u.value_int64);

	OTELC_RETURN();
}


/***
 * NAME
 *   update_instrument_double_cb - callback to update a double metric instrument
 *
 * SYNOPSIS
 *   static void update_instrument_double_cb(struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   data - observable callback descriptor
 *
 * DESCRIPTION
 *   This function is an asynchronous callback that increments the value of
 *   a double metric instrument by a random floating-point amount.  It helps
 *   verify that asynchronous double metrics are correctly handled by the
 *   library.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void update_instrument_double_cb(struct otelc_metric_observable_cb *data)
{
	OTELC_FUNC("%p", data);

	OTELC_DBG_METRIC_OBSERVABLE_CB(WORKER, "callback data", data);

	if (_NULL(data) || _NULL(data->value))
		OTELC_RETURN();
	else if (data->value->u_type != OTELC_VALUE_DOUBLE)
		OTELC_RETURN();

	data->value->u.value_double += (random() % 1000) / 10.0;

	OTELC_DBG(DEBUG, "value_double = %f", data->value->u.value_double);

	OTELC_RETURN();
}


/***
 * NAME
 *   worker_thread - main execution loop for a worker
 *
 * SYNOPSIS
 *   static void *worker_thread(void *data)
 *
 * ARGUMENTS
 *   data - pointer to the worker structure
 *
 * DESCRIPTION
 *   This function implements the state machine for a worker thread.  It
 *   performs various OpenTelemetry operations, such as starting spans,
 *   injecting/extracting trace context, updating metrics, and logging.
 *   The worker continues to run until it reaches the specified run count
 *   or runtime limit.
 *
 * RETURN VALUE
 *   This function does not return a value.  In threaded mode it
 *   terminates via pthread_exit().
 */
#ifdef USE_THREADS
__attribute__((noreturn)) static void *worker_thread(void *data)
#else
static void worker_thread(void *data)
#endif
{
#ifdef USE_THREADS
	char                               name[16];
#endif
#ifndef OTELC_USE_THREAD_SHARED_HANDLE
	char                               otel_infbuf[BUFSIZ];
#endif
	OTELC_DBG_IFDEF(int n = 0, );
	struct otelc_text_map_writer       tm_wr;
	struct otelc_http_headers_writer   hh_wr;
	struct otelc_span_context         *context;
	struct timespec                    ts_steady, ts_system;
	struct timeval                     now;
	struct worker                     *worker = data;
	int64_t                            meter_instrument[] = { 0, 0, 0, 0, 0 };
	/***
	 * This is used only to reduce cobwebs in the code.
	 */
	struct otelc_tracer              **tracer       = &(cfg.otel_tracer);
	struct otelc_span                **span_root    = worker->otelc_span + WORKER_SPAN_ROOT;
	struct otelc_span                **span_child   = worker->otelc_span + WORKER_SPAN_CHILD;
	struct otelc_span                **span_prop_tm = worker->otelc_span + WORKER_SPAN_PROP_TM;
	struct otelc_span                **span_prop_hh = worker->otelc_span + WORKER_SPAN_PROP_HH;
	struct otelc_span                **span_linked  = worker->otelc_span + WORKER_SPAN_LINKED;
	struct otelc_meter               **meter        = &(cfg.otel_meter);
	struct otelc_logger              **logger       = &(cfg.otel_logger);

	OTELC_FUNC("%p", data);

#ifdef __linux__
	OTELC_DBG(WORKER, "Worker started, thread id: %" PRI_PTHREADT, syscall(SYS_gettid));
#else
	OTELC_DBG(WORKER, "Worker started, thread id: %" PRI_PTHREADT, worker->thread);
#endif

	(void)memset(&tm_wr, 0, sizeof(tm_wr));
	(void)memset(&hh_wr, 0, sizeof(hh_wr));

	(void)gettimeofday(&now, NULL);
	(void)srandom(cfg.random_seed ? (cfg.random_seed + worker->id) : now.tv_usec);

#ifdef USE_THREADS
	(void)snprintf(name, sizeof(name), "test/wrk: %d", worker->id);
	(void)pthread_setname_np(worker->thread, name);

	while (!prg.flag_run) {
		otelc_nsleep(0, UINT32_C(10000000));

		OTELC_DBG_IFDEF(n++, );
	}
#endif /* USE_THREADS */

	OTELC_DBG(DEBUG, "waiting loop count: %d", n);

	for ( ; 1; worker->otel_state++) {
		OTELC_DBG(WORKER, "state: %" PRIu64 " / %d", worker->count, worker->otel_state);

		(void)clock_gettime(CLOCK_REALTIME, &ts_system);
		(void)clock_gettime(CLOCK_MONOTONIC, &ts_steady);

		if (worker->otel_state == WORKER_STATE_LOOP_BEGIN) {
			worker->runtime_ms = OTELC_RUNTIME_MS();

			if ((cfg.runtime_ms > 0) && (worker->runtime_ms >= cfg.runtime_ms))
				break;
			else if ((cfg.runcount > 0) && (worker->count >= (uint)cfg.runcount))
				break;
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_ROOT_START) {
			if (_NULL(*tracer))
				continue;

			if (_NULL(*span_root = (*tracer)->start_span_with_options(*tracer, "root span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0)))
				break;
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_ROOT_SET_BAGGAGE) {
			if (_NULL(*span_root))
				continue;

			(void)(*span_root)->set_baggage_var(*span_root, "root_baggage_1", "value_1", "root_baggage_2", "value_2", NULL);
		}
		else if (worker->otel_state == WORKER_STATE_LOG_SPAN_ROOT) {
			static const struct otelc_kv attr[] = {
				{ .key = (char *)"log_attr_1_key_null",   .value = { .u_type = OTELC_VALUE_NULL,   .u.value_string = NULL                 } },
				{ .key = (char *)"log_attr_1_key_bool",   .value = { .u_type = OTELC_VALUE_BOOL,   .u.value_bool   = true                 } },
				{ .key = (char *)"log_attr_1_key_int32",  .value = { .u_type = OTELC_VALUE_INT32,  .u.value_int32  = INT32_C(1234)        } },
				{ .key = (char *)"log_attr_1_key_int64",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(12340000)    } },
				{ .key = (char *)"log_attr_1_key_uint32", .value = { .u_type = OTELC_VALUE_UINT32, .u.value_uint32 = UINT32_C(5678)       } },
				{ .key = (char *)"log_attr_1_key_uint64", .value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(56780000)   } },
				{ .key = (char *)"log_attr_1_key_double", .value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 3.14                 } },
				{ .key = (char *)"log_attr_1_key_string", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "string value"       } },
				{ .key = (char *)"log_attr_1_key_data",   .value = { .u_type = OTELC_VALUE_DATA,   .u.value_data   = (void *)"data value" } },
			};
			struct timespec ts = { INT32_C(1111111111), INT32_C(222222222) };
			uint8_t         span_id[OTELC_SPAN_ID_SIZE] = { 0, 1, 2, 3, 4, 5, 6, 7 };
			uint8_t         trace_id[OTELC_TRACE_ID_SIZE] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
			uint8_t         trace_flags = random() % 256;

			if (_NULL(*span_root) || _NULL(*logger))
				continue;

			(void)(*span_root)->get_id(*span_root, span_id, sizeof(span_id), trace_id, sizeof(trace_id), &trace_flags);
			(*logger)->log(*logger, OTELC_LOG_SEVERITY_DEBUG, 0, NULL, span_id, sizeof(span_id), trace_id, sizeof(trace_id), trace_flags, &ts, attr, OTELC_TABLESIZE(attr), "debug log from worker %d (span_root)", worker->id);
			(*logger)->log_span(*logger, OTELC_LOG_SEVERITY_INFO, 0, NULL, *span_root, &ts, attr, OTELC_TABLESIZE(attr), "info log from worker %d (span_root)", worker->id);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_CHILD_START) {
			if (_NULL(*tracer))
				continue;

			if (_NULL(*span_child = (*tracer)->start_span_with_options(*tracer, "child span", *span_root, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0)))
				break;
		}
		else if (worker->otel_state == WORKER_STATE_METER_INSTRUMENTS_CREATE) {
			static const double bounds[] = { 1.0, 10.0, 100.0, 1000.0 };
			static struct otelc_metric_observable_cb cb_data[] = {
				{ .func = update_instrument_int64_cb  },
				{ .func = update_instrument_double_cb },
			};

			if (_NULL(*meter))
				continue;

			(*meter)->add_view(*meter, "histogram_double", "", "histogram_double", "", OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE, OTELC_METRIC_AGGREGATION_HISTOGRAM, bounds, OTELC_TABLESIZE(bounds));

			meter_instrument[0] = (*meter)->create_instrument(*meter, "counter_uint64", "synchronous counter of type uint64_t", "", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
			meter_instrument[1] = (*meter)->create_instrument(*meter, "histogram_double", "histogram of double values", "", OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE, NULL);
			meter_instrument[2] = (*meter)->create_instrument(*meter, "observable_counter_int64", "asynchronous counter of type int64_t", "", OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64, cb_data + 0);
			meter_instrument[3] = (*meter)->create_instrument(*meter, "observable_updowncounter_double", "asynchronous up-down counter of type double", "", OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_DOUBLE, cb_data + 1);
			meter_instrument[4] = (*meter)->create_instrument(*meter, "observable_gauge_int64", "asynchronous gauge of type int64_t", "", OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_INT64, cb_data + 0);
		}
		else if (worker->otel_state == WORKER_STATE_METER_INSTRUMENTS_UPDATE) {
			struct otelc_value value[] = {
				{ .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = random() % 100             },
				{ .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = random() % 10000           },
				{ .u_type = OTELC_VALUE_DOUBLE, .u.value_double = (random() % 1000)   / 10.0 },
				{ .u_type = OTELC_VALUE_DOUBLE, .u.value_double = (random() % 100000) / 10.0 },
			};

			if (_NULL(*meter))
				continue;

			(*meter)->update_instrument(*meter, meter_instrument[0], value + 0);
			(*meter)->update_instrument(*meter, meter_instrument[0], value + 1);

			(*meter)->update_instrument(*meter, meter_instrument[1], value + 2);
			(*meter)->update_instrument(*meter, meter_instrument[1], value + 3);
		}
		else if (worker->otel_state == WORKER_STATE_METER_INSTRUMENTS_UPDATE_KV) {
			static const struct otelc_kv attr_1[] = {
				{ .key = (char *)"metric_attr_1_string", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "string value" } },
				{ .key = (char *)"metric_attr_1_int64",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(42)    } },
			};
			static const struct otelc_kv attr_2[] = {
				{ .key = (char *)"metric_attr_2_int32", .value = { .u_type = OTELC_VALUE_INT32, .u.value_int32 = INT32_C(24) } },
				{ .key = (char *)"metric_attr_2_bool",  .value = { .u_type = OTELC_VALUE_BOOL,  .u.value_bool   = true       } },
			};
			struct otelc_value value[] = {
				{ .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = random() % 10000           },
				{ .u_type = OTELC_VALUE_DOUBLE, .u.value_double = (random() % 100000) / 10.0 },
			};

			if (_NULL(*meter))
				continue;

			(*meter)->update_instrument_kv_n(*meter, meter_instrument[0], value + 0, attr_1, OTELC_TABLESIZE(attr_1));
			(*meter)->update_instrument_kv_n(*meter, meter_instrument[1], value + 1, attr_2, OTELC_TABLESIZE(attr_2));
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_CHILD_INJECT_TEXT_MAP) {
			if (_NULL(*span_child))
				continue;

			otelc_span_inject_text_map(*span_child, &tm_wr);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_CHILD_SET_BAGGAGE) {
			if (_NULL(*span_child))
				continue;

			(void)(*span_child)->set_baggage_var(*span_child, "child_baggage_1", "value_3", "child_baggage_2", "value_4", NULL);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_PROP_TM_START) {
			struct otelc_text_map_reader  tm_rd;
			struct otelc_text_map        *text_map = &(tm_wr.text_map);

			if (_NULL(*tracer))
				continue;

			if (_nNULL(context = otelc_tracer_extract_text_map(*tracer, &tm_rd, text_map))) {
				if (_NULL(*span_prop_tm = (*tracer)->start_span_with_options(*tracer, "text map propagation", NULL, context, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0)))
					break;

				OTELC_OPSR(context, destroy);
			}
			otelc_text_map_destroy(&text_map);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_CHILD_GET_BAGGAGE) {
			if (_NULL(*span_child))
				continue;

			struct otelc_text_map *baggage = (*span_child)->get_baggage_var(*span_child, "root_baggage_1", "root_baggage_2", "child_baggage_1", "child_baggage_2", NULL);

			otelc_text_map_destroy(&baggage);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_CHILD_INJECT_HTTP_HEADERS) {
			if (_NULL(*span_child))
				continue;

			otelc_span_inject_http_headers(*span_child, &hh_wr);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_PROP_HH_START) {
			struct otelc_http_headers_reader  hh_rd;
			struct otelc_text_map            *text_map = &(hh_wr.text_map);

			if (_NULL(*tracer))
				continue;

			if (_nNULL(context = otelc_tracer_extract_http_headers(*tracer, &hh_rd, text_map))) {
				if (_NULL(*span_prop_hh = (*tracer)->start_span_with_options(*tracer, "http headers propagation", NULL, context, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0)))
					break;

				OTELC_OPSR(context, destroy);
			}
			otelc_text_map_destroy(&text_map);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_PROP_TM_GET_BAGGAGE) {
			if (_NULL(*span_prop_tm))
				continue;

			struct otelc_text_map *baggage = (*span_prop_tm)->get_baggage_var(*span_prop_tm, "root_baggage_1", "root_baggage_2", "child_baggage_1", "child_baggage_2", NULL);
			char                  *value = (*span_prop_tm)->get_baggage(*span_prop_tm, "root_baggage_1");
			if (_nNULL(value))
				OTELC_FREE(__func__, __LINE__, value);

			otelc_text_map_destroy(&baggage);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_PROP_HH_GET_BAGGAGE) {
			if (_NULL(*span_prop_hh))
				continue;

			struct otelc_text_map *baggage = (*span_prop_hh)->get_baggage_var(*span_prop_hh, "root_baggage_1", "root_baggage_2", "child_baggage_1", "child_baggage_2", NULL);
			char                  *value = (*span_prop_hh)->get_baggage(*span_prop_hh, "child_baggage_1");
			if (_nNULL(value))
				OTELC_FREE(__func__, __LINE__, value);

			otelc_text_map_destroy(&baggage);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_ROOT_SET_ATTRIBUTE) {
			static const struct otelc_kv attr[] = {
				{ .key = (char *)"attr_1_key_null",   .value = { .u_type = OTELC_VALUE_NULL,   .u.value_string = NULL                 } },
				{ .key = (char *)"attr_1_key_bool",   .value = { .u_type = OTELC_VALUE_BOOL,   .u.value_bool   = true                 } },
				{ .key = (char *)"attr_1_key_int32",  .value = { .u_type = OTELC_VALUE_INT32,  .u.value_int32  = INT32_C(1234)        } },
				{ .key = (char *)"attr_1_key_int64",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(12340000)    } },
				{ .key = (char *)"attr_1_key_uint32", .value = { .u_type = OTELC_VALUE_UINT32, .u.value_uint32 = UINT32_C(5678)       } },
				{ .key = (char *)"attr_1_key_uint64", .value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(56780000)   } },
				{ .key = (char *)"attr_1_key_double", .value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 3.14                 } },
				{ .key = (char *)"attr_1_key_string", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "string value"       } },
				{ .key = (char *)"attr_1_key_data",   .value = { .u_type = OTELC_VALUE_DATA,   .u.value_data   = (void *)"data value" } },
			};

			if (_NULL(*span_root))
				continue;

			(void)(*span_root)->set_attribute_var(*span_root, attr[0].key, &(attr[0].value), attr[1].key, &(attr[1].value), attr[2].key, &(attr[2].value), NULL);
			(void)(*span_root)->set_attribute_kv_var(*span_root, attr + 3, attr + 4, NULL);
			(void)(*span_root)->set_attribute_kv_n(*span_root, attr + 5, 4);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_PROP_TM_ADD_EVENT) {
			static const struct otelc_kv event[] = {
				{ .key = (char *)"event_1_key_null",   .value = { .u_type = OTELC_VALUE_NULL,   .u.value_string = NULL                 } },
				{ .key = (char *)"event_1_key_bool",   .value = { .u_type = OTELC_VALUE_BOOL,   .u.value_bool   = true                 } },
				{ .key = (char *)"event_1_key_int32",  .value = { .u_type = OTELC_VALUE_INT32,  .u.value_int32  = INT32_C(1234)        } },
				{ .key = (char *)"event_1_key_int64",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(12340000)    } },
				{ .key = (char *)"event_1_key_uint32", .value = { .u_type = OTELC_VALUE_UINT32, .u.value_uint32 = UINT32_C(5678)       } },
				{ .key = (char *)"event_1_key_uint64", .value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(56780000)   } },
				{ .key = (char *)"event_1_key_double", .value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 3.14                 } },
				{ .key = (char *)"event_1_key_string", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "string value"       } },
				{ .key = (char *)"event_1_key_data",   .value = { .u_type = OTELC_VALUE_DATA,   .u.value_data   = (void *)"data value" } },
			};

			if (_NULL(*span_prop_tm))
				continue;

			(void)(*span_prop_tm)->add_event_var(*span_prop_tm, "event_1", &ts_system, event[0].key, &(event[0].value), event[1].key, &(event[1].value), event[2].key, &(event[2].value), NULL);
			(void)(*span_prop_tm)->add_event_kv_var(*span_prop_tm, "event_1", &ts_system, event + 3, event + 4, NULL);
			(void)(*span_prop_tm)->add_event_kv_n(*span_prop_tm, "event_1", &ts_system, event + 5, 4);
		}
		else if (worker->otel_state == WORKER_STATE_SPAN_LINKED_START) {
			static const struct otelc_kv attr[] = {
				{ .key = (char *)"link_1_key_null",   .value = { .u_type = OTELC_VALUE_NULL,   .u.value_string = NULL                 } },
				{ .key = (char *)"link_1_key_bool",   .value = { .u_type = OTELC_VALUE_BOOL,   .u.value_bool   = true                 } },
				{ .key = (char *)"link_1_key_int32",  .value = { .u_type = OTELC_VALUE_INT32,  .u.value_int32  = INT32_C(1234)        } },
				{ .key = (char *)"link_1_key_int64",  .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(12340000)    } },
				{ .key = (char *)"link_1_key_uint32", .value = { .u_type = OTELC_VALUE_UINT32, .u.value_uint32 = UINT32_C(5678)       } },
				{ .key = (char *)"link_1_key_uint64", .value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(56780000)   } },
				{ .key = (char *)"link_1_key_double", .value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 3.14                 } },
				{ .key = (char *)"link_1_key_string", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "string value"       } },
				{ .key = (char *)"link_1_key_data",   .value = { .u_type = OTELC_VALUE_DATA,   .u.value_data   = (void *)"data value" } },
			};

			if (_NULL(*tracer))
				continue;

			if (_NULL(*span_linked = (*tracer)->start_span_with_options(*tracer, "linked span", NULL, NULL, &ts_steady, &ts_system, OTELC_SPAN_KIND_SERVER, NULL, 0)))
				break;
			else if ((*span_linked)->add_link(*span_linked, *span_root, NULL, attr, OTELC_TABLESIZE(attr)) == OTELC_RET_ERROR)
				break;
		}
		else if (worker->otel_state >= WORKER_STATE_LOOP_END) {
			worker_end_all_spans(worker);

			worker->otel_state = WORKER_STATE_LOOP_BEGIN - 1;
			worker->count++;
		}

		if (cfg.delay_ms) {
			const uint64_t delay = ((random() % 10) + 1) * cfg.delay_ms * UINT64_C(100000);

			otelc_nsleep(delay / UINT64_C(1000000000), delay % UINT64_C(1000000000));
		}
	}

#ifndef OTELC_USE_THREAD_SHARED_HANDLE
	int otelc_status  = otelc_statistics_check(0, 0, worker->count * 5, 0, worker->count * 5, worker->count * 5);
	otelc_status     |= otelc_statistics_check(1, 0, worker->count * 2, 0, worker->count * 2, worker->count * 2);
	otelc_status     |= otelc_statistics_check(2, 5, 5, 0, 0, 0);
	otelc_status     |= otelc_statistics_check(3, 1, 1, 0, 0, 0);

	otelc_statistics(otel_infbuf, sizeof(otel_infbuf));
	OTELC_LOG(stdout, "[%4d] %s traces: %" PRIu64 ", %s", worker->id, otelc_status ? "ERROR" : "OK", worker->count, otel_infbuf);
#endif

#ifdef __linux__
	OTELC_DBG(WORKER, "Worker stopped, thread id: %" PRI_PTHREADT, syscall(SYS_gettid));
#else
	OTELC_DBG(WORKER, "Worker stopped, thread id: %" PRI_PTHREADT, worker->thread);
#endif

	OTELC_FUNC_END("}");

#ifdef USE_THREADS
	pthread_exit(NULL);
#endif
}


/***
 * NAME
 *   worker_run - initializes and starts all worker threads
 *
 * SYNOPSIS
 *   static int worker_run(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Sets up the worker structures, creates the specified number of worker
 *   threads, and waits for them to complete.  It also tracks the total number
 *   of operations performed across all workers and prints final statistics.
 *
 * RETURN VALUE
 *   Returns EX_OK on success, or an appropriate exit code on failure.
 */
static int worker_run(void)
{
#ifdef USE_THREADS
	uint64_t total_cnt = 0;
	double   total_cps = 0.0;
	int      i, num_threads = 0;
#endif
#ifdef OTELC_USE_THREAD_SHARED_HANDLE
	char     otel_infbuf[BUFSIZ];
#endif
	int      retval = EX_OK;

	OTELC_FUNC("");

#ifdef USE_THREADS
	(void)pthread_setname_np(prg.main_thread, "test/wrk: main");

	for (i = 0; i < cfg.threads; i++) {
		prg.worker[i].id = i + 1;

		if (pthread_create(&(prg.worker[i].thread), NULL, worker_thread, prg.worker + i) != 0) {
			OTELC_LOG(stderr, "ERROR: Unable to start thread for worker %d: %m", prg.worker[i].id);
			prg.worker[i].id = 0;
		} else
			num_threads++;
	}

	prg.flag_run = 1;

	OTELC_DBG(WORKER, "%d threads started in %" PRId64 " ms", num_threads, OTELC_RUNTIME_MS());

	for (i = 0; i < cfg.threads; i++) {
		if (prg.worker[i].id == 0)
			continue;

		if (pthread_join(prg.worker[i].thread, NULL) != 0)
			OTELC_LOG(stderr, "ERROR: Unable to join worker thread %d: %m", prg.worker[i].id);
		else
			OTELC_LOG(stdout, "worker %*d count: %" PRIu64, decimal_width(cfg.threads), prg.worker[i].id, prg.worker[i].count);

		total_cnt += prg.worker[i].count;
		total_cps += prg.worker[i].count / (prg.worker[i].runtime_ms / 1000.0);
	}

	OTELC_LOG(stdout, "%d worker(s) total count: %" PRIu64", %.2f/sec", cfg.threads, total_cnt, total_cps);
#else
	(void)worker_thread(&(prg.worker));
#endif /* USE_THREADS */

#ifdef OTELC_USE_THREAD_SHARED_HANDLE
	int otelc_status  = otelc_statistics_check(0, 0, total_cnt * 5, 0, total_cnt * 5, total_cnt * 5);
	otelc_status     |= otelc_statistics_check(1, 0, total_cnt * 2, 0, total_cnt * 2, total_cnt * 2);
	otelc_status     |= otelc_statistics_check(2, 5, 5, 0, 0, 0);
	otelc_status     |= otelc_statistics_check(3, 1, 1, 0, 0, 0);

	otelc_statistics(otel_infbuf, sizeof(otel_infbuf));
	OTELC_LOG(stdout, "OpenTelemetry statistics: %s %s (%" PRIu64 ")", otelc_status ? "ERROR" : "OK", otel_infbuf, prg.drop_cnt);
#endif

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   usage - prints the program usage information
 *
 * SYNOPSIS
 *   static void usage(const char *program_name, bool_t flag_verbose)
 *
 * ARGUMENTS
 *   program_name - name of the program executable
 *   flag_verbose - flag indicating whether to print detailed option descriptions
 *
 * DESCRIPTION
 *   Outputs the command-line usage summary to the standard output.  If the
 *   verbose flag is set, it also provides detailed descriptions of all available
 *   options and their default values.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void usage(const char *program_name, bool_t flag_verbose)
{
	(void)printf("\nUsage: %s { -h --help }\n", program_name);
	(void)printf("       %s { -V --version }\n", program_name);
	(void)printf("       %s { [ -R --runcount=VALUE ] | [ -r --runtime=TIME ] } [OPTION]...\n\n", program_name);

#define _ OTELC_DBG_IFDEF("  ", "")
	if (flag_verbose) {
		(void)printf("Options are:\n");
		(void)printf("  -c, --config=FILE        " _ "OpenTelemetry configuration file (default: %s).\n", DEFAULT_CFG_FILE);
		(void)printf("  -D, --delay=TIME         " _ "Delay between OpenTelemetry operations (default: %d ms).\n", DEFAULT_SPAN_TIME_DELAY);
#ifdef DEBUG
		(void)printf("  -d, --debug=LEVEL        " _ "Debug level (default: 0x%04x).", DEFAULT_DEBUG_LEVEL);
#endif
		(void)printf("  -h, --help               " _ "Show this text.\n");
		(void)printf("  -R, --runcount=VALUE     " _ "Number of passes to run (default: %d, 0 = unlimited).\n", DEFAULT_RUNCOUNT);
		(void)printf("  -r, --runtime=TIME       " _ "Run time in ms (0 = unlimited).\n");
		(void)printf("  -s, --random-seed=VALUE  " _ "Set the random seed for reproducible results.\n");
#ifdef DEBUG
		(void)printf("  -T, --trigger-debug-throw  Enable a debug-only throw for testing purposes.\n");
#endif
#ifdef USE_THREADS
		(void)printf("  -t, --threads=VALUE      " _ "Number of threads (default: %d).\n", DEFAULT_THREADS_COUNT);
#else
		(void)printf("  -t, --threads=VALUE      " _ "Ignored (threads are not enabled).\n");
#endif
		(void)printf("  -V, --version            " _ "Show program version.\n\n");
		(void)printf("Copyright 2026 HAProxy Technologies\n");
		(void)printf("SPDX-License-Identifier: Apache-2.0\n\n");
	} else {
		(void)printf("For help type: %s -h\n\n", program_name);
	}
#undef _
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
 *   Initializes the OpenTelemetry library and starts the worker simulation
 *   based on the provided command-line options.  It handles configuration
 *   file loading, tracer/meter/logger creation, and clean shutdown.
 *
 * RETURN VALUE
 *   Returns EX_OK on success, or an appropriate error code on failure.
 */
int main(int argc, char **argv)
{
	static const struct option longopts[] = {
		{ "config",              required_argument, NULL, 'c' },
		{ "delay",               required_argument, NULL, 'D' },
#ifdef DEBUG
		{ "debug",               required_argument, NULL, 'd' },
#endif
		{ "help",                no_argument,       NULL, 'h' },
		{ "runcount",            required_argument, NULL, 'R' },
		{ "runtime",             required_argument, NULL, 'r' },
		{ "trigger-debug-throw", no_argument,       NULL, 'T' },
		{ "threads",             required_argument, NULL, 't' },
		{ "version",             no_argument,       NULL, 'V' },
		{ NULL,                  0,                 NULL, 0   }
	};
#ifdef OTELC_DBG_MEM
	static struct otelc_dbg_mem_data  dbg_mem_data[1000000];
	struct otelc_dbg_mem              dbg_mem;
#endif
	const char                       *shortopts = "c:D:d:hR:r:s:Tt:V";
	char                             *otel_err = NULL;
	int                               rc, c, longopts_idx = -1, retval = EX_OK;
	bool_t                            flag_error = 0;

	OTELC_DBG_IFDEF(otelc_dbg_level = DEFAULT_DEBUG_LEVEL, );

	OTELC_FUNC("%d, %p:%p", argc, argv, _NULL(argv) ? NULL : *argv);

	if (!OTELC_IS_VALID_VERSION()) {
		OTELC_LOG(stderr, "ERROR: OpenTelemetry C Wrapper version mismatch: library (%s) does not match header files (%s).  Please ensure both are the same version.", otelc_version(), OTELC_VERSION);

		OTELC_RETURN_INT(EX_SOFTWARE);
	}

	prg.name        = basename(argv[0]);
#ifdef USE_THREADS
	prg.main_thread = pthread_self();
#endif

	(void)otelc_runtime();
	otelc_ext_init(NULL, NULL, thread_id);

	(void)setvbuf(stdout, NULL, _IOLBF, 0);
	(void)setvbuf(stderr, NULL, _IOLBF, 0);

#ifdef OTELC_DBG_MEM
	retval = otelc_dbg_mem_init(&dbg_mem, dbg_mem_data, OTELC_TABLESIZE(dbg_mem_data));
	if (retval == OTELC_RET_ERROR) {
		OTELC_LOG(stderr, "ERROR: Unable to initialize memory debugger");

		OTELC_RETURN_INT(retval);
	}
#endif

	while ((c = getopt_long(argc, argv, shortopts, longopts, &longopts_idx)) != EOF) {
		if (c == 'c')
			cfg.cfg_file = optarg;
		else if (c == 'D')
			cfg.delay_ms = atoi(optarg);
#ifdef DEBUG
		else if (c == 'd')
			flag_error = !otelc_strtoi(optarg, NULL, 1, 0, &otelc_dbg_level, 0, 0x0fff, &otel_err);
		else if (c == 'T')
			otelc_dbg_trigger_throw = true;
#endif
		else if (c == 'h')
			cfg.opt_flags |= FLAG_OPT_HELP;
		else if (c == 'R')
			cfg.runcount = atoi(optarg);
		else if (c == 'r')
			cfg.runtime_ms = atoi(optarg);
		else if (c == 's')
			cfg.random_seed = atoi(optarg);
#ifdef USE_THREADS
		else if (c == 't')
			cfg.threads = atoi(optarg);
#endif
		else if (c == 'V')
			cfg.opt_flags |= FLAG_OPT_VERSION;
		else
			cfg.opt_flags |= FLAG_OPT_USAGE;
	}

	if (cfg.opt_flags & (FLAG_OPT_HELP | FLAG_OPT_USAGE)) {
		usage(prg.name, (cfg.opt_flags & FLAG_OPT_HELP) ? 1 : 0);

		OTELC_RETURN_INT((cfg.opt_flags & FLAG_OPT_HELP) ? EX_OK : EX_USAGE);
	}
	else if (cfg.opt_flags & FLAG_OPT_VERSION) {
		(void)printf("\n%s v%s [build %d] by %s, %s\n\n", prg.name, OTELC_PACKAGE_VERSION, OTELC_PACKAGE_BUILD, OTELC_PACKAGE_AUTHOR, __DATE__);
	}
	else {
		if (flag_error)
			OTELC_LOG(stderr, "ERROR: %s", otel_err);

		/***
		 * All this YAML path fiddling is just because the program ends
		 * up in test/.libs while the configuration stays in test.
		 */
		if (strcmp(cfg.cfg_file, DEFAULT_CFG_FILE) == 0) {
			static char cfg_file[PATH_MAX];

			rc = snprintf(cfg_file, sizeof(cfg_file), "%s/%s", dirname(argv[0]), cfg.cfg_file);
			if (OTELC_IN_RANGE(rc, 0, (int)(sizeof(cfg_file) - 1))) {
				if (access(cfg_file, F_OK) == -1) {
					char *path = OTELC_STRNDUP(__func__, __LINE__, cfg_file, rc);

					if (_nNULL(path)) {
						rc = snprintf(cfg_file, sizeof(cfg_file), "%s/../%s", dirname(path), cfg.cfg_file);

						OTELC_SFREE(path);
					} else {
						rc = -1;
					}
				}

				if (OTELC_IN_RANGE(rc, 0, (int)(sizeof(cfg_file) - 1))) {
					cfg.cfg_file = cfg_file;

					OTELC_DBG(DEBUG, "cfg_file = \"%s\"", cfg.cfg_file);
				} else {
					OTELC_LOG(stderr, "ERROR: Unable to set path for configuration file");
					flag_error = 1;
				}
			} else {
				OTELC_LOG(stderr, "ERROR: Unable to set path for configuration file");
				flag_error = 1;
			}
		}

		if (!flag_error && (access(cfg.cfg_file, F_OK) == -1)) {
			OTELC_LOG(stderr, "ERROR: Unable to find the configuration file");
			flag_error = 1;
		}

		if ((cfg.runcount >= 0) && (cfg.runtime_ms >= 0)) {
			OTELC_LOG(stderr, "ERROR: Parameters '--runcount' and '--runtime' are mutually exclusive.");
			flag_error = 1;
		}
		else if (cfg.runtime_ms >= 0)
			/* Do nothing. */;
		else if (cfg.runcount < 0)
			cfg.runcount = DEFAULT_RUNCOUNT;

#ifdef USE_THREADS
		if (!OTELC_IN_RANGE(cfg.threads, 1, (int)OTELC_TABLESIZE(prg.worker))) {
			OTELC_LOG(stderr, "ERROR: Invalid number of threads '%d'", cfg.threads);
			flag_error = 1;
		} else {
			OTELC_DBG_IFDEF(otelc_dbg_tid_width = decimal_width(cfg.threads), );
			atomic_init(&thread_id_offset, next_power_of_10(cfg.threads));
		}
#endif

		if (flag_error)
			usage(prg.name, 0);
	}

	if (flag_error || (cfg.opt_flags & (FLAG_OPT_HELP | FLAG_OPT_VERSION)))
		OTELC_RETURN_INT(flag_error ? EX_USAGE : EX_OK);

	if (otelc_init(cfg.cfg_file, &otel_err) == OTELC_RET_ERROR) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init library" : otel_err);

		retval = EX_SOFTWARE;
	}
	else if (_NULL(cfg.otel_tracer = otelc_tracer_create(&otel_err))) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init traces" : otel_err);

		retval = EX_SOFTWARE;
	}
	else if (cfg.otel_tracer->start(cfg.otel_tracer) == OTELC_RET_ERROR) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(cfg.otel_tracer->err) ? "Unable to start traces" : cfg.otel_tracer->err);

		retval = EX_SOFTWARE;
	}
	else if (_NULL(cfg.otel_meter = otelc_meter_create(&otel_err))) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init metrics" : otel_err);

		retval = EX_SOFTWARE;
	}
	else if (cfg.otel_meter->start(cfg.otel_meter) == OTELC_RET_ERROR) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(cfg.otel_meter->err) ? "Unable to start metrics" : cfg.otel_meter->err);

		retval = EX_SOFTWARE;
	}
	else if (_NULL(cfg.otel_logger = otelc_logger_create(&otel_err))) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init logs" : otel_err);

		retval = EX_SOFTWARE;
	}
	else if (cfg.otel_logger->start(cfg.otel_logger) == OTELC_RET_ERROR) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(cfg.otel_logger->err) ? "Unable to start logs" : cfg.otel_logger->err);

		retval = EX_SOFTWARE;
	}
	else {
		otelc_log_set_handler(log_handler_cb, NULL, false);

		retval = worker_run();
	}

	otelc_deinit(&(cfg.otel_tracer), &(cfg.otel_meter), &(cfg.otel_logger));

	OTELC_SFREE(otel_err);
	OTELC_LOG(stdout, "Program runtime: %" PRId64 " ms", OTELC_RUNTIME_MS());
	OTELC_MEMINFO();

	OTELC_RETURN_INT(retval);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
