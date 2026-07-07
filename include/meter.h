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
#ifndef _OPENTELEMETRY_C_WRAPPER_METER_H_
#define _OPENTELEMETRY_C_WRAPPER_METER_H_

#define OTEL_YAML_METER_PREFIX              "/signals/metrics"
#define OTEL_YAML_READERS                   "/readers"

#define OTEL_ERROR_MSG_INVALID_METER        "Invalid meter"
#define OTEL_ERROR_MSG_INVALID_INSTRUMENT   "Invalid instrument name"
#define OTEL_ERROR_MSG_INVALID_CALLBACK     "Invalid observable callback descriptor"
#define OTEL_ERROR_MSG_INVALID_VALUE        "Invalid value"
#define OTEL_ERROR_MSG_INSTRUMENT_TYPE      "Invalid OpenTelemetry meter instrument type: %d"
#define OTEL_ERROR_MSG_METER_PROVIDER       "Unable to get meter provider"
#define OTEL_ERROR_MSG_ADD_METRIC_READER    "Unable to add metric reader"

#define OTEL_METER_ERROR(f, ...)            OTEL_SIGNAL_ERROR(meter->err, f, ##__VA_ARGS__)
#define OTEL_METER_RETURN(f, ...)           OTEL_RETURN(meter, f, ##__VA_ARGS__)
#define OTEL_METER_RETURN_EX(t,r,f, ...)    OTEL_RETURN_EX(meter, t, (r), f, ##__VA_ARGS__)
#define OTEL_METER_RETURN_INT(f, ...)       OTEL_RETURN_INT(meter, f, ##__VA_ARGS__)
#define OTEL_METER_RETURN_PTR(f, ...)       OTEL_RETURN_PTR(meter, f, ##__VA_ARGS__)

#define OTEL_METER_LOGFILE(m)               (OTEL_CAST_STATIC(struct otel_meter_impl *, (m)->impl)->logfile)

#define OTEL_METER_IMPL(m)                  (OTEL_CAST_STATIC(struct otel_meter_impl *, (m)->impl))
#define OTEL_INSTRUMENT_HANDLE(a)           otel_map_find(OTEL_METER_IMPL(meter)->instrument.shards[0].map, (a))
#define OTEL_DBG_INSTRUMENT()                                                                              \
	OTELC_DBG(_OTEL, OTEL_HANDLE_FMT("otel_instrument"),                                               \
		OTEL_METER_IMPL(meter)->instrument.total_map_size(),                                       \
		OTEL_METER_IMPL(meter)->instrument.max_bucket_count(),                                     \
		OTEL_METER_IMPL(meter)->instrument.shards.size(),                                          \
		OTEL_METER_IMPL(meter)->instrument.id.load(),                                              \
		OTEL_METER_IMPL(meter)->instrument.peak_size.load(),                                       \
		OTEL_METER_IMPL(meter)->instrument.alloc_fail_cnt.load(),                                  \
		OTEL_METER_IMPL(meter)->instrument.erase_cnt.load(),                                       \
		OTEL_METER_IMPL(meter)->instrument.destroy_cnt.load())

#define T_CONSTRUCTOR(arg_ptr, arg_type, arg_member)                                                                              \
	otel_nostd::arg_ptr<otel_metrics::arg_type> arg_member;                                                                   \
	                                                                                                                          \
	T(otel_nostd::arg_ptr<otel_metrics::arg_type> arg_member##_, const char *name_, otelc_metric_instrument_t type_) noexcept \
	{                                                                                                                         \
		OTELCPP_FUNC("<" #arg_member ">, \"%s\", %d", OTELC_STRINGIFY(T), OTELC_STR_ARG(name_), type_);                   \
		                                                                                                                  \
		arg_member = std::move(arg_member##_);                                                                            \
		name       = name_;                                                                                               \
		type       = type_;                                                                                               \
		                                                                                                                  \
		OTELC_RETURN();                                                                                                   \
	}

#define T   otel_instrument_handle
struct T {
	/***
	 * Describes a metric instrument, including its name, type, and callback
	 * function used to produce observable measurements.
	 */
	std::string               name; /* Name of the metric instrument. */
	otelc_metric_instrument_t type; /* Type of the metric instrument. */

	T_CONSTRUCTOR(unique_ptr, Counter<uint64_t>,      counter_uint64)
	T_CONSTRUCTOR(unique_ptr, Counter<double>,        counter_double)
	T_CONSTRUCTOR(unique_ptr, Histogram<uint64_t>,    histogram_uint64)
	T_CONSTRUCTOR(unique_ptr, Histogram<double>,      histogram_double)
	T_CONSTRUCTOR(unique_ptr, UpDownCounter<int64_t>, udcounter_int64)
	T_CONSTRUCTOR(unique_ptr, UpDownCounter<double>,  udcounter_double)
	T_CONSTRUCTOR(shared_ptr, ObservableInstrument,   observable)
#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
	T_CONSTRUCTOR(unique_ptr, Gauge<int64_t>,         gauge_int64)
	T_CONSTRUCTOR(unique_ptr, Gauge<double>,          gauge_double)
#endif

	/***
	 * Checks whether the smart pointer matching 'type' was successfully
	 * constructed.  Meter::Create*() variants are 'noexcept' and their
	 * contract nowhere guarantees a non-empty pointer; this guard keeps
	 * an unexpectedly empty instrument from reaching the dispatch sites
	 * and dereferencing null there.
	 */
	bool is_valid() const noexcept
	{
		if (type == OTELC_METRIC_INSTRUMENT_COUNTER_UINT64)
			return !OTEL_NULL(counter_uint64);
		else if (type == OTELC_METRIC_INSTRUMENT_COUNTER_DOUBLE)
			return !OTEL_NULL(counter_double);
		else if (type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_UINT64)
			return !OTEL_NULL(histogram_uint64);
		else if (type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE)
			return !OTEL_NULL(histogram_double);
		else if (type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_INT64)
			return !OTEL_NULL(udcounter_int64);
		else if (type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_DOUBLE)
			return !OTEL_NULL(udcounter_double);
		else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64)
			return !OTEL_NULL(observable);
		else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_DOUBLE)
			return !OTEL_NULL(observable);
		else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_INT64)
			return !OTEL_NULL(observable);
		else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_DOUBLE)
			return !OTEL_NULL(observable);
		else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_INT64)
			return !OTEL_NULL(observable);
		else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_DOUBLE)
			return !OTEL_NULL(observable);
#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
		else if (type == OTELC_METRIC_INSTRUMENT_GAUGE_INT64)
			return !OTEL_NULL(gauge_int64);
		else if (type == OTELC_METRIC_INSTRUMENT_GAUGE_DOUBLE)
			return !OTEL_NULL(gauge_double);
#endif

		return false;
	}

	~T() noexcept
	{
		OTELCPP_FUNC("", OTELC_STRINGIFY(T));

		type             = OTEL_CAST_STATIC(otelc_metric_instrument_t, -1);

		counter_uint64   = nullptr;
		counter_double   = nullptr;
		histogram_uint64 = nullptr;
		histogram_double = nullptr;
		udcounter_int64  = nullptr;
		udcounter_double = nullptr;
		observable       = nullptr;
#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
		gauge_int64      = nullptr;
		gauge_double     = nullptr;
#endif

		OTELC_RETURN();
	}
};
#undef T

#define T   otel_view_handle
struct T {
	std::string name; /* Name of the metric view. */

	T(const char *name_) noexcept
	{
		OTELCPP_FUNC("\"%s\"", OTELC_STRINGIFY(T), OTELC_STR_ARG(name_));

		name = name_;

		OTELC_RETURN();
	}

	~T() noexcept
	{
		OTELCPP_FUNC("", OTELC_STRINGIFY(T));

		OTELC_RETURN();
	}
};
#undef T

/***
 * Takes the instrument map mutex shared, which is sufficient because the
 * looked-up handle is immutable after creation and the SDK instruments it
 * carries are thread-safe.  Handles are only erased under the exclusive
 * lock during meter teardown, and the documented destroy contract requires
 * all concurrent meter operations to be drained before destroy is invoked.
 */
#define OTEL_LOCK_INSTRUMENT_HANDLE(arg_type, arg_idx)           \
	OTEL_LOCK_METER_SHARED(instrument);                      \
	                                                         \
	const auto instrument = OTEL_INSTRUMENT_HANDLE(arg_idx); \
	if (OTEL_NULL(instrument))                               \
		OTEL_METER_RETURN##arg_type("Invalid OpenTelemetry meter instrument index: %d", (arg_idx));

/***
 * Generates an observable callback function that adapts the OpenTelemetry
 * observer interface to the C wrapper callback.
 */
#define OTEL_METER_OBSERVABLE_CB(arg_type, arg_state, arg_u_type, arg_u_value, arg_u_value_init, arg_fmt)              \
	OTELC_FUNC("<observer>, %p", (arg_state));                                                                     \
	                                                                                                               \
	if (OTEL_NULL(arg_state))                                                                                      \
		OTELC_RETURN();                                                                                        \
	                                                                                                               \
	struct otelc_value value_;                                                                                     \
	value_.u_type        = (arg_u_type);                                                                           \
	value_.u.arg_u_value = (arg_u_value_init);                                                                     \
	                                                                                                               \
	auto *data_ = OTEL_CAST_REINTERPRET(struct otelc_metric_observable_cb *, (arg_state));                         \
	data_->value = &value_;                                                                                        \
	OTEL_CAST_REINTERPRET(otelc_metric_observable_instrument_cb_t, data_->func)(data_);                            \
	OTELC_DBG_VALUE(DEBUG, arg_fmt, &value_);                                                                      \
	                                                                                                               \
	const auto _obs_ = otel_nostd::get<otel_nostd::shared_ptr<otel_metrics::ObserverResultT<arg_type>>>(observer); \
	_obs_->Observe(value_.u.arg_u_value);                                                                          \
	                                                                                                               \
	OTELC_RETURN();


#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
#  define OTEL_METER_OBSERVABLE_DISPATCH_GAUGE(arg_instr)                   \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_GAUGE_INT64)  \
		/* Do nothing. */;                                          \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_GAUGE_DOUBLE) \
		/* Do nothing. */;
#else
#  define OTEL_METER_OBSERVABLE_DISPATCH_GAUGE(arg_instr)
#endif

/***
 * Dispatches an observable callback method (AddCallback or RemoveCallback)
 * based on the instrument type.  For synchronous instrument types the macro
 * is a no-op; for observable types it invokes the appropriate int64 or double
 * callback adapter.
 *
 * Requires 'meter' in scope for OTEL_METER_RETURN_INT.
 */
#define OTEL_METER_OBSERVABLE_DISPATCH(arg_instr, arg_meth, arg_state)                                                          \
	if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_COUNTER_UINT64)                                                        \
		/* Do nothing. */;                                                                                              \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_COUNTER_DOUBLE)                                                   \
		/* Do nothing. */;                                                                                              \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_UINT64)                                                 \
		/* Do nothing. */;                                                                                              \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE)                                                 \
		/* Do nothing. */;                                                                                              \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_INT64)                                                  \
		/* Do nothing. */;                                                                                              \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_DOUBLE)                                                 \
		/* Do nothing. */;                                                                                              \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64)                                         \
		(arg_instr)->observable->arg_meth(otel_meter_observable_int64_cb, OTEL_CAST_REINTERPRET(void *, (arg_state)));  \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_DOUBLE)                                        \
		(arg_instr)->observable->arg_meth(otel_meter_observable_double_cb, OTEL_CAST_REINTERPRET(void *, (arg_state))); \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_INT64)                                           \
		(arg_instr)->observable->arg_meth(otel_meter_observable_int64_cb, OTEL_CAST_REINTERPRET(void *, (arg_state)));  \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_DOUBLE)                                          \
		(arg_instr)->observable->arg_meth(otel_meter_observable_double_cb, OTEL_CAST_REINTERPRET(void *, (arg_state))); \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_INT64)                                       \
		(arg_instr)->observable->arg_meth(otel_meter_observable_int64_cb, OTEL_CAST_REINTERPRET(void *, (arg_state)));  \
	else if ((arg_instr)->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_DOUBLE)                                      \
		(arg_instr)->observable->arg_meth(otel_meter_observable_double_cb, OTEL_CAST_REINTERPRET(void *, (arg_state))); \
	OTEL_METER_OBSERVABLE_DISPATCH_GAUGE(arg_instr)                                                                         \
	else                                                                                                                    \
		OTEL_METER_RETURN_INT(OTEL_ERROR_MSG_INSTRUMENT_TYPE, (arg_instr)->type)


/***
 * Per-instance implementation state for a meter.  Holds the SDK MeterProvider,
 * the SDK Meter obtained from it, the instrument and view handle maps used by
 * this meter, and the ostream exporter logfile.  All members are owned by the
 * instance, so multiple meters can coexist without sharing process-wide state.
 * The handle maps use otel_shared_mutex so lookups and instrument updates
 * (OTEL_LOCK_METER_SHARED) can run concurrently, while registration and
 * teardown (OTEL_LOCK_METER) remain exclusive.  The instrument_index maps the
 * case-folded instrument key built by otel_meter_instrument_key() to the
 * instrument ID and is guarded by the same mutex as the instrument map.
 *
 * The create_mutex serializes instrument and view creation.  A thread that
 * misses the shared-lock probe first takes create_mutex and re-probes under
 * the shared lock, so of a whole startup herd racing for the same name only
 * the winner ever acquires the exclusive map lock; every other thread leaves
 * through the shared re-probe.  Lock order: create_mutex first, then the
 * handle map mutex; never the other way around.
 */
struct otel_meter_impl {
	otel_nostd::shared_ptr<otel_metrics::MeterProvider>                          provider;
	otel_nostd::shared_ptr<otel_metrics::Meter>                                  meter;
	std::ofstream                                                                logfile;
	struct otel_handle<struct otel_instrument_handle *, true, otel_shared_mutex> instrument{1};
	struct otel_handle<struct otel_view_handle *, true, otel_shared_mutex>       view{1};
	std::unordered_map<std::string, int64_t>                                     instrument_index;
	std::mutex                                                                   create_mutex;

	otel_meter_impl();
	~otel_meter_impl();
};

#endif /* _OPENTELEMETRY_C_WRAPPER_METER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
