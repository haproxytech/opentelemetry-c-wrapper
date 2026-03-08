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
#ifndef OPENTELEMETRY_C_WRAPPER_METER_H
#define OPENTELEMETRY_C_WRAPPER_METER_H

__CPLUSPLUS_DECL_BEGIN

#define OTELC_DBG_METER(l,h,p)                                          \
	OTELC_DBG_STRUCT(_##l, h, h " %p:{ %p:\"%s\" %p:\"%s\" }", (p), \
	                 (p)->err, OTELC_STR_ARG((p)->err), (p)->scope_name, OTELC_STR_ARG((p)->scope_name))

#define OTELC_DBG_METRIC_OBSERVABLE_CB(l,h,p) \
	OTELC_DBG_STRUCT(_##l, h, h " %p:{ %p %p %p }", (p), (p)->func, (p)->value, (p)->data)

#define OTEL_METRIC_INSTRUMENT_IS_OBSERVABLE(t)   OTELC_IN_RANGE((t), OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64, OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_DOUBLE)

/* <opentelemetry/sdk/metrics/instruments.h> */
#define OTELC_METRIC_INSTRUMENT_DEFINES                                                    \
	OTELC_METRIC_INSTRUMENT_DEF(COUNTER_UINT64,              kCounter                ) \
	OTELC_METRIC_INSTRUMENT_DEF(COUNTER_DOUBLE,              kCounter                ) \
	OTELC_METRIC_INSTRUMENT_DEF(HISTOGRAM_UINT64,            kHistogram              ) \
	OTELC_METRIC_INSTRUMENT_DEF(HISTOGRAM_DOUBLE,            kHistogram              ) \
	OTELC_METRIC_INSTRUMENT_DEF(UDCOUNTER_INT64,             kUpDownCounter          ) \
	OTELC_METRIC_INSTRUMENT_DEF(UDCOUNTER_DOUBLE,            kUpDownCounter          ) \
	OTELC_METRIC_INSTRUMENT_DEF(OBSERVABLE_COUNTER_INT64,    kObservableCounter      ) \
	OTELC_METRIC_INSTRUMENT_DEF(OBSERVABLE_COUNTER_DOUBLE,   kObservableCounter      ) \
	OTELC_METRIC_INSTRUMENT_DEF(OBSERVABLE_GAUGE_INT64,      kObservableGauge        ) \
	OTELC_METRIC_INSTRUMENT_DEF(OBSERVABLE_GAUGE_DOUBLE,     kObservableGauge        ) \
	OTELC_METRIC_INSTRUMENT_DEF(OBSERVABLE_UDCOUNTER_INT64,  kObservableUpDownCounter) \
	OTELC_METRIC_INSTRUMENT_DEF(OBSERVABLE_UDCOUNTER_DOUBLE, kObservableUpDownCounter) \
	OTELC_METRIC_INSTRUMENT_DEF(GAUGE_INT64,                 kGauge                  ) \
	OTELC_METRIC_INSTRUMENT_DEF(GAUGE_DOUBLE,                kGauge                  )

#define OTELC_METRIC_INSTRUMENT_DEF(a,b)   OTELC_METRIC_INSTRUMENT_##a,
typedef enum {
	OTELC_METRIC_INSTRUMENT_DEFINES
} otelc_metric_instrument_t;
#undef OTELC_METRIC_INSTRUMENT_DEF

/* <opentelemetry/sdk/metrics/instruments.h> */
#define OTELC_METRIC_AGGREGATION_DEFINES                                                                       \
	OTELC_METRIC_AGGREGATION_DEF(DROP,                        kDrop                     , "drop"         ) \
	OTELC_METRIC_AGGREGATION_DEF(HISTOGRAM,                   kHistogram                , "histogram"    ) \
	OTELC_METRIC_AGGREGATION_DEF(LAST_VALUE,                  kLastValue                , "last_value"   ) \
	OTELC_METRIC_AGGREGATION_DEF(SUM,                         kSum                      , "sum"          ) \
	OTELC_METRIC_AGGREGATION_DEF(DEFAULT,                     kDefault                  , "default"      ) \
	OTELC_METRIC_AGGREGATION_DEF(BASE2_EXPONENTIAL_HISTOGRAM, kBase2ExponentialHistogram, "exp_histogram")

#define OTELC_METRIC_AGGREGATION_DEF(a,b,c)   OTELC_METRIC_AGGREGATION_##a,
typedef enum {
	OTELC_METRIC_AGGREGATION_DEFINES
} otelc_metric_aggregation_type_t;
#undef OTELC_METRIC_AGGREGATION_DEF

struct otelc_metric_observable_cb;
typedef void (*otelc_metric_observable_instrument_cb_t)(struct otelc_metric_observable_cb *data);

struct otelc_metric_observable_cb {
	otelc_metric_observable_instrument_cb_t  func;
	struct otelc_value                      *value;
	void                                    *data;
};

/***
 * The meter operations vtable.
 */
struct otelc_meter;
struct otelc_meter_ops {
	/***
	 * NAME
	 *   create_instrument - creates a metric instrument
	 *
	 * SYNOPSIS
	 *   int64_t (*create_instrument)(struct otelc_meter *meter, const char *name, const char *desc, const char *unit, otelc_metric_instrument_t type, struct otelc_metric_observable_cb *data)
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
	int64_t (*create_instrument)(struct otelc_meter *meter, const char *name, const char *desc, const char *unit, otelc_metric_instrument_t type, struct otelc_metric_observable_cb *data)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   update_instrument - updates a metric instrument with a value
	 *
	 * SYNOPSIS
	 *   int (*update_instrument)(struct otelc_meter *meter, int idx, const struct otelc_value *value)
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
	 * RETURN VALUE
	 *   Returns the instrument ID on success, or OTELC_RET_ERROR if the instrument
	 *   type is invalid.
	 */
	int (*update_instrument)(struct otelc_meter *meter, int idx, const struct otelc_value *value)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   update_instrument_kv_n - updates a metric instrument with a value and attributes
	 *
	 * SYNOPSIS
	 *   int (*update_instrument_kv_n)(struct otelc_meter *meter, int idx, const struct otelc_value *value, const struct otelc_kv *kv, size_t kv_len)
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
	 * RETURN VALUE
	 *   Returns the instrument ID on success, or OTELC_RET_ERROR if the
	 *   instrument type is invalid.
	 */
	int (*update_instrument_kv_n)(struct otelc_meter *meter, int idx, const struct otelc_value *value, const struct otelc_kv *kv, size_t kv_len)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   add_instrument_callback - registers a callback for an observable instrument
	 *
	 * SYNOPSIS
	 *   int (*add_instrument_callback)(struct otelc_meter *meter, int idx, struct otelc_metric_observable_cb *data)
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
	int (*add_instrument_callback)(struct otelc_meter *meter, int idx, struct otelc_metric_observable_cb *data)
		OTELC_NONNULL(1, 3);

	/***
	 * NAME
	 *   remove_instrument_callback - unregisters a callback for an observable instrument
	 *
	 * SYNOPSIS
	 *   int (*remove_instrument_callback)(struct otelc_meter *meter, int idx, struct otelc_metric_observable_cb *data)
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
	int (*remove_instrument_callback)(struct otelc_meter *meter, int idx, struct otelc_metric_observable_cb *data)
		OTELC_NONNULL(1, 3);

	/***
	 * NAME
	 *   add_view - adds a metrics view to a meter
	 *
	 * SYNOPSIS
	 *   int64_t (*add_view)(struct otelc_meter *meter, const char *view_name, const char *view_desc, const char *instrument_name, const char *instrument_unit, otelc_metric_instrument_t instrument_type, otelc_metric_aggregation_type_t aggregation_type, const double *bounds, size_t bounds_num)
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
	 * RETURN VALUE
	 *   Returns the ID of the added view on success, or OTELC_RET_ERROR on
	 *   failure.
	 */
	int64_t (*add_view)(struct otelc_meter *meter, const char *view_name, const char *view_desc, const char *instrument_name, const char *instrument_unit, otelc_metric_instrument_t instrument_type, otelc_metric_aggregation_type_t aggregation_type, const double *bounds, size_t bounds_num)
		OTELC_NONNULL(1, 2, 3, 4, 5);

	/***
	 * NAME
	 *   get_instrument - retrieves an instrument ID by name and type
	 *
	 * SYNOPSIS
	 *   int64_t (*get_instrument)(struct otelc_meter *meter, const char *name, otelc_metric_instrument_t type)
	 *
	 * ARGUMENTS
	 *   meter - meter instance
	 *   name  - name of the instrument to look up (case-insensitive)
	 *   type  - instrument type to match
	 *
	 * DESCRIPTION
	 *   Searches the internal instrument registry for an instrument with
	 *   the given name and type.  The name comparison is case-insensitive,
	 *   consistent with the OpenTelemetry specification.
	 *
	 * RETURN VALUE
	 *   Returns the instrument ID on success, or OTELC_RET_ERROR if no
	 *   matching instrument is found.
	 */
	int64_t (*get_instrument)(struct otelc_meter *meter, const char *name, otelc_metric_instrument_t type)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   enabled - checks whether the meter is enabled
	 *
	 * SYNOPSIS
	 *   int (*enabled)(struct otelc_meter *meter)
	 *
	 * ARGUMENTS
	 *   meter - meter instance
	 *
	 * DESCRIPTION
	 *   Queries the underlying meter to determine whether it is enabled.
	 *   Callers can use this check to skip expensive metric setup when the
	 *   meter would discard the data anyway.  The OpenTelemetry C++ SDK
	 *   does not yet provide a Meter::Enabled() method, so this function
	 *   currently returns true whenever the meter is valid.
	 *
	 * RETURN VALUE
	 *   Returns true if the meter is enabled, false if it is not,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*enabled)(struct otelc_meter *meter)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   force_flush - forces the export of any buffered metrics
	 *
	 * SYNOPSIS
	 *   int (*force_flush)(struct otelc_meter *meter, const struct timespec *timeout)
	 *
	 * ARGUMENTS
	 *   meter   - meter instance
	 *   timeout - maximum time to wait, or NULL for no limit
	 *
	 * DESCRIPTION
	 *   Forces the meter to export all metrics that have been collected but
	 *   not yet exported.  This is a blocking call that will not return
	 *   until the export is complete.  The optional timeout argument
	 *   limits how long the operation may block.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
	 */
	int (*force_flush)(struct otelc_meter *meter, const struct timespec *timeout)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   shutdown - shuts down the underlying meter provider
	 *
	 * SYNOPSIS
	 *   int (*shutdown)(struct otelc_meter *meter, const struct timespec *timeout)
	 *
	 * ARGUMENTS
	 *   meter   - meter instance
	 *   timeout - maximum time to wait, or NULL for no limit
	 *
	 * DESCRIPTION
	 *   Shuts down the underlying meter provider, flushing any
	 *   buffered metrics and releasing provider resources.  After
	 *   shutdown, no further metrics will be exported.  The optional
	 *   timeout argument limits how long the operation may block.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on
	 *   failure.
	 */
	int (*shutdown)(struct otelc_meter *meter, const struct timespec *timeout)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   start - initializes and starts the meter instance
	 *
	 * SYNOPSIS
	 *   int (*start)(struct otelc_meter *meter)
	 *
	 * ARGUMENTS
	 *   meter - meter instance
	 *
	 * DESCRIPTION
	 *   The meter whose configuration is specified in the OpenTelemetry YAML
	 *   configuration file (set by the previous call to the otelc_init() function)
	 *   is started.  The function initializes the meter in such a way that the
	 *   following components are initialized individually: exporter and finally
	 *   provider.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an
	 *   error.
	 */
	int (*start)(struct otelc_meter *meter)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   destroy - stops the meter and frees all associated memory
	 *
	 * SYNOPSIS
	 *   void (*destroy)(struct otelc_meter **meter)
	 *
	 * ARGUMENTS
	 *   meter - address of a meter instance pointer to be stopped and destroyed
	 *
	 * DESCRIPTION
	 *   Stops the meter and releases all resources and memory associated
	 *   with the meter instance.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	void (*destroy)(struct otelc_meter **meter)
		OTELC_NONNULL_ALL;
};

/***
 * The meter instance data.
 */
struct otelc_meter {
	char                         *err;        /* Character array containing the last library error. */
	char                         *scope_name; /* Meter instrumentation scope name. */
	const struct otelc_meter_ops *ops;        /* Pointer to the operations vtable. */
};


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
 *   Returns a pointer to a newly created meter instance on success, or
 *   nullptr on failure.
 */
struct otelc_meter *otelc_meter_create(char **err);

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
otelc_metric_aggregation_type_t otelc_meter_aggr_parse(const char *name);

__CPLUSPLUS_DECL_END
#endif /* OPENTELEMETRY_C_WRAPPER_METER_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
