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
#define OTELC_STATIC_ASSERT_METER
#include "include.h"


static otel_nostd::shared_ptr<otel_metrics::Meter>          otel_meter_owner{};
static std::atomic<otel_metrics::Meter *>                   otel_meter{nullptr};
#ifdef OTELC_USE_STATIC_HANDLE
struct otel_handle<struct otel_view_handle *>               otel_view{1};
struct otel_handle<struct otel_instrument_handle *>         otel_instrument{1};
#else
struct otel_handle<struct otel_view_handle *>              *otel_view = nullptr;
struct otel_handle<struct otel_instrument_handle *>        *otel_instrument = nullptr;
#endif

#ifdef OTELC_USE_INSTRUMENT_VALIDATOR
static const otel_sdk_metrics::InstrumentMetaDataValidator  otel_instrument_validator;
#endif


/***
 * NAME
 *   otel_meter_observable_int64_cb - invokes an observable int64 callback
 *
 * SYNOPSIS
 *   static void otel_meter_observable_int64_cb(otel_metrics::ObserverResult observer, void *state)
 *
 * ARGUMENTS
 *   observer - observer result used to record observed int64 values
 *   state    - opaque pointer to the observable callback descriptor
 *
 * DESCRIPTION
 *   Invokes the user-defined observable callback associated with an int64
 *   metric.  The callback is called with the provided observer result and
 *   is expected to record one or more int64 values using the observer.
 *
 *   This function acts as an adapter between the OpenTelemetry observer
 *   interface and the internally stored callback representation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_meter_observable_int64_cb(otel_metrics::ObserverResult observer, void *state)
{
	OTEL_METER_OBSERVABLE_CB(int64_t, state, OTELC_VALUE_INT64, value_int64, 0, "value_int64 ");
}


/***
 * NAME
 *   otel_meter_observable_double_cb - invokes an observable double callback
 *
 * SYNOPSIS
 *   static void otel_meter_observable_double_cb(otel_metrics::ObserverResult observer, void *state)
 *
 * ARGUMENTS
 *   observer - observer result used to record observed double values
 *   state    - opaque pointer to the observable callback descriptor
 *
 * DESCRIPTION
 *   Invokes the user-defined observable callback associated with a double
 *   metric.  The callback is called with the provided observer result and
 *   is expected to record one or more double values using the observer.
 *
 *   This function acts as an adapter between the OpenTelemetry observer
 *   interface and the internally stored callback representation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_meter_observable_double_cb(otel_metrics::ObserverResult observer, void *state)
{
	OTEL_METER_OBSERVABLE_CB(double, state, OTELC_VALUE_DOUBLE, value_double, 0.0, "value_double ");
}


/***
 * NAME
 *   otel_meter_get_view_id - checks whether a view exists for a meter
 *
 * SYNOPSIS
 *   static int64_t otel_meter_get_view_id(const char *name)
 *
 * ARGUMENTS
 *   name - name of the view to look up
 *
 * DESCRIPTION
 *   Checks whether a view with the specified name is registered for a meter.
 *   This function performs a lookup using the provided name and determines
 *   if a matching view is present.
 *
 * RETURN VALUE
 *   Returns the ID of the view if found, or OTELC_RET_ERROR if not found.
 */
static int64_t otel_meter_get_view_id(const char *name)
{
	if (OTEL_NULL(name))
		return OTELC_RET_ERROR;

	const auto view = std::find_if(
		OTEL_HANDLE(otel_view, shards[0].map).begin(),
		OTEL_HANDLE(otel_view, shards[0].map).end(),
		[&](const auto &it) {
			return otel_sdk_metrics::InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals(it.second->name, name);
		}
	);

	if (view != OTEL_HANDLE(otel_view, shards[0].map).end()) {
		OTELC_DBG(OTEL, "view found: %" PRId64, view->first);

		return view->first;
	}

	return OTELC_RET_ERROR;
}


/***
 * NAME
 *   otel_meter_add_view - adds a metrics view to a meter
 *
 * SYNOPSIS
 *   static int64_t otel_meter_add_view(struct otelc_meter *meter, const char *view_name, const char *view_desc, const char *instrument_name, const char *instrument_unit, otelc_metric_instrument_t instrument_type, otelc_metric_aggregation_type_t aggregation_type, const double *bounds, size_t bounds_num)
 *
 * ARGUMENTS
 *   meter            - meter instance
 *   view_name        - name of the view
 *   view_desc        - description of the view
 *   instrument_name  - name of the instrument the view applies to
 *   instrument_unit  - unit of the instrument
 *   instrument_type  - type of the instrument
 *   aggregation_type - aggregation strategy used by the view
 *   bounds           - array of histogram bucket boundaries
 *   bounds_num       - number of elements in the bounds array
 *
 * DESCRIPTION
 *   Creates and registers a metrics view for the specified meter.  The view
 *   defines how measurements from a matching instrument are aggregated and
 *   reported, including the instrument name, unit, type, and aggregation
 *   strategy.  This allows customization of metric aggregation behavior without
 *   modifying the instrument itself.  The optional aggregation configuration
 *   can be used to fine-tune aggregation behavior, such as histogram bucket
 *   boundaries.
 *
 *   Note: the view must be registered before the instrument is created,
 *   because in the OpenTelemetry C++ SDK views are not dynamically applied
 *   to existing instruments.
 *
 * RETURN VALUE
 *   Returns the ID of the added view, or OTELC_RET_ERROR in case of an error.
 */
static int64_t otel_meter_add_view(struct otelc_meter *meter, const char *view_name, const char *view_desc, const char *instrument_name, const char *instrument_unit, otelc_metric_instrument_t instrument_type, otelc_metric_aggregation_type_t aggregation_type, const double *bounds, size_t bounds_num)
{
	OTEL_LOCK_METER(view);
	std::shared_ptr<otel_sdk_metrics::HistogramAggregationConfig> config{};
	otel_sdk_metrics::InstrumentType                              instr_type;
	otel_sdk_metrics::AggregationType                             aggr_type;
	int64_t                                                       view_id;

	OTELC_FUNC("%p, \"%s\", \"%s\", \"%s\", \"%s\", %d, %d, %p, %zu", meter, OTELC_STR_ARG(view_name), OTELC_STR_ARG(view_desc), OTELC_STR_ARG(instrument_name), OTELC_STR_ARG(instrument_unit), instrument_type, aggregation_type, bounds, bounds_num);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(view_name))
		OTEL_METER_ERETURN_INT("Invalid view name");
	else if (OTEL_NULL(instrument_name))
		OTEL_METER_ERETURN_INT("Invalid instrument name");

	OTEL_ARG_DEFAULT(view_desc, "");
	OTEL_ARG_DEFAULT(instrument_unit, "");

	/* If a view with the same name already exists, it will not be added. */
	if ((view_id = otel_meter_get_view_id(view_name)) != OTELC_RET_ERROR)
		OTELC_RETURN_EX(view_id, int64_t, "%" PRId64);

#define OTELC_METRIC_INSTRUMENT_DEF(a,b)   otel_sdk_metrics::InstrumentType::b,
	static constexpr otel_sdk_metrics::InstrumentType instrument_type_map[] = { OTELC_METRIC_INSTRUMENT_DEFINES };
#undef OTELC_METRIC_INSTRUMENT_DEF

	/***
	 * Range check is valid because instrument_type_map is generated in the
	 * same order as the C enum, making the mapping one-to-one.
	 */
	if (!OTELC_IN_RANGE(instrument_type, 0, OTELC_TABLESIZE_1(instrument_type_map)))
		OTEL_METER_ERETURN_INT("Invalid OpenTelemetry meter instrument type: %d", instrument_type);
	instr_type = instrument_type_map[instrument_type];

	/***
	 * Direct cast is safe because the enums are one-to-one (verified by
	 * static_assert).
	 */
	if (!OTELC_IN_RANGE(aggregation_type, OTELC_METRIC_AGGREGATION_DROP, OTELC_METRIC_AGGREGATION_BASE2_EXPONENTIAL_HISTOGRAM))
		OTEL_METER_ERETURN_INT("Invalid OpenTelemetry meter aggregation type: %d", aggregation_type);
	aggr_type = OTEL_CAST_STATIC(otel_sdk_metrics::AggregationType, aggregation_type);

	if ((instr_type == otel_sdk_metrics::InstrumentType::kHistogram) && !OTEL_NULL(bounds)) {
#ifdef DEBUG
		char    buffer[BUFSIZ] = "";
		size_t  len = 0;
		ssize_t n = 0;

		for (size_t i = 0; (i < bounds_num) && (len < sizeof(buffer)); i++, len += n) {
			n = snprintf(buffer + len, sizeof(buffer) - len, "%s%.2f", (len == 0) ? "" : " ", bounds[i]);
			if (!OTELC_IN_RANGE(n, 0, OTEL_CAST_STATIC(ssize_t, sizeof(buffer) - len - 1)))
				break;
		}
		OTELC_DBG(DEBUG, "adding bounds: %p:{ %s }", bounds, buffer);
#endif /* DEBUG */

		if (bounds_num == 0)
			OTEL_METER_ERETURN_INT("Invalid number of histogram view boundaries: 0");

#if 1
		/***
		 * The allocation of HistogramAggregationConfig lacked exception
		 * safety, as the std::shared_ptr constructor may throw
		 * std::bad_alloc during memory allocation.
		 */
		try {
			OTEL_DBG_THROW();
			config = std::shared_ptr<otel_sdk_metrics::HistogramAggregationConfig>(new(std::nothrow) otel_sdk_metrics::HistogramAggregationConfig());
		}
		OTEL_CATCH_ERETURN( , OTEL_METER_ERETURN_INT, "Unable to create histogram aggregation config")
#else
		/***
		 * NOTE: This would be more logical, but due to the following
		 * warning it is not used:
		 *
		 * ../include/std.h:49:74: warning: noexcept-expression evaluates to 'false' because of a call to 'opentelemetry::v2::sdk::metrics::HistogramAggregationConfig::HistogramAggregationConfig(size_t)' [-Wnoexcept]
		 */
		config = otel::make_shared_nothrow<otel_sdk_metrics::HistogramAggregationConfig>();
#endif
		if (OTEL_NULL(config))
			OTEL_METER_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("histogram aggregation config"));

		for (size_t i = 0; i < bounds_num; i++)
			try {
				OTEL_DBG_THROW();
				config->boundaries_.emplace_back(bounds[i]);
			}
			OTEL_CATCH_ERETURN( , OTEL_METER_ERETURN_INT, "Unable to add histogram view boundaries")
	}

	/* Create a selector to filter instruments by type, name, and unit. */
	auto instrument_selector = otel::make_unique_nothrow<otel_sdk_metrics::InstrumentSelector>(instr_type, instrument_name, instrument_unit);
	if (OTEL_NULL(instrument_selector))
		OTEL_METER_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("instrument selector"));

	/* Create a selector to filter meters by scope name, version, and schema. */
	auto meter_selector = otel::make_unique_nothrow<otel_sdk_metrics::MeterSelector>(meter->scope_name, OTELC_SCOPE_VERSION, OTELC_SCOPE_SCHEMA_URL);
	if (OTEL_NULL(meter_selector))
		OTEL_METER_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("meter selector"));

	/* Create the view with the given name, description, and aggregation. */
	auto view = otel::make_unique_nothrow<otel_sdk_metrics::View>(view_name, view_desc, aggr_type, std::move(config));
	if (OTEL_NULL(view))
		OTEL_METER_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("view"));

	/* Register the view with the provider and track it in the handle map. */
	const auto provider_sdk = OTEL_METER_PROVIDER();
	if (!OTEL_NULL(provider_sdk)) {
		std::pair<std::unordered_map<int64_t, struct otel_view_handle *>::iterator, bool> emplace_status{};

		/* Register the view with the meter provider. */
		provider_sdk->AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

		/* Create a handle to track the view in the internal map. */
		const auto view_handle = new(std::nothrow) otel_view_handle{view_name};
		if (OTEL_NULL(view_handle))
			OTEL_METER_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("view handle"));

		try {
			OTEL_DBG_THROW();
			emplace_status = OTEL_HANDLE(otel_view, shards[0].map).emplace(OTEL_HANDLE(otel_view, id), view_handle);
		}
		OTEL_CATCH_ERETURN(delete view_handle, OTEL_METER_ERETURN_INT, "Unable to add meter instrument view")

		if (emplace_status.second) {
			OTELC_DBG(OTEL, "View added");
		} else {
			delete view_handle;

			OTEL_METER_ERETURN_INT("Unable to add meter instrument view: duplicate id %" PRId64, OTEL_HANDLE(otel_view, id).load());
		}

		OTEL_HANDLE_PEAK_SIZE(otel_view, shards[0]);
	} else {
		OTEL_METER_ERETURN_INT("Unable to get meter provider");
	}

	OTELC_RETURN_EX(OTEL_HANDLE(otel_view, id++), int64_t, "%" PRId64);
}


/***
 * NAME
 *   otel_meter_get_instrument - retrieves an instrument ID by name and type
 *
 * SYNOPSIS
 *   static int64_t otel_meter_get_instrument(struct otelc_meter *meter, const char *name, otelc_metric_instrument_t type)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   name  - name of the instrument to look up (case-insensitive)
 *   type  - instrument type to match
 *
 * DESCRIPTION
 *   Searches the internal instrument registry for an instrument with the given
 *   name and type.  The name comparison is case-insensitive, consistent with
 *   the OpenTelemetry specification.
 *
 * RETURN VALUE
 *   Returns the instrument ID on success, or OTELC_RET_ERROR if no matching
 *   instrument is found.
 */
static int64_t otel_meter_get_instrument(struct otelc_meter *meter, const char *name, otelc_metric_instrument_t type)
{
	OTELC_FUNC("%p, \"%s\", %d", meter, OTELC_STR_ARG(name), type);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(name))
		OTEL_METER_ERETURN_INT("Invalid instrument name");

	OTEL_LOCK_METER(instrument);

	/* Search for an existing instrument by name and type. */
	const auto instrument = std::find_if(
		OTEL_HANDLE(otel_instrument, shards[0].map).begin(),
		OTEL_HANDLE(otel_instrument, shards[0].map).end(),
		[&](const auto &it) {
			return otel_sdk_metrics::InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals(it.second->name, name) && (it.second->type == type);
		}
	);

	if (instrument != OTEL_HANDLE(otel_instrument, shards[0].map).end()) {
		OTELC_DBG(OTEL, "instrument found: %" PRId64, instrument->first);

		OTELC_RETURN_INT(instrument->first);
	}

	OTEL_METER_ERETURN_EX(_INT, OTELC_RET_ERROR, "Instrument not found: \"%s\" type %d", name, type);
}


/***
 * NAME
 *   otel_nolock_meter_add_instrument_callback - registers a callback for an observable instrument
 *
 * SYNOPSIS
 *   static int otel_nolock_meter_add_instrument_callback(struct otelc_meter *meter, struct otel_instrument_handle *instrument, struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   meter      - meter instance
 *   instrument - pointer to the instrument handle
 *   data       - observable callback descriptor to register
 *
 * DESCRIPTION
 *   This is the non-locking version of the otel_meter_add_instrument_callback()
 *   function.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if the instrument type
 *   is invalid.
 */
static int otel_nolock_meter_add_instrument_callback(struct otelc_meter *meter, struct otel_instrument_handle *instrument, struct otelc_metric_observable_cb *data)
{
	OTELC_FUNC("%p, <instrument>, %p", meter, data);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(instrument))
		OTEL_METER_ERETURN_INT("Invalid OpenTelemetry meter instrument");
	else if (OTEL_NULL(data))
		OTEL_METER_ERETURN_INT("Invalid observable callback descriptor");

	OTEL_METER_OBSERVABLE_DISPATCH(instrument, AddCallback, data);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_meter_add_instrument_callback - registers a callback for an observable instrument
 *
 * SYNOPSIS
 *   static int otel_meter_add_instrument_callback(struct otelc_meter *meter, int idx, struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   idx   - instrument identifier
 *   data  - observable callback descriptor to register
 *
 * DESCRIPTION
 *   Registers an observation callback for the specified metric instrument.
 *   The instrument is identified by its instrument ID, as returned by
 *   otel_meter_create_instrument().  This function is applicable only to
 *   observable instrument types; the callback will be invoked by the metrics
 *   SDK during collection to produce measurement values.  For non-observable
 *   instruments, this function has no effect.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if the instrument type
 *   is invalid.
 */
static int otel_meter_add_instrument_callback(struct otelc_meter *meter, int idx, struct otelc_metric_observable_cb *data)
{
	OTELC_FUNC("%p, %d, %p", meter, idx, data);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(data))
		OTEL_METER_ERETURN_INT("Invalid observable callback descriptor");

	OTEL_LOCK_METER(instrument);

	OTELC_RETURN_INT(otel_nolock_meter_add_instrument_callback(meter, OTEL_INSTRUMENT_HANDLE(idx), data));
}


/***
 * NAME
 *   otel_meter_remove_instrument_callback - unregisters a callback for an observable instrument
 *
 * SYNOPSIS
 *   static int otel_meter_remove_instrument_callback(struct otelc_meter *meter, int idx, struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   idx   - instrument identifier
 *   data  - observable callback descriptor to unregister
 *
 * DESCRIPTION
 *   Removes a previously registered observation callback for the specified
 *   metric instrument.  The instrument is identified by its instrument ID, as
 *   returned by otel_meter_create_instrument().  This function only affects
 *   observable instruments; for non-observable instruments, it performs no
 *   operation.  After removal, the callback will no longer be invoked during
 *   metrics collection.
 *
 *   Multiple callback functions can be registered on the same instrument.
 *   Therefore, when removing a callback, the specific function instance must
 *   be provided.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if the instrument type
 *   is invalid.
 */
static int otel_meter_remove_instrument_callback(struct otelc_meter *meter, int idx, struct otelc_metric_observable_cb *data)
{
	OTELC_FUNC("%p, %d, %p", meter, idx, data);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(data))
		OTEL_METER_ERETURN_INT("Invalid observable callback descriptor");

	OTEL_LOCK_INSTRUMENT_HANDLE(_INT, idx);

	OTEL_METER_OBSERVABLE_DISPATCH(instrument, RemoveCallback, data);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_meter_create_instrument - creates a metric instrument
 *
 * SYNOPSIS
 *   static int64_t otel_meter_create_instrument(struct otelc_meter *meter, const char *name, const char *desc, const char *unit, otelc_metric_instrument_t type, struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   name  - name of the instrument
 *   desc  - description of the instrument
 *   unit  - unit of measurement
 *   type  - instrument type
 *   data  - observable callback descriptor, or NULL for synchronous instruments
 *
 * DESCRIPTION
 *   Creates and registers a metric instrument with the specified meter.
 *   The instrument type determines whether the instrument is synchronous or
 *   observable.  For observable instruments, the provided callback function is
 *   invoked to collect measurements.  For synchronous instruments, the callback
 *   is ignored.  This function encapsulates the common logic required to create
 *   different metric instrument types in the C wrapper library.
 *
 * RETURN VALUE
 *   Returns a non-negative instrument ID on success, or OTELC_RET_ERROR on
 *   failure.
 */
static int64_t otel_meter_create_instrument(struct otelc_meter *meter, const char *name, const char *desc, const char *unit, otelc_metric_instrument_t type, struct otelc_metric_observable_cb *data)
{
	OTELC_FUNC("%p, \"%s\", \"%s\", \"%s\", %d, %p", meter, OTELC_STR_ARG(name), OTELC_STR_ARG(desc), OTELC_STR_ARG(unit), type, data);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(name))
		OTEL_METER_ERETURN_INT("Invalid instrument name");
	else if (OTEL_NULL(data)) {
		if (OTEL_METRIC_INSTRUMENT_IS_OBSERVABLE(type))
			OTEL_METER_ERETURN_INT("Missing observable callback for observable instrument");
	}
	else if (!OTEL_METRIC_INSTRUMENT_IS_OBSERVABLE(type))
		OTEL_METER_ERETURN_INT("Unexpected callback function for synchronous instrument");

	OTEL_ARG_DEFAULT(desc, "");
	OTEL_ARG_DEFAULT(unit, "");

#ifdef OTELC_USE_INSTRUMENT_VALIDATOR
	if (!otel_instrument_validator.ValidateName(name))
		OTEL_METER_ERETURN_INT("Invalid instrument name: \"%s\"", name);
	else if (!otel_instrument_validator.ValidateUnit(unit))
		OTEL_METER_ERETURN_INT("Invalid instrument unit: \"%s\"", unit);
	else if (!otel_instrument_validator.ValidateDescription(desc))
		OTEL_METER_ERETURN_INT("Invalid instrument description: \"%s\"", desc);
#endif

	auto *meter_ptr = otel_meter.load();
	if (OTEL_NULL(meter_ptr))
		OTEL_METER_ERETURN_INT("Invalid meter");

	OTEL_LOCK_METER(instrument);

	/***
	 * Returns the instrument ID if the instrument already exists.  The
	 * lookup is based on both the instrument name and type, with the name
	 * compared case-insensitively.
	 */
#if 0
	for (const auto &it : OTEL_HANDLE(otel_instrument, shards[0].map))
		if (otel_sdk_metrics::InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals(it.second->name, name) && (it.second->type == type)) {
			OTELC_DBG(OTEL, "instrument found: %" PRId64, it.first);

			OTELC_RETURN_INT(it.first);
		}
#endif

	const auto instrument = std::find_if(
		OTEL_HANDLE(otel_instrument, shards[0].map).begin(),
		OTEL_HANDLE(otel_instrument, shards[0].map).end(),
		[&](const auto &it) {
			return otel_sdk_metrics::InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals(it.second->name, name) && (it.second->type == type);
		}
	);

	if (instrument != OTEL_HANDLE(otel_instrument, shards[0].map).end()) {
		OTELC_DBG(OTEL, "instrument found: %" PRId64, instrument->first);

		OTELC_RETURN_INT(instrument->first);
	}

	struct otel_instrument_handle *instrument_handle = nullptr;

	/* Create the instrument handle for the requested type. */
	if (type == OTELC_METRIC_INSTRUMENT_COUNTER_UINT64)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateUInt64Counter(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_COUNTER_DOUBLE)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateDoubleCounter(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_UINT64)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateUInt64Histogram(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateDoubleHistogram(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_INT64)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateInt64UpDownCounter(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_DOUBLE)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateDoubleUpDownCounter(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateInt64ObservableCounter(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_DOUBLE)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateDoubleObservableCounter(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_INT64)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateInt64ObservableGauge(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_DOUBLE)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateDoubleObservableGauge(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_INT64)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateInt64ObservableUpDownCounter(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_DOUBLE)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateDoubleObservableUpDownCounter(name, desc, unit), name, type};
#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
	else if (type == OTELC_METRIC_INSTRUMENT_GAUGE_INT64)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateInt64Gauge(name, desc, unit), name, type};
	else if (type == OTELC_METRIC_INSTRUMENT_GAUGE_DOUBLE)
		instrument_handle = new(std::nothrow) otel_instrument_handle{meter_ptr->CreateDoubleGauge(name, desc, unit), name, type};
#endif
	else
		OTEL_METER_ERETURN_INT("Invalid OpenTelemetry meter instrument type: %d", type);

	if (OTEL_NULL(instrument_handle))
		OTEL_METER_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("instrument handle"));

	/* Store the instrument handle in the internal map. */
	std::pair<std::unordered_map<int64_t, struct otel_instrument_handle *>::iterator, bool> emplace_status{};
	try {
		OTEL_DBG_THROW();
		emplace_status = OTEL_HANDLE(otel_instrument, shards[0].map).emplace(OTEL_HANDLE(otel_instrument, id), instrument_handle);

		if (!emplace_status.second) {
			delete instrument_handle;

			OTEL_METER_ERETURN_INT("Unable to create meter instrument: duplicate id %" PRId64, OTEL_HANDLE(otel_instrument, id).load());
		}
		else if (OTEL_NULL(data)) {
			/* Do nothing. */
		}
		else if (otel_nolock_meter_add_instrument_callback(meter, instrument_handle, data) == OTELC_RET_ERROR) {
			OTEL_HANDLE(otel_instrument, shards[0].map).erase(OTEL_HANDLE(otel_instrument, id));

			delete instrument_handle;

			OTEL_METER_ERETURN_INT("Unable to register instrument callback");
		}

		OTELC_DBG(OTEL, "Instrument added");
	}
	OTEL_CATCH_ERETURN({
		if (emplace_status.second)
			OTEL_HANDLE(otel_instrument, shards[0].map).erase(OTEL_HANDLE(otel_instrument, id));

		delete instrument_handle;
		}, OTEL_METER_ERETURN_INT, "Unable to create meter instrument"
	)

	OTEL_HANDLE_PEAK_SIZE(otel_instrument, shards[0]);

	OTEL_DBG_INSTRUMENT();

	OTELC_RETURN_INT(OTEL_HANDLE(otel_instrument, id++));
}


/***
 * NAME
 *   otel_meter_instrument_value_type - validates the value type for a metric instrument
 *
 * SYNOPSIS
 *   static int otel_meter_instrument_value_type(struct otelc_meter *meter, const struct otelc_value *value, otelc_metric_instrument_t type)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   value - value to validate
 *   type  - instrument type to validate against
 *
 * DESCRIPTION
 *   Checks that the value type stored in value matches the type expected by
 *   the given instrument type.  Observable and unrecognized instrument types
 *   accept any value type.  For uint64 instruments (counter and histogram),
 *   an OTELC_VALUE_INT64 value is accepted if it is non-negative.  Negative
 *   int64 values are rejected with an error.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK if the value type is valid, or OTELC_RET_ERROR on
 *   a type mismatch or a negative int64 value for a uint64 instrument.
 */
static int otel_meter_instrument_value_type(struct otelc_meter *meter, const struct otelc_value *value, otelc_metric_instrument_t type)
{
	otelc_value_type_t expected_type;

	OTELC_FUNC("%p, %p, %d", meter, value, type);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(value))
		OTEL_METER_ERETURN_INT("Invalid value");

	/* Determine the expected value type for the instrument. */
	if ((type == OTELC_METRIC_INSTRUMENT_COUNTER_UINT64) || (type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_UINT64))
		expected_type = OTELC_VALUE_UINT64;
	else if ((type == OTELC_METRIC_INSTRUMENT_COUNTER_DOUBLE) || (type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE) || (type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_DOUBLE))
		expected_type = OTELC_VALUE_DOUBLE;
	else if (type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_INT64)
		expected_type = OTELC_VALUE_INT64;
#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
	else if (type == OTELC_METRIC_INSTRUMENT_GAUGE_INT64)
		expected_type = OTELC_VALUE_INT64;
	else if (type == OTELC_METRIC_INSTRUMENT_GAUGE_DOUBLE)
		expected_type = OTELC_VALUE_DOUBLE;
#endif
	else
		expected_type = OTELC_VALUE_NULL;

	/* Validate the value type, allowing int64-to-uint64 promotion. */
	if ((expected_type == OTELC_VALUE_NULL) || (value->u_type == expected_type))
		/* Do nothing. */;
	else if ((expected_type != OTELC_VALUE_UINT64) || (value->u_type != OTELC_VALUE_INT64))
		OTEL_METER_ERETURN_INT("Value type mismatch: instrument expects %d, got %d", expected_type, value->u_type);
	else if (value->u.value_int64 < 0)
		OTEL_METER_ERETURN_INT("Negative int64 value cannot be used for a uint64 instrument");

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_meter_update_instrument - updates a metric instrument with a value
 *
 * SYNOPSIS
 *   static int otel_meter_update_instrument(struct otelc_meter *meter, int idx, const struct otelc_value *value)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   idx   - instrument identifier
 *   value - value to record
 *
 * DESCRIPTION
 *   Updates the specified metric instrument with the provided value.
 *   The instrument is identified by its instrument ID, as returned by
 *   otel_meter_create_instrument().  For synchronous instruments, the value is
 *   recorded immediately.  For observable instruments, this function performs
 *   no operation, as values are collected via the observation callback.
 *
 *   For uint64 instruments (counter and histogram), an OTELC_VALUE_INT64 value
 *   is accepted if it is non-negative, and is cast to uint64_t.  Negative
 *   int64 values are rejected with an error.
 *
 * RETURN VALUE
 *   Returns the instrument ID on success, or OTELC_RET_ERROR if the instrument
 *   type is invalid.
 */
static int otel_meter_update_instrument(struct otelc_meter *meter, int idx, const struct otelc_value *value)
{
	OTELC_FUNC("%p, %d, %p", meter, idx, value);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(value))
		OTEL_METER_ERETURN_INT("Invalid value");

	OTEL_LOCK_INSTRUMENT_HANDLE(_INT, idx);

	if (otel_meter_instrument_value_type(meter, value, instrument->type) == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

#ifndef OTELC_USE_RUNTIME_CONTEXT
	otel_context::Context rt_context{};
#else
	const auto            rt_context = otel_context::RuntimeContext::GetCurrent();
#endif

	/* Dispatch the value to the appropriate instrument method. */
	if (instrument->type == OTELC_METRIC_INSTRUMENT_COUNTER_UINT64)
		instrument->counter_uint64->Add(OTELC_VALUE_TO_UINT64(value), rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_COUNTER_DOUBLE)
		instrument->counter_double->Add(value->u.value_double, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_UINT64)
		instrument->histogram_uint64->Record(OTELC_VALUE_TO_UINT64(value), rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE)
		instrument->histogram_double->Record(value->u.value_double, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_INT64)
		instrument->udcounter_int64->Add(value->u.value_int64, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_DOUBLE)
		instrument->udcounter_double->Add(value->u.value_double, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_DOUBLE)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_INT64)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_DOUBLE)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_INT64)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_DOUBLE)
		/* Do nothing. */;
#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_GAUGE_INT64)
		instrument->gauge_int64->Record(value->u.value_int64, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_GAUGE_DOUBLE)
		instrument->gauge_double->Record(value->u.value_double, rt_context);
#endif
	else
		OTEL_METER_ERETURN_INT("Invalid OpenTelemetry meter instrument type: %d", instrument->type);

	OTELC_RETURN_INT(idx);
}


/***
 * NAME
 *   otel_meter_update_instrument_kv_n - updates a metric instrument with a value and attributes
 *
 * SYNOPSIS
 *   static int otel_meter_update_instrument_kv_n(struct otelc_meter *meter, int idx, const struct otelc_value *value, const struct otelc_kv *kv, size_t kv_len)
 *
 * ARGUMENTS
 *   meter  - meter instance
 *   idx    - instrument identifier
 *   value  - value to record
 *   kv     - an array of key-value pairs of attributes to be set
 *   kv_len - size of key-value pair array
 *
 * DESCRIPTION
 *   Updates the specified metric instrument with the provided value and
 *   attributes.  The instrument is identified by its instrument ID, as
 *   returned by otel_meter_create_instrument().  For synchronous
 *   instruments, the value and attributes are recorded immediately.  For
 *   observable instruments, this function performs no operation.
 *
 *   For uint64 instruments (counter and histogram), an OTELC_VALUE_INT64 value
 *   is accepted if it is non-negative, and is cast to uint64_t.  Negative
 *   int64 values are rejected with an error.
 *
 * RETURN VALUE
 *   Returns the instrument ID on success, or OTELC_RET_ERROR if the instrument
 *   type is invalid.
 */
static int otel_meter_update_instrument_kv_n(struct otelc_meter *meter, int idx, const struct otelc_value *value, const struct otelc_kv *kv, size_t kv_len)
{
	std::map<std::string, otel_attribute_value> attr{};

	OTELC_FUNC("%p, %d, %p, %p, %zu", meter, idx, value, kv, kv_len);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(value))
		OTEL_METER_ERETURN_INT("Invalid value");

	OTEL_LOCK_INSTRUMENT_HANDLE(_INT, idx);

	if (otel_meter_instrument_value_type(meter, value, instrument->type) == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (!OTEL_NULL(kv) && (kv_len > 0))
		for (size_t i = 0; i < kv_len; i++)
			OTEL_VALUE_ADD(_INT, attr, emplace, kv[i].key, &(kv[i].value), &(meter->err), "Unable to add metric attribute");

#ifndef OTELC_USE_RUNTIME_CONTEXT
	otel_context::Context rt_context{};
#else
	const auto            rt_context = otel_context::RuntimeContext::GetCurrent();
#endif

	/* Dispatch the value and attributes to the appropriate instrument method. */
	if (instrument->type == OTELC_METRIC_INSTRUMENT_COUNTER_UINT64)
		instrument->counter_uint64->Add(OTELC_VALUE_TO_UINT64(value), attr, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_COUNTER_DOUBLE)
		instrument->counter_double->Add(value->u.value_double, attr, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_UINT64)
		instrument->histogram_uint64->Record(OTELC_VALUE_TO_UINT64(value), attr, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE)
		instrument->histogram_double->Record(value->u.value_double, attr, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_INT64)
		instrument->udcounter_int64->Add(value->u.value_int64, attr, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_UDCOUNTER_DOUBLE)
		instrument->udcounter_double->Add(value->u.value_double, attr, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_DOUBLE)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_INT64)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_DOUBLE)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_INT64)
		/* Do nothing. */;
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_DOUBLE)
		/* Do nothing. */;
#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_GAUGE_INT64)
		instrument->gauge_int64->Record(value->u.value_int64, attr, rt_context);
	else if (instrument->type == OTELC_METRIC_INSTRUMENT_GAUGE_DOUBLE)
		instrument->gauge_double->Record(value->u.value_double, attr, rt_context);
#endif
	else
		OTEL_METER_ERETURN_INT("Invalid OpenTelemetry meter instrument type: %d", instrument->type);

	OTELC_RETURN_INT(idx);
}


/***
 * NAME
 *   otel_meter_force_flush - forces the export of any buffered metrics
 *
 * SYNOPSIS
 *   static int otel_meter_force_flush(struct otelc_meter *meter, const struct timespec *timeout)
 *
 * ARGUMENTS
 *   meter   - meter instance
 *   timeout - maximum time to wait, or NULL for no limit
 *
 * DESCRIPTION
 *   Forces the meter to export all metrics that have been collected but not
 *   yet exported.  This is a blocking call that will not return until the
 *   export is complete.  The optional timeout argument limits how long the
 *   operation may block.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
static int otel_meter_force_flush(struct otelc_meter *meter, const struct timespec *timeout)
{
	OTEL_PROVIDER_OP(METER, meter, ForceFlush, "Unable to force flush meter provider");
}


/***
 * NAME
 *   otel_meter_shutdown - shuts down the underlying meter provider
 *
 * SYNOPSIS
 *   static int otel_meter_shutdown(struct otelc_meter *meter, const struct timespec *timeout)
 *
 * ARGUMENTS
 *   meter   - meter instance
 *   timeout - maximum time to wait, or NULL for no limit
 *
 * DESCRIPTION
 *   Shuts down the underlying meter provider, flushing any buffered metrics
 *   and releasing provider resources.  After shutdown, no further metrics
 *   will be exported.  The optional timeout argument limits how long the
 *   operation may block.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
static int otel_meter_shutdown(struct otelc_meter *meter, const struct timespec *timeout)
{
	OTEL_PROVIDER_OP(METER, meter, Shutdown, "Unable to shut down meter provider");
}


/***
 * NAME
 *   otel_meter_start - initializes and starts the meter instance
 *
 * SYNOPSIS
 *   static int otel_meter_start(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   The meter whose configuration is specified in the OpenTelemetry YAML
 *   configuration file (set by the previous call to the otelc_init() function)
 *   is started.  The function initializes the meter in such a way that the
 *   following components are initialized individually: one or more
 *   exporter-reader pairs, and finally provider.  When the YAML configuration
 *   specifies a sequence of exporters (and optionally a matching sequence of
 *   readers), each pair is created and passed to the provider.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_meter_start(struct otelc_meter *meter)
{
	std::vector<std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader>> readers;
	std::shared_ptr<otel_metrics::MeterProvider>                                  provider;
	char                                                                          scope_name[OTEL_YAML_BUFSIZ];
	int                                                                           retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p", meter);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(retval);

	retval = yaml_find(otelc_fyd, &(meter->err), 1, "OpenTelemetry meter instrumentation scope", OTEL_YAML_METER_PREFIX "/scope_name", scope_name, sizeof(scope_name));
	if (retval < 1)
		OTELC_RETURN_INT(retval);

	meter->scope_name = OTELC_STRDUP(__func__, __LINE__, scope_name);
	if (OTEL_NULL(meter->scope_name))
		OTEL_METER_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("scope name"));

	/* Build exporters and readers from YAML configuration. */
	if (yaml_is_sequence(otelc_fyd, OTEL_YAML_METER_PREFIX OTEL_YAML_EXPORTERS)) {
		const int count = yaml_get_sequence_len(otelc_fyd, &(meter->err), OTEL_YAML_METER_PREFIX OTEL_YAML_EXPORTERS);
		if (count < 0)
			OTELC_RETURN_INT(OTELC_RET_ERROR);

		int count_readers = yaml_get_sequence_len(otelc_fyd, &(meter->err), OTEL_YAML_METER_PREFIX OTEL_YAML_READERS);
		if (count_readers < 0)
			count_readers = 0;

		for (int i = 0; i < count; i++) {
			std::unique_ptr<otel_sdk_metrics::PushMetricExporter>            exporter;
			std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader> reader;
			char                                                             exporter_name[OTEL_YAML_BUFSIZ], reader_name[OTEL_YAML_BUFSIZ] = "";

			if (yaml_get_sequence_value(otelc_fyd, &(meter->err), OTEL_YAML_METER_PREFIX OTEL_YAML_EXPORTERS, i, exporter_name, sizeof(exporter_name)) != 1)
				OTELC_RETURN_INT(OTELC_RET_ERROR);

			if (!yaml_is_sequence(otelc_fyd, OTEL_YAML_METER_PREFIX OTEL_YAML_READERS)) {
				/* Do nothing. */
			}
			else if (i < count_readers) {
				if (yaml_get_sequence_value(otelc_fyd, &(meter->err), OTEL_YAML_METER_PREFIX OTEL_YAML_READERS, i, reader_name, sizeof(reader_name)) != 1)
					OTELC_RETURN_INT(OTELC_RET_ERROR);
			}
			else if (count_readers > 0) {
				if (yaml_get_sequence_value(otelc_fyd, &(meter->err), OTEL_YAML_METER_PREFIX OTEL_YAML_READERS, count_readers - 1, reader_name, sizeof(reader_name)) != 1)
					OTELC_RETURN_INT(OTELC_RET_ERROR);
			}

			if (otel_meter_exporter_create(meter, exporter, exporter_name) != OTELC_RET_OK)
				OTELC_RETURN_INT(OTELC_RET_ERROR);
			else if (otel_meter_reader_create(meter, exporter, reader, (*reader_name != '\0') ? reader_name : nullptr) != OTELC_RET_OK)
				OTELC_RETURN_INT(OTELC_RET_ERROR);

			try {
				OTEL_DBG_THROW();
				readers.push_back(std::move(reader));
			}
			OTEL_CATCH_ERETURN( , OTEL_METER_ERETURN_INT, "Unable to add metric reader")
		}
	} else {
		std::unique_ptr<otel_sdk_metrics::PushMetricExporter>            exporter;
		std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader> reader;

		/* Use default exporter and reader when no sequence is defined. */
		if ((retval = otel_meter_exporter_create(meter, exporter)) == OTELC_RET_ERROR)
			OTELC_RETURN_INT(retval);
		else if ((retval = otel_meter_reader_create(meter, exporter, reader)) == OTELC_RET_ERROR)
			OTELC_RETURN_INT(retval);

		try {
			OTEL_DBG_THROW();
			readers.push_back(std::move(reader));
		}
		OTEL_CATCH_ERETURN( , OTEL_METER_ERETURN_INT, "Unable to add metric reader")
	}

	/* Create the provider and meter, then install them as globals. */
	if ((retval = otel_meter_provider_create(meter, readers, provider)) != OTELC_RET_ERROR) {
		otel_nostd::shared_ptr<otel_metrics::Meter> meter_maybe{};

		meter_maybe = provider->GetMeter(meter->scope_name, OTELC_SCOPE_VERSION, OTELC_SCOPE_SCHEMA_URL);
		if (OTEL_NULL(meter_maybe))
			OTEL_METER_ERETURN_INT("Unable to get meter from provider");

		otel_meter_owner = std::move(meter_maybe);
		otel_meter.store(otel_meter_owner.get());
		otel_metrics::Provider::SetMeterProvider(provider);
	}

	OTELC_DBG_METER(OTEL, "meter", meter);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_meter_enabled - checks whether the meter is enabled
 *
 * SYNOPSIS
 *   static int otel_meter_enabled(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Queries the underlying meter to determine whether it is enabled.  Callers
 *   can use this check to skip expensive metric setup when the meter would
 *   discard the data anyway.  The OpenTelemetry C++ SDK does not yet provide
 *   a Meter::Enabled() method, so this function currently returns true
 *   whenever the meter is valid.
 *
 * RETURN VALUE
 *   Returns true if the meter is enabled, false if it is not,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_meter_enabled(struct otelc_meter *meter)
{
	OTELC_FUNC("%p", meter);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	auto *meter_ptr = otel_meter.load();
	if (OTEL_NULL(meter_ptr))
		OTEL_METER_ERETURN_INT("Invalid meter");

	OTELC_RETURN_INT(true);
}


/***
 * NAME
 *   otel_meter_destroy - stops the meter and frees all associated memory
 *
 * SYNOPSIS
 *   static void otel_meter_destroy(struct otelc_meter **meter)
 *
 * ARGUMENTS
 *   meter - address of a meter instance pointer to be stopped and destroyed
 *
 * DESCRIPTION
 *   Stops the meter and releases all resources and memory associated with the
 *   meter instance.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_meter_destroy(struct otelc_meter **meter)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(meter));

	if (OTEL_NULL(meter) || OTEL_NULL(*meter))
		OTELC_RETURN();

	OTELC_DBG_METER(OTEL, "meter", *meter);

	/***
	 * Clear otel_meter before provider teardown to prevent concurrent
	 * callers from accessing a meter that is being destroyed.
	 */
	if (!OTEL_NULL(otel_meter)) {
		otel_meter.store(nullptr);
		otel_meter_owner = {};
	}

	otel_meter_provider_destroy();
	otel_meter_exporter_destroy();

#ifdef OTELC_USE_STATIC_HANDLE
	OTEL_HANDLE(otel_view, clear_locked());
	OTEL_HANDLE(otel_instrument, clear_locked());

#else

	if (!OTEL_NULL(otel_view)) {
		OTEL_HANDLE(otel_view, clear_locked());

		delete otel_view;
		otel_view = nullptr;
	}

	if (!OTEL_NULL(otel_instrument)) {
		OTEL_HANDLE(otel_instrument, clear_locked());

		delete otel_instrument;
		otel_instrument = nullptr;
	}
#endif /* OTELC_USE_STATIC_HANDLE */

	OTELC_SFREE((*meter)->err);
	OTELC_SFREE((*meter)->scope_name);
	OTELC_SFREE_CLEAR(*meter);

	OTELC_RETURN();
}


/* The meter operations vtable. */
const static struct otelc_meter_ops otel_meter_ops = {
	.create_instrument          = otel_meter_create_instrument,          /* lock otel_instrument */
	.update_instrument          = otel_meter_update_instrument,          /* lock otel_instrument */
	.update_instrument_kv_n     = otel_meter_update_instrument_kv_n,     /* lock otel_instrument */
	.add_instrument_callback    = otel_meter_add_instrument_callback,    /* lock otel_instrument */
	.remove_instrument_callback = otel_meter_remove_instrument_callback, /* lock otel_instrument */
	.add_view                   = otel_meter_add_view,                   /* lock otel_view */
	.get_instrument             = otel_meter_get_instrument,             /* lock otel_instrument */
	.enabled                    = otel_meter_enabled,                    /* Locking not required. */
	.force_flush                = otel_meter_force_flush,                /* Locking not required. */
	.shutdown                   = otel_meter_shutdown,                   /* Locking not required. */
	.start                      = otel_meter_start,                      /* Locking not required. */
	.destroy                    = otel_meter_destroy,                    /* Locking not required. */
};


/***
 * NAME
 *   otel_meter_new - creates a new meter instance in an unstarted state
 *
 * SYNOPSIS
 *   static struct otelc_meter *otel_meter_new(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Allocates and initializes a new meter instance in an unstarted state.
 *   The caller is responsible for starting and eventually destroying the
 *   meter.
 *
 * RETURN VALUE
 *   Returns a pointer to a newly created meter instance on success, or nullptr
 *   on failure.
 */
static struct otelc_meter *otel_meter_new(void)
{
	struct otelc_meter *retptr = nullptr;

	OTELC_FUNC("");

	if (!OTEL_NULL(retptr = OTEL_CAST_TYPEOF(retptr, OTELC_CALLOC(__func__, __LINE__, 1, sizeof(*retptr))))) {
		retptr->err        = nullptr;
		retptr->scope_name = nullptr;
		retptr->ops        = &otel_meter_ops;
	}

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_meter_create - creates and returns a new meter instance
 *
 * SYNOPSIS
 *   struct otelc_meter *otelc_meter_create(char **err)
 *
 * ARGUMENTS
 *   err - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Allocates and initializes a new meter instance by calling
 *   otel_meter_new().  On failure, an error message may be written
 *   to *err if provided.
 *
 * RETURN VALUE
 *   Returns a pointer to a newly created meter instance on success, or nullptr
 *   on failure.
 */
struct otelc_meter *otelc_meter_create(char **err)
{
	struct otelc_meter *retptr = nullptr;

	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(err));

	if (OTEL_NULL(retptr = otel_meter_new()))
		OTEL_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("meter"));

#ifndef OTELC_USE_STATIC_HANDLE
	if (OTEL_NULL(otel_instrument))
		otel_instrument = new(std::nothrow) struct otel_handle<struct otel_instrument_handle *>(1);
	if (OTEL_NULL(otel_instrument)) {
		otel_meter_destroy(&retptr);

		OTEL_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("meter"));
	}

	if (OTEL_NULL(otel_view))
		otel_view = new(std::nothrow) struct otel_handle<struct otel_view_handle *>(1);
	if (OTEL_NULL(otel_view)) {
		otel_meter_destroy(&retptr);

		OTEL_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("meter"));
	}
#endif /* !OTELC_USE_STATIC_HANDLE */

	OTELC_DBG_METER(OTEL, "meter", retptr);

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_meter_aggr_parse - parses an aggregation type name
 *
 * SYNOPSIS
 *   otelc_metric_aggregation_type_t otelc_meter_aggr_parse(const char *name)
 *
 * ARGUMENTS
 *   name - aggregation type name to look up
 *
 * DESCRIPTION
 *   Performs a case-insensitive lookup of the aggregation type name against the
 *   known OTELC_METRIC_AGGREGATION_DEFINES entries.  If the name is NULL or
 *   does not match any known aggregation type, the function returns an error.
 *
 * RETURN VALUE
 *   Returns the matching otelc_metric_aggregation_type_t value on success,
 *   or OTELC_RET_ERROR cast to the enum type on failure.
 */
otelc_metric_aggregation_type_t otelc_meter_aggr_parse(const char *name)
{
#define OTELC_METRIC_AGGREGATION_DEF(a,b,c)   { c, OTELC_METRIC_AGGREGATION_##a },
	static constexpr struct {
		const char                      *name;
		otelc_metric_aggregation_type_t  type;
	} aggregation_names[] = { OTELC_METRIC_AGGREGATION_DEFINES };
#undef OTELC_METRIC_AGGREGATION_DEF

	if (OTEL_NULL(name))
		return OTEL_CAST_STATIC(otelc_metric_aggregation_type_t, OTELC_RET_ERROR);

	for (size_t i = 0; i < OTELC_TABLESIZE(aggregation_names); i++)
		if (strcasecmp(aggregation_names[i].name, name) == 0)
			return aggregation_names[i].type;

	return OTEL_CAST_STATIC(otelc_metric_aggregation_type_t, OTELC_RET_ERROR);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
