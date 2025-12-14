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

#define OTEL_METER_ERROR(f, ...)            do { (void)otelc_sprintf(&(meter->err), f, ##__VA_ARGS__); OTELC_DBG(OTEL, "%s", meter->err); } while (0)
#define OTEL_METER_ERETURN(f, ...)          do { OTEL_METER_ERROR(f, ##__VA_ARGS__); OTELC_RETURN(); } while (0)
#define OTEL_METER_ERETURN_EX(t,r,f, ...)   do { OTEL_METER_ERROR(f, ##__VA_ARGS__); OTELC_RETURN##t(r); } while (0)
#define OTEL_METER_ERETURN_INT(f, ...)      OTEL_METER_ERETURN_EX(_INT, OTELC_RET_ERROR, f, ##__VA_ARGS__)
#define OTEL_METER_ERETURN_PTR(f, ...)      OTEL_METER_ERETURN_EX(_PTR, nullptr, f, ##__VA_ARGS__)

#define OTEL_INSTRUMENT_HANDLE(a)           (OTEL_HANDLE(otel_instrument, find)(a))
#define OTEL_DBG_INSTRUMENT()               OTEL_DBG_HANDLE(OTEL, "otel_instrument", otel_instrument)

#define T_CONSTRUCTOR(arg_ptr, arg_type, arg_member)                                                                              \
	otel_nostd::arg_ptr<otel_metrics::arg_type> arg_member;                                                                   \
	                                                                                                                          \
	T(otel_nostd::arg_ptr<otel_metrics::arg_type> arg_member##_, const char *name_, otelc_metric_instrument_t type_) noexcept \
	{                                                                                                                         \
		OTELC_FUNCPP("<" #arg_member ">, \"%s\", %d", OTELC_STRINGIFY(T), OTELC_STR_ARG(name_), type_);                   \
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

	~T() noexcept
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(T));

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
		OTELC_FUNCPP("\"%s\"", OTELC_STRINGIFY(T), OTELC_STR_ARG(name_));

		name = name_;

		OTELC_RETURN();
	}

	~T() noexcept
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(T));

		OTELC_RETURN();
	}
};
#undef T

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
 * Requires 'meter' in scope for OTEL_METER_ERETURN_INT.
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
		OTEL_METER_ERETURN_INT("Invalid OpenTelemetry meter instrument type: %d", (arg_instr)->type)


#ifdef OTELC_USE_STATIC_HANDLE
extern struct otel_handle<struct otel_instrument_handle *>  otel_instrument;
extern struct otel_handle<struct otel_view_handle *>        otel_view;
#else
extern struct otel_handle<struct otel_instrument_handle *> *otel_instrument;
extern struct otel_handle<struct otel_view_handle *>       *otel_view;
#endif

#endif /* _OPENTELEMETRY_C_WRAPPER_METER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
