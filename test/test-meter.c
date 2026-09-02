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

#define METRICS_RESTART_FILE   "__metrics_restart"


/***
 * NAME
 *   observable_int64_cb - callback for observable int64 instruments
 *
 * SYNOPSIS
 *   static void observable_int64_cb(struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   data - observable callback descriptor
 *
 * DESCRIPTION
 *   A test callback that increments an int64 metric value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void observable_int64_cb(struct otelc_metric_observable_cb *data)
{
	if (_NULL(data) || _NULL(data->value))
		return;
	else if (data->value->u_type != OTELC_VALUE_INT64)
		return;

	data->value->u.value_int64 += 100;
}


/***
 * NAME
 *   observable_double_cb - callback for observable double instruments
 *
 * SYNOPSIS
 *   static void observable_double_cb(struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   data - observable callback descriptor
 *
 * DESCRIPTION
 *   A test callback that increments a double metric value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void observable_double_cb(struct otelc_metric_observable_cb *data)
{
	if (_NULL(data) || _NULL(data->value))
		return;
	else if (data->value->u_type != OTELC_VALUE_DOUBLE)
		return;

	data->value->u.value_double += 1.5;
}


/***
 * NAME
 *   observable_int64_cb_2 - second callback for observable int64 instruments
 *
 * SYNOPSIS
 *   static void observable_int64_cb_2(struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   data - observable callback descriptor
 *
 * DESCRIPTION
 *   A second test callback used to verify add/remove callback operations.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void observable_int64_cb_2(struct otelc_metric_observable_cb *data)
{
	if (_NULL(data) || _NULL(data->value))
		return;
	else if (data->value->u_type != OTELC_VALUE_INT64)
		return;

	data->value->u.value_int64 += 200;
}


/***
 * NAME
 *   observable_int64_fixed_cb - callback reporting a fixed int64 value
 *
 * SYNOPSIS
 *   static void observable_int64_fixed_cb(struct otelc_metric_observable_cb *data)
 *
 * ARGUMENTS
 *   data - observable callback descriptor
 *
 * DESCRIPTION
 *   A test callback that reports a fixed value, so the exported output can be
 *   checked for the value the callback produced.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void observable_int64_fixed_cb(struct otelc_metric_observable_cb *data)
{
	if (_NULL(data) || _NULL(data->value))
		return;
	else if (data->value->u_type != OTELC_VALUE_INT64)
		return;

	data->value->u.value_int64 = 42;
}


/* One descriptor for the callback tests, so the removal names the added one. */
static struct otelc_metric_observable_cb test_cb_extra = { .func = observable_int64_cb_2 };


/***
 * NAME
 *   test_meter_create_destroy - tests meter creation and destruction
 *
 * SYNOPSIS
 *   static void test_meter_create_destroy(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context providing the YAML configuration
 *
 * DESCRIPTION
 *   Verifies that a meter can be created and destroyed without errors.  Also
 *   verifies that the function pointers are properly initialized.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_create_destroy(struct otelc_ctx *ctx)
{
	struct otelc_meter *meter;
	char               *err = NULL;
	int                 retval = TEST_FAIL;

	meter = otelc_meter_create(ctx, &err);
	if (_nNULL(meter)) {
		if (_nNULL(meter->ops))
			retval = TEST_PASS;

		OTELC_OPSR(meter, destroy);

		if (_nNULL(meter))
			retval = TEST_FAIL;
	}

	OTELC_SFREE(err);

	test_report("meter create/destroy", retval);
}


/***
 * NAME
 *   test_meter_create_err_null - tests meter creation with NULL err pointer
 *
 * SYNOPSIS
 *   static void test_meter_create_err_null(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context providing the YAML configuration
 *
 * DESCRIPTION
 *   Verifies that a meter can be created when the err argument is NULL.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_create_err_null(struct otelc_ctx *ctx)
{
	struct otelc_meter *meter;
	int                 retval = TEST_FAIL;

	meter = otelc_meter_create(ctx, NULL);
	if (_nNULL(meter)) {
		OTELC_OPSR(meter, destroy);

		retval = TEST_PASS;
	}

	test_report("meter create with NULL err", retval);
}


/***
 * NAME
 *   test_meter_start - tests meter initialization and startup
 *
 * SYNOPSIS
 *   static void test_meter_start(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance to start
 *
 * DESCRIPTION
 *   Verifies that a meter can be started using the YAML configuration.  After
 *   a successful start, the meter's scope_name should be set.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_start(struct otelc_meter *meter)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(meter, start) == OTELC_RET_OK)
		if (_nNULL(meter->scope_name))
			retval = TEST_PASS;

	test_report("meter start", retval);
}


/***
 * NAME
 *   test_create_counter_uint64 - tests creation of a uint64 counter instrument
 *
 * SYNOPSIS
 *   static void test_create_counter_uint64(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that a uint64 counter instrument can be created.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_counter_uint64(struct otelc_meter *meter, int64_t *id)
{
	int retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_counter_uint64", "test uint64 counter", "items", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create counter_uint64", retval);
}


/***
 * NAME
 *   test_create_counter_double - tests creation of a double counter instrument
 *
 * SYNOPSIS
 *   static void test_create_counter_double(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that a double counter instrument can be created.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_counter_double(struct otelc_meter *meter, int64_t *id)
{
	int retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_counter_double", "test double counter", "items", OTELC_METRIC_INSTRUMENT_COUNTER_DOUBLE, NULL);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create counter_double", retval);
}


/***
 * NAME
 *   test_create_histogram_uint64 - tests creation of a uint64 histogram instrument
 *
 * SYNOPSIS
 *   static void test_create_histogram_uint64(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that a uint64 histogram instrument can be created.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_histogram_uint64(struct otelc_meter *meter, int64_t *id)
{
	int retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_histogram_uint64", "test uint64 histogram", "ms", OTELC_METRIC_INSTRUMENT_HISTOGRAM_UINT64, NULL);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create histogram_uint64", retval);
}


/***
 * NAME
 *   test_create_histogram_double - tests creation of a double histogram instrument
 *
 * SYNOPSIS
 *   static void test_create_histogram_double(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that a double histogram instrument can be created.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_histogram_double(struct otelc_meter *meter, int64_t *id)
{
	int retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_histogram_double", "test double histogram", "ms", OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE, NULL);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create histogram_double", retval);
}


/***
 * NAME
 *   test_create_udcounter_int64 - tests creation of an int64 up-down counter
 *
 * SYNOPSIS
 *   static void test_create_udcounter_int64(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that an int64 up-down counter instrument can be created.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_udcounter_int64(struct otelc_meter *meter, int64_t *id)
{
	int retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_udcounter_int64", "test int64 up-down counter", "items", OTELC_METRIC_INSTRUMENT_UDCOUNTER_INT64, NULL);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create udcounter_int64", retval);
}


/***
 * NAME
 *   test_create_udcounter_double - tests creation of a double up-down counter
 *
 * SYNOPSIS
 *   static void test_create_udcounter_double(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that a double up-down counter instrument can be created.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_udcounter_double(struct otelc_meter *meter, int64_t *id)
{
	int retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_udcounter_double", "test double up-down counter", "items", OTELC_METRIC_INSTRUMENT_UDCOUNTER_DOUBLE, NULL);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create udcounter_double", retval);
}


/***
 * NAME
 *   test_create_observable_counter_int64 - tests creation of an observable int64 counter
 *
 * SYNOPSIS
 *   static void test_create_observable_counter_int64(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that an observable int64 counter instrument can be created
 *   with a callback function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_observable_counter_int64(struct otelc_meter *meter, int64_t *id)
{
	static struct otelc_metric_observable_cb cb_data = { .func = observable_int64_cb };
	int                                      retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_obs_counter_int64", "test observable int64 counter", "items", OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64, &cb_data);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create observable_counter_int64", retval);
}


/***
 * NAME
 *   test_create_observable_counter_double - tests creation of an observable double counter
 *
 * SYNOPSIS
 *   static void test_create_observable_counter_double(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that an observable double counter instrument can be created with
 *   a callback function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_observable_counter_double(struct otelc_meter *meter, int64_t *id)
{
	static struct otelc_metric_observable_cb cb_data = { .func = observable_double_cb };
	int                                      retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_obs_counter_double", "test observable double counter", "items", OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_DOUBLE, &cb_data);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create observable_counter_double", retval);
}


/***
 * NAME
 *   test_create_observable_gauge_int64 - tests creation of an observable int64 gauge
 *
 * SYNOPSIS
 *   static void test_create_observable_gauge_int64(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that an observable int64 gauge instrument can be created with a
 *   callback function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_observable_gauge_int64(struct otelc_meter *meter, int64_t *id)
{
	static struct otelc_metric_observable_cb cb_data = { .func = observable_int64_cb };
	int                                      retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_obs_gauge_int64", "test observable int64 gauge", "items", OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_INT64, &cb_data);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create observable_gauge_int64", retval);
}


/***
 * NAME
 *   test_create_observable_gauge_double - tests creation of an observable double gauge
 *
 * SYNOPSIS
 *   static void test_create_observable_gauge_double(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that an observable double gauge instrument can be created with a
 *   callback function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_observable_gauge_double(struct otelc_meter *meter, int64_t *id)
{
	static struct otelc_metric_observable_cb cb_data = { .func = observable_double_cb };
	int                                      retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_obs_gauge_double", "test observable double gauge", "items", OTELC_METRIC_INSTRUMENT_OBSERVABLE_GAUGE_DOUBLE, &cb_data);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create observable_gauge_double", retval);
}


/***
 * NAME
 *   test_create_observable_udcounter_int64 - tests creation of an observable int64 up-down counter
 *
 * SYNOPSIS
 *   static void test_create_observable_udcounter_int64(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that an observable int64 up-down counter instrument can be created
 *   with a callback function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_observable_udcounter_int64(struct otelc_meter *meter, int64_t *id)
{
	static struct otelc_metric_observable_cb cb_data = { .func = observable_int64_cb };
	int                                      retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_obs_udcounter_int64", "test observable int64 up-down counter", "items", OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_INT64, &cb_data);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create observable_udcounter_int64", retval);
}


/***
 * NAME
 *   test_create_observable_udcounter_double - tests creation of an observable double up-down counter
 *
 * SYNOPSIS
 *   static void test_create_observable_udcounter_double(struct otelc_meter *meter, int64_t *id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - pointer to store the created instrument ID
 *
 * DESCRIPTION
 *   Verifies that an observable double up-down counter instrument can be
 *   created with a callback function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_observable_udcounter_double(struct otelc_meter *meter, int64_t *id)
{
	static struct otelc_metric_observable_cb cb_data = { .func = observable_double_cb };
	int                                      retval = TEST_FAIL;

	*id = OTELC_OPS(meter, create_instrument, "test_obs_udcounter_double", "test observable double up-down counter", "items", OTELC_METRIC_INSTRUMENT_OBSERVABLE_UDCOUNTER_DOUBLE, &cb_data);
	if (*id >= 0)
		retval = TEST_PASS;

	test_report("create observable_udcounter_double", retval);
}


/***
 * NAME
 *   test_create_duplicate_instrument - tests that creating a duplicate returns the same ID
 *
 * SYNOPSIS
 *   static void test_create_duplicate_instrument(struct otelc_meter *meter, int64_t expected_id)
 *
 * ARGUMENTS
 *   meter       - meter instance
 *   expected_id - the ID returned from the first creation of the instrument
 *
 * DESCRIPTION
 *   Verifies that creating an instrument with the same name and type as an
 *   existing one returns the same instrument ID rather than creating a new one.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_duplicate_instrument(struct otelc_meter *meter, int64_t expected_id)
{
	int64_t id;
	int     retval = TEST_FAIL;

	id = OTELC_OPS(meter, create_instrument, "test_counter_uint64", "test uint64 counter", "items", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
	if (id == expected_id)
		retval = TEST_PASS;

	test_report("create duplicate instrument", retval);
}


/***
 * NAME
 *   test_update_counter_uint64 - tests updating a uint64 counter
 *
 * SYNOPSIS
 *   static void test_update_counter_uint64(struct otelc_meter *meter, int64_t id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - instrument ID of the uint64 counter
 *
 * DESCRIPTION
 *   Verifies that a uint64 counter can be updated with a value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_counter_uint64(struct otelc_meter *meter, int64_t id)
{
	const struct otelc_value value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(42) };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument, id, &value) >= 0)
		retval = TEST_PASS;

	test_report("update counter_uint64", retval);
}


/***
 * NAME
 *   test_update_counter_double - tests updating a double counter
 *
 * SYNOPSIS
 *   static void test_update_counter_double(struct otelc_meter *meter, int64_t id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - instrument ID of the double counter
 *
 * DESCRIPTION
 *   Verifies that a double counter can be updated with a value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_counter_double(struct otelc_meter *meter, int64_t id)
{
	const struct otelc_value value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 3.14 };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument, id, &value) >= 0)
		retval = TEST_PASS;

	test_report("update counter_double", retval);
}


/***
 * NAME
 *   test_update_histogram_uint64 - tests recording a uint64 histogram value
 *
 * SYNOPSIS
 *   static void test_update_histogram_uint64(struct otelc_meter *meter, int64_t id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - instrument ID of the uint64 histogram
 *
 * DESCRIPTION
 *   Verifies that a uint64 histogram can record a value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_histogram_uint64(struct otelc_meter *meter, int64_t id)
{
	const struct otelc_value value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(150) };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument, id, &value) >= 0)
		retval = TEST_PASS;

	test_report("update histogram_uint64", retval);
}


/***
 * NAME
 *   test_update_histogram_double - tests recording a double histogram value
 *
 * SYNOPSIS
 *   static void test_update_histogram_double(struct otelc_meter *meter, int64_t id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - instrument ID of the double histogram
 *
 * DESCRIPTION
 *   Verifies that a double histogram can record a value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_histogram_double(struct otelc_meter *meter, int64_t id)
{
	const struct otelc_value value = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 99.9 };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument, id, &value) >= 0)
		retval = TEST_PASS;

	test_report("update histogram_double", retval);
}


/***
 * NAME
 *   test_update_udcounter_int64 - tests updating an int64 up-down counter
 *
 * SYNOPSIS
 *   static void test_update_udcounter_int64(struct otelc_meter *meter, int64_t id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - instrument ID of the int64 up-down counter
 *
 * DESCRIPTION
 *   Verifies that an int64 up-down counter can be updated with positive and
 *   negative values.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_udcounter_int64(struct otelc_meter *meter, int64_t id)
{
	const struct otelc_value value_up   = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(10) };
	const struct otelc_value value_down = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(-3) };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument, id, &value_up) >= 0)
		if (OTELC_OPS(meter, update_instrument, id, &value_down) >= 0)
			retval = TEST_PASS;

	test_report("update udcounter_int64", retval);
}


/***
 * NAME
 *   test_update_udcounter_double - tests updating a double up-down counter
 *
 * SYNOPSIS
 *   static void test_update_udcounter_double(struct otelc_meter *meter, int64_t id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - instrument ID of the double up-down counter
 *
 * DESCRIPTION
 *   Verifies that a double up-down counter can be updated with positive and
 *   negative values.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_udcounter_double(struct otelc_meter *meter, int64_t id)
{
	const struct otelc_value value_up   = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 5.5  };
	const struct otelc_value value_down = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = -2.0 };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument, id, &value_up) >= 0)
		if (OTELC_OPS(meter, update_instrument, id, &value_down) >= 0)
			retval = TEST_PASS;

	test_report("update udcounter_double", retval);
}


/***
 * NAME
 *   test_update_counter_uint64_with_int64 - tests updating a uint64 counter
 *   with an int64 value
 *
 * SYNOPSIS
 *   static void test_update_counter_uint64_with_int64(struct otelc_meter *meter, int64_t id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - instrument ID of the uint64 counter
 *
 * DESCRIPTION
 *   Verifies that a uint64 counter can be updated with a non-negative int64
 *   value.  The value is automatically converted to uint64.  Both a positive
 *   value and zero are tested.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_counter_uint64_with_int64(struct otelc_meter *meter, int64_t id)
{
	const struct otelc_value value      = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(42) };
	const struct otelc_value value_zero = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(0)  };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument, id, &value) >= 0)
		if (OTELC_OPS(meter, update_instrument, id, &value_zero) >= 0)
			retval = TEST_PASS;

	test_report("update counter_uint64 with int64", retval);
}


/***
 * NAME
 *   test_update_histogram_uint64_with_int64 - tests recording a uint64
 *   histogram value with an int64 value
 *
 * SYNOPSIS
 *   static void test_update_histogram_uint64_with_int64(struct otelc_meter *meter, int64_t id)
 *
 * ARGUMENTS
 *   meter - meter instance
 *   id    - instrument ID of the uint64 histogram
 *
 * DESCRIPTION
 *   Verifies that a uint64 histogram can record a non-negative int64 value.
 *   The value is automatically converted to uint64.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_histogram_uint64_with_int64(struct otelc_meter *meter, int64_t id)
{
	const struct otelc_value value = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(150) };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument, id, &value) >= 0)
		retval = TEST_PASS;

	test_report("update histogram_uint64 with int64", retval);
}


/***
 * NAME
 *   test_update_uint64_with_negative_int64 - tests that negative int64
 *   values are rejected for uint64 instruments
 *
 * SYNOPSIS
 *   static void test_update_uint64_with_negative_int64(struct otelc_meter *meter, int64_t counter_id, int64_t histogram_id)
 *
 * ARGUMENTS
 *   meter        - meter instance
 *   counter_id   - instrument ID of a uint64 counter
 *   histogram_id - instrument ID of a uint64 histogram
 *
 * DESCRIPTION
 *   Verifies that updating a uint64 counter or histogram with a negative
 *   int64 value is rejected with an error.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_uint64_with_negative_int64(struct otelc_meter *meter, int64_t counter_id, int64_t histogram_id)
{
	const struct otelc_value value = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(-1) };
	int                      rc, retval = TEST_PASS;

	rc = OTELC_OPS(meter, update_instrument, (int)counter_id, &value);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	rc = OTELC_OPS(meter, update_instrument, (int)histogram_id, &value);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	test_report("update uint64 with negative int64", retval);
}


/***
 * NAME
 *   test_update_instrument_uint64_with_int64 - tests updating uint64
 *   instruments with int64 values and attributes
 *
 * SYNOPSIS
 *   static void test_update_instrument_uint64_with_int64(struct otelc_meter *meter, int64_t counter_id, int64_t histogram_id)
 *
 * ARGUMENTS
 *   meter        - meter instance
 *   counter_id   - instrument ID of a uint64 counter
 *   histogram_id - instrument ID of a uint64 histogram
 *
 * DESCRIPTION
 *   Verifies that uint64 instruments can be updated with non-negative int64
 *   values together with key-value attribute pairs.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_instrument_uint64_with_int64(struct otelc_meter *meter, int64_t counter_id, int64_t histogram_id)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"region", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "us-west-2" } },
	};
	const struct otelc_value counter_val   = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(7)   };
	const struct otelc_value histogram_val = { .u_type = OTELC_VALUE_INT64, .u.value_int64 = INT64_C(150) };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument_kv_n, counter_id, &counter_val, attr, OTELC_TABLESIZE(attr)) >= 0)
		if (OTELC_OPS(meter, update_instrument_kv_n, histogram_id, &histogram_val, attr, OTELC_TABLESIZE(attr)) >= 0)
			retval = TEST_PASS;

	test_report("update uint64 instrument with int64 kv", retval);
}


/***
 * NAME
 *   test_update_instrument - tests updating an instrument with attributes
 *
 * SYNOPSIS
 *   static void test_update_instrument(struct otelc_meter *meter, int64_t counter_id, int64_t histogram_id)
 *
 * ARGUMENTS
 *   meter        - meter instance
 *   counter_id   - instrument ID of a counter
 *   histogram_id - instrument ID of a histogram
 *
 * DESCRIPTION
 *   Verifies that instruments can be updated with both a value and key-value
 *   attribute pairs.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_instrument(struct otelc_meter *meter, int64_t counter_id, int64_t histogram_id)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"region",      .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "us-east-1" } },
		{ .key = (char *)"environment", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "staging"   } },
		{ .key = (char *)"priority",    .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(1)  } },
	};
	const struct otelc_value counter_val   = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(7) };
	const struct otelc_value histogram_val = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 42.5        };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument_kv_n, counter_id, &counter_val, attr, OTELC_TABLESIZE(attr)) >= 0)
		if (OTELC_OPS(meter, update_instrument_kv_n, histogram_id, &histogram_val, attr, 2) >= 0)
			retval = TEST_PASS;

	test_report("update instrument with kv attributes", retval);
}


/***
 * NAME
 *   test_update_instrument_udcounter - tests updating up-down counters with attributes
 *
 * SYNOPSIS
 *   static void test_update_instrument_udcounter(struct otelc_meter *meter, int64_t udcounter_i64_id, int64_t udcounter_dbl_id)
 *
 * ARGUMENTS
 *   meter            - meter instance
 *   udcounter_i64_id - instrument ID of an int64 up-down counter
 *   udcounter_dbl_id - instrument ID of a double up-down counter
 *
 * DESCRIPTION
 *   Verifies that up-down counters can be updated with both a value and
 *   key-value attribute pairs.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_instrument_udcounter(struct otelc_meter *meter, int64_t udcounter_i64_id, int64_t udcounter_dbl_id)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"host",   .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "server-01" } },
		{ .key = (char *)"active", .value = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(1) }  },
	};
	const struct otelc_value val_i64 = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(5) };
	const struct otelc_value val_dbl = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = -1.5       };
	int                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, update_instrument_kv_n, udcounter_i64_id, &val_i64, attr, OTELC_TABLESIZE(attr)) >= 0)
		if (OTELC_OPS(meter, update_instrument_kv_n, udcounter_dbl_id, &val_dbl, attr, OTELC_TABLESIZE(attr)) >= 0)
			retval = TEST_PASS;

	test_report("update udcounter with kv attributes", retval);
}


/***
 * NAME
 *   test_add_view_histogram - tests adding a histogram view with bucket boundaries
 *
 * SYNOPSIS
 *   static void test_add_view_histogram(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Verifies that a histogram view with custom bucket boundaries can be added
 *   to the meter.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_add_view_histogram(struct otelc_meter *meter)
{
	static const double bounds[] = { 1.0, 5.0, 10.0, 50.0, 100.0, 500.0, 1000.0 };
	int64_t             view_id;
	int                 retval = TEST_FAIL;

	view_id = OTELC_OPS(meter, add_view, "test_histogram_double", "", "test_histogram_double", "", OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE, OTELC_METRIC_AGGREGATION_HISTOGRAM, bounds, OTELC_TABLESIZE(bounds));
	if (view_id >= 0)
		retval = TEST_PASS;

	test_report("add_view histogram with bounds", retval);
}


/***
 * NAME
 *   test_add_view_default - tests adding a view with default aggregation
 *
 * SYNOPSIS
 *   static void test_add_view_default(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Verifies that a view with default aggregation type can be added to the
 *   meter.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_add_view_default(struct otelc_meter *meter)
{
	int64_t view_id;
	int     retval = TEST_FAIL;

	view_id = OTELC_OPS(meter, add_view, "test_counter_uint64", "", "test_counter_uint64", "", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, OTELC_METRIC_AGGREGATION_DEFAULT, NULL, 0);
	if (view_id >= 0)
		retval = TEST_PASS;

	test_report("add_view default aggregation", retval);
}


/***
 * NAME
 *   test_add_view_duplicate - tests that adding a duplicate view returns the existing ID
 *
 * SYNOPSIS
 *   static void test_add_view_duplicate(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Verifies that adding a view with the same name as an existing view returns
 *   the existing view ID rather than creating a new one.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_add_view_duplicate(struct otelc_meter *meter)
{
	int64_t id1, id2;
	int     retval = TEST_FAIL;

	id1 = OTELC_OPS(meter, add_view, "test_dup_view", "", "test_dup_view", "", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, OTELC_METRIC_AGGREGATION_SUM, NULL, 0);
	id2 = OTELC_OPS(meter, add_view, "test_dup_view", "", "test_dup_view", "", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, OTELC_METRIC_AGGREGATION_SUM, NULL, 0);

	if ((id1 >= 0) && (id1 == id2))
		retval = TEST_PASS;

	test_report("add_view duplicate returns same ID", retval);
}


/***
 * NAME
 *   test_add_instrument_callback - tests registering an additional callback
 *
 * SYNOPSIS
 *   static void test_add_instrument_callback(struct otelc_meter *meter, int64_t obs_id)
 *
 * ARGUMENTS
 *   meter  - meter instance
 *   obs_id - instrument ID of an observable instrument
 *
 * DESCRIPTION
 *   Verifies that an additional callback can be registered on an existing
 *   observable instrument.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_add_instrument_callback(struct otelc_meter *meter, int64_t obs_id)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(meter, add_instrument_callback, obs_id, &test_cb_extra) == OTELC_RET_OK)
		retval = TEST_PASS;

	test_report("add_instrument_callback", retval);
}


/***
 * NAME
 *   test_remove_instrument_callback - tests unregistering a callback
 *
 * SYNOPSIS
 *   static void test_remove_instrument_callback(struct otelc_meter *meter, int64_t obs_id)
 *
 * ARGUMENTS
 *   meter  - meter instance
 *   obs_id - instrument ID of an observable instrument
 *
 * DESCRIPTION
 *   Verifies that the descriptor registered by the add test can be removed
 *   from the observable instrument again.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_remove_instrument_callback(struct otelc_meter *meter, int64_t obs_id)
{
	int retval = TEST_FAIL;

	if (OTELC_OPS(meter, remove_instrument_callback, obs_id, &test_cb_extra) == OTELC_RET_OK)
		retval = TEST_PASS;

	test_report("remove_instrument_callback", retval);
}


/***
 * NAME
 *   test_instrument_invalid_handle - tests operations with a non-existent instrument ID
 *
 * SYNOPSIS
 *   static void test_instrument_invalid_handle(struct otelc_meter *meter, int64_t valid_id)
 *
 * ARGUMENTS
 *   meter    - meter instance
 *   valid_id - a known-good instrument ID used for the final sanity check
 *
 * DESCRIPTION
 *   Passes a large bogus instrument ID to update_instrument(),
 *   update_instrument_kv_n(), add_instrument_callback(), and
 *   remove_instrument_callback().  All of these must fail because the ID does
 *   not exist in the internal handle map.  Afterwards, the valid instrument is
 *   updated to confirm the meter is still functional.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_instrument_invalid_handle(struct otelc_meter *meter, int64_t valid_id)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"inv_attr", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "invalid" } },
	};
	static struct otelc_metric_observable_cb cb_data = { .func = observable_int64_cb };
	const struct otelc_value                 value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(1) };
	const int                                bogus_id = INT32_MAX;
	int                                      rc, retval = TEST_PASS;

	/* Try to update an instrument with a bogus ID. */
	rc = OTELC_OPS(meter, update_instrument, bogus_id, &value);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* Try to update with kv attributes. */
	rc = OTELC_OPS(meter, update_instrument_kv_n, bogus_id, &value, attr, OTELC_TABLESIZE(attr));
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* Try to add a callback. */
	rc = OTELC_OPS(meter, add_instrument_callback, bogus_id, &cb_data);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* Try to remove a callback. */
	rc = OTELC_OPS(meter, remove_instrument_callback, bogus_id, &cb_data);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* Confirm the valid instrument still works. */
	rc = OTELC_OPS(meter, update_instrument, (int)valid_id, &value);
	if (rc < 0)
		retval = TEST_FAIL;

	test_report("instrument invalid handle", retval);
}


/***
 * NAME
 *   test_get_instrument - tests instrument retrieval by name and type
 *
 * SYNOPSIS
 *   static void test_get_instrument(struct otelc_meter *meter, int64_t expected_id)
 *
 * ARGUMENTS
 *   meter       - meter instance
 *   expected_id - the ID returned when the instrument was first created
 *
 * DESCRIPTION
 *   Verifies that an existing instrument can be looked up by name and type
 *   using the get_instrument() interface.  The returned ID must match the
 *   original creation ID.  Also verifies that a lookup with a non-existent
 *   name returns OTELC_RET_ERROR.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_get_instrument(struct otelc_meter *meter, int64_t expected_id)
{
	int64_t found_id;
	int     retval = TEST_FAIL;

	found_id = OTELC_OPS(meter, get_instrument, "test_counter_uint64", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64);
	if (found_id == expected_id) {
		/* Verify that a non-existent name returns error. */
		if (OTELC_OPS(meter, get_instrument, "no_such_instrument", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64) == OTELC_RET_ERROR)
			retval = TEST_PASS;
	}

	test_report("get_instrument", retval);
}


/***
 * NAME
 *   test_meter_enabled - tests whether the meter reports enabled state
 *
 * SYNOPSIS
 *   static void test_meter_enabled(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Verifies that the enabled() function reports a started meter whose
 *   wrapper flag has never been cleared as enabled, rather than as disabled
 *   or as an error.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_enabled(struct otelc_meter *meter)
{
	int rc, retval = TEST_PASS;

	rc = OTELC_OPS(meter, enabled);
	if (rc != true)
		retval = TEST_FAIL;

	test_report("meter enabled", retval);
}


/***
 * NAME
 *   test_update_value_type_mismatch - tests that mismatched value types
 *   are rejected
 *
 * SYNOPSIS
 *   static void test_update_value_type_mismatch(struct otelc_meter *meter, int64_t counter_u64_id, int64_t counter_dbl_id, int64_t udcounter_i64_id)
 *
 * ARGUMENTS
 *   meter            - meter instance
 *   counter_u64_id   - instrument ID of a uint64 counter
 *   counter_dbl_id   - instrument ID of a double counter
 *   udcounter_i64_id - instrument ID of an int64 up-down counter
 *
 * DESCRIPTION
 *   Verifies that updating an instrument with a value whose type does not
 *   match the instrument's expected type is rejected with an error.  Tests
 *   a double value against a uint64 counter, a uint64 value against a
 *   double counter, and a double value against an int64 up-down counter.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_value_type_mismatch(struct otelc_meter *meter, int64_t counter_u64_id, int64_t counter_dbl_id, int64_t udcounter_i64_id)
{
	const struct otelc_value val_double = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 1.0             };
	const struct otelc_value val_uint64 = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(1)    };
	const struct otelc_value val_string = { .u_type = OTELC_VALUE_STRING, .u.value_string = "not a number" };
	int                      rc, retval = TEST_PASS;

	/* Double value -> uint64 counter: must fail. */
	rc = OTELC_OPS(meter, update_instrument, (int)counter_u64_id, &val_double);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* Uint64 value -> double counter: must fail. */
	rc = OTELC_OPS(meter, update_instrument, (int)counter_dbl_id, &val_uint64);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* Double value -> int64 up-down counter: must fail. */
	rc = OTELC_OPS(meter, update_instrument, (int)udcounter_i64_id, &val_double);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* String value -> uint64 counter: must fail. */
	rc = OTELC_OPS(meter, update_instrument, (int)counter_u64_id, &val_string);
	if (rc != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	test_report("update value type mismatch", retval);
}


/***
 * NAME
 *   test_gauge_instruments - tests gauge instrument creation and update
 *
 * SYNOPSIS
 *   static int test_gauge_instruments(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Verifies that int64 and double gauge instruments can be created and
 *   updated.  Gauge instruments require OpenTelemetry ABI version 2 or
 *   later.  On older ABI versions the creation is expected to fail; this
 *   test passes in either case, verifying only that the API behaves
 *   correctly for the running ABI.
 *
 * RETURN VALUE
 *   Returns the number of gauge instruments created (0 or 2).
 */
static int test_gauge_instruments(struct otelc_meter *meter)
{
	const struct otelc_value val_i64 = { .u_type = OTELC_VALUE_INT64,  .u.value_int64  = INT64_C(42) };
	const struct otelc_value val_dbl = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 3.14        };
	int64_t                  id_i64, id_dbl;
	int                      retval = TEST_PASS;

	id_i64 = OTELC_OPS(meter, create_instrument, "test_gauge_int64", "test int64 gauge", "items", OTELC_METRIC_INSTRUMENT_GAUGE_INT64, NULL);
	id_dbl = OTELC_OPS(meter, create_instrument, "test_gauge_double", "test double gauge", "items", OTELC_METRIC_INSTRUMENT_GAUGE_DOUBLE, NULL);

	if ((id_i64 >= 0) && (id_dbl >= 0)) {
		/* ABI >= 2: creation succeeded, verify update works. */
		if (OTELC_OPS(meter, update_instrument, (int)id_i64, &val_i64) < 0)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, update_instrument, (int)id_dbl, &val_dbl) < 0)
			retval = TEST_FAIL;
	} else if ((id_i64 == OTELC_RET_ERROR) && (id_dbl == OTELC_RET_ERROR)) {
		/* ABI < 2 rejects the type; another message is a regression. */
		if (_NULL(meter->err) || _NULL(strstr(meter->err, "instrument type")))
			retval = TEST_FAIL;
	} else {
		/* Inconsistent result. */
		retval = TEST_FAIL;
	}

	test_report("gauge instruments", retval);

	return (id_i64 >= 0) ? 2 : 0;
}


/***
 * NAME
 *   test_force_flush - tests forced export of buffered metrics
 *
 * SYNOPSIS
 *   static void test_force_flush(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Verifies that the force_flush() function completes without errors.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_force_flush(struct otelc_meter *meter)
{
	const struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
	int                   retval = TEST_FAIL;

	if ((OTELC_OPS(meter, force_flush, NULL) == OTELC_RET_OK) &&
	    (OTELC_OPS(meter, force_flush, &timeout) == OTELC_RET_OK))
		retval = TEST_PASS;

	test_report("force_flush", retval);
}


/***
 * NAME
 *   test_shutdown - tests shutdown of the meter provider
 *
 * SYNOPSIS
 *   static void test_shutdown(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Verifies that the shutdown() function completes without errors with an
 *   unlimited timeout (NULL), and that a repeated call with a specific timeout
 *   returns; the result of the repeated call belongs to the SDK and is not
 *   checked.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_shutdown(struct otelc_meter *meter)
{
	const struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
	int                   retval = TEST_FAIL;

	if (OTELC_OPS(meter, shutdown, NULL) == OTELC_RET_OK)
		retval = TEST_PASS;

	/* A repeated shutdown's outcome is the SDK's; it only has to return. */
	(void)OTELC_OPS(meter, shutdown, &timeout);

	test_report("shutdown", retval);
}


/***
 * NAME
 *   test_meter_create_null_ctx - tests meter creation without a context
 *
 * SYNOPSIS
 *   static void test_meter_create_null_ctx(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that otelc_meter_create() refuses a NULL context and reports the
 *   refusal through the err argument.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_create_null_ctx(void)
{
	struct otelc_meter *meter;
	char               *err = NULL;
	int                 retval = TEST_FAIL;

	meter = otelc_meter_create(NULL, &err);
	if (_NULL(meter) && _nNULL(err))
		retval = TEST_PASS;

	if (_nNULL(meter))
		OTELC_OPSR(meter, destroy);
	OTELC_SFREE(err);

	test_report("meter create with NULL context", retval);
}


/***
 * NAME
 *   test_meter_unstarted - tests the operations of a meter never started
 *
 * SYNOPSIS
 *   static void test_meter_unstarted(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context providing the YAML configuration
 *
 * DESCRIPTION
 *   Verifies that a meter that was created but never started refuses to
 *   create instruments and views and reports enabled(), force_flush() and
 *   shutdown() as errors, leaving a message behind, and that it can still be
 *   destroyed.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_unstarted(struct otelc_ctx *ctx)
{
	const otelc_metric_instrument_t       type = OTELC_METRIC_INSTRUMENT_COUNTER_UINT64;
	const otelc_metric_aggregation_type_t aggr = OTELC_METRIC_AGGREGATION_SUM;
	struct otelc_meter                   *meter;
	char                                 *err = NULL;
	int                                   retval = TEST_PASS;

	meter = otelc_meter_create(ctx, &err);
	if (_NULL(meter)) {
		retval = TEST_FAIL;
	} else {
		if (OTELC_OPS(meter, create_instrument, "unstarted_counter", "", "", type, NULL) != OTELC_RET_ERROR)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, add_view, "unstarted_view", "", "unstarted_counter", "", type, aggr, NULL, 0) != OTELC_RET_ERROR)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, enabled) != OTELC_RET_ERROR)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, force_flush, NULL) != OTELC_RET_ERROR)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, shutdown, NULL) != OTELC_RET_ERROR)
			retval = TEST_FAIL;
		if (_NULL(meter->err))
			retval = TEST_FAIL;

		OTELC_OPSR(meter, destroy);
	}

	OTELC_SFREE(err);

	test_report("meter operations before start", retval);
}


/***
 * NAME
 *   test_create_instrument_invalid_args - tests the argument checks of instrument creation
 *
 * SYNOPSIS
 *   static void test_create_instrument_invalid_args(struct otelc_meter *meter, int64_t obs_id)
 *
 * ARGUMENTS
 *   meter  - meter instance
 *   obs_id - instrument ID of an existing observable instrument
 *
 * DESCRIPTION
 *   Verifies that create_instrument() refuses an empty name, an observable
 *   type without a descriptor, a synchronous type with a descriptor and a type
 *   outside the enum, each with a message, and that an existing observable
 *   instrument is returned again even when the descriptor is missing.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_create_instrument_invalid_args(struct otelc_meter *meter, int64_t obs_id)
{
	static struct otelc_metric_observable_cb cb_data = { .func = observable_int64_cb };
	const otelc_metric_instrument_t          sync_type = OTELC_METRIC_INSTRUMENT_COUNTER_UINT64;
	const otelc_metric_instrument_t          obs_type = OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64;
	const otelc_metric_instrument_t          bad_type = (otelc_metric_instrument_t)99;
	int                                      retval = TEST_PASS;

	/* An empty name fails the SDK name validation. */
	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, create_instrument, "", "", "", sync_type, NULL) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, create_instrument, "new_obs_no_cb", "", "", obs_type, NULL) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, create_instrument, "new_sync_cb", "", "", sync_type, &cb_data) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, create_instrument, "new_bad_type", "", "", bad_type, NULL) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	/* An existing observable instrument ignores the missing descriptor. */
	if (OTELC_OPS(meter, create_instrument, "test_obs_counter_int64", "", "", obs_type, NULL) != obs_id)
		retval = TEST_FAIL;

	test_report("create_instrument with invalid arguments", retval);
}


/***
 * NAME
 *   test_add_view_invalid_args - tests the argument checks of add_view
 *
 * SYNOPSIS
 *   static void test_add_view_invalid_args(struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance
 *
 * DESCRIPTION
 *   Verifies that add_view() refuses an instrument type and an aggregation type
 *   outside their enums, a boundary count without an array, an array without
 *   entries, and boundaries for an instrument type that is not a histogram,
 *   each with a message.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_add_view_invalid_args(struct otelc_meter *meter)
{
	static const double                   bounds[] = { 1.0, 2.0 };
	const otelc_metric_instrument_t       hist = OTELC_METRIC_INSTRUMENT_HISTOGRAM_DOUBLE;
	const otelc_metric_instrument_t       cnt = OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, bad_type = (otelc_metric_instrument_t)99;
	const otelc_metric_aggregation_type_t sum = OTELC_METRIC_AGGREGATION_SUM, bad_aggr = (otelc_metric_aggregation_type_t)99;
	int                                   retval = TEST_PASS;

	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, add_view, "bv_type", "", "bv_src", "", bad_type, sum, NULL, 0) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, add_view, "bv_aggr", "", "bv_src", "", cnt, bad_aggr, NULL, 0) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	/* Boundaries announced without an array. */
	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, add_view, "bv_bounds", "", "bv_src", "", hist, sum, NULL, 3) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	/* An array without entries. */
	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, add_view, "bv_empty", "", "bv_src", "", hist, sum, bounds, 0) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	/* Boundaries for an instrument type that is not a histogram. */
	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, add_view, "bv_kind", "", "bv_src", "", cnt, sum, bounds, 2) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;

	test_report("add_view with invalid arguments", retval);
}


/***
 * NAME
 *   test_update_invalid_args - tests the argument checks of the update operations
 *
 * SYNOPSIS
 *   static void test_update_invalid_args(struct otelc_meter *meter, int64_t counter_id, int64_t obs_id)
 *
 * ARGUMENTS
 *   meter      - meter instance
 *   counter_id - instrument ID of a uint64 counter
 *   obs_id     - instrument ID of an observable instrument
 *
 * DESCRIPTION
 *   Verifies that both update operations refuse a missing value, that the
 *   attribute variant applies the same type check as the plain one and records
 *   without attributes like the plain one, and that an observable instrument
 *   records nothing while reporting its ID.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_update_invalid_args(struct otelc_meter *meter, int64_t counter_id, int64_t obs_id)
{
	static const struct otelc_kv attr[] = {
		{ .key = (char *)"kv_attr", .value = { .u_type = OTELC_VALUE_STRING, .u.value_string = "value" } },
	};
	const struct otelc_value     val_uint64 = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(1) };
	const struct otelc_value     val_double = { .u_type = OTELC_VALUE_DOUBLE, .u.value_double = 1.0 };
	int                          retval = TEST_PASS;

	OTELC_SFREE_CLEAR(meter->err);
	if ((OTELC_OPS(meter, update_instrument, (int)counter_id, NULL) != OTELC_RET_ERROR) || _NULL(meter->err))
		retval = TEST_FAIL;
	if (OTELC_OPS(meter, update_instrument_kv_n, (int)counter_id, NULL, attr, OTELC_TABLESIZE(attr)) != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* A type mismatch on the attribute variant. */
	if (OTELC_OPS(meter, update_instrument_kv_n, (int)counter_id, &val_double, attr, OTELC_TABLESIZE(attr)) != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* The attribute variant with no attributes acts like the plain one. */
	if (OTELC_OPS(meter, update_instrument_kv_n, (int)counter_id, &val_uint64, NULL, 0) != (int)counter_id)
		retval = TEST_FAIL;

	/* An observable instrument records nothing and reports its ID. */
	if (OTELC_OPS(meter, update_instrument, (int)obs_id, &val_uint64) != (int)obs_id)
		retval = TEST_FAIL;
	if (OTELC_OPS(meter, update_instrument_kv_n, (int)obs_id, &val_uint64, attr, OTELC_TABLESIZE(attr)) != (int)obs_id)
		retval = TEST_FAIL;

	test_report("update with invalid arguments", retval);
}


/***
 * NAME
 *   test_meter_set_enabled - tests the runtime switch of the meter
 *
 * SYNOPSIS
 *   static void test_meter_set_enabled(struct otelc_meter *meter, int64_t counter_id)
 *
 * ARGUMENTS
 *   meter      - meter instance
 *   counter_id - instrument ID of an existing uint64 counter
 *
 * DESCRIPTION
 *   Verifies that clearing the wrapper flag with set_enabled() makes enabled()
 *   report false and blocks the creation of instruments and views while an
 *   existing instrument keeps recording, and that setting the flag again
 *   restores instrument creation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_set_enabled(struct otelc_meter *meter, int64_t counter_id)
{
	const struct otelc_value              value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(1) };
	const otelc_metric_instrument_t       type = OTELC_METRIC_INSTRUMENT_COUNTER_UINT64;
	const otelc_metric_aggregation_type_t aggr = OTELC_METRIC_AGGREGATION_SUM;
	int                                   retval = TEST_PASS;

	if (OTELC_OPS(meter, set_enabled, false) != OTELC_RET_OK)
		retval = TEST_FAIL;
	if (OTELC_OPS(meter, enabled) != false)
		retval = TEST_FAIL;

	if (OTELC_OPS(meter, create_instrument, "disabled_counter", "", "", type, NULL) != OTELC_RET_ERROR)
		retval = TEST_FAIL;
	if (OTELC_OPS(meter, add_view, "disabled_view", "", "disabled_counter", "", type, aggr, NULL, 0) != OTELC_RET_ERROR)
		retval = TEST_FAIL;

	/* Existing instruments keep recording. */
	if (OTELC_OPS(meter, update_instrument, (int)counter_id, &value) != (int)counter_id)
		retval = TEST_FAIL;

	if (OTELC_OPS(meter, set_enabled, true) != OTELC_RET_OK)
		retval = TEST_FAIL;
	if (OTELC_OPS(meter, enabled) != true)
		retval = TEST_FAIL;
	if (OTELC_OPS(meter, create_instrument, "enabled_again_counter", "", "", type, NULL) < 0)
		retval = TEST_FAIL;

	test_report("meter set_enabled", retval);
}


/***
 * NAME
 *   test_aggr_parse - tests the aggregation name parser
 *
 * SYNOPSIS
 *   static void test_aggr_parse(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that otelc_meter_aggr_parse() maps the known aggregation names to
 *   their enum values regardless of letter case, and that an unknown name and
 *   a NULL name are reported as errors.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_aggr_parse(void)
{
	const otelc_metric_aggregation_type_t error = (otelc_metric_aggregation_type_t)OTELC_RET_ERROR;
	int                                   retval = TEST_PASS;

	if (otelc_meter_aggr_parse("sum") != OTELC_METRIC_AGGREGATION_SUM)
		retval = TEST_FAIL;
	if (otelc_meter_aggr_parse("HISTOGRAM") != OTELC_METRIC_AGGREGATION_HISTOGRAM)
		retval = TEST_FAIL;
	if (otelc_meter_aggr_parse("exp_histogram") != OTELC_METRIC_AGGREGATION_BASE2_EXPONENTIAL_HISTOGRAM)
		retval = TEST_FAIL;
	if (otelc_meter_aggr_parse("drop") != OTELC_METRIC_AGGREGATION_DROP)
		retval = TEST_FAIL;
	if (otelc_meter_aggr_parse("no_such_aggregation") != error)
		retval = TEST_FAIL;
	if (otelc_meter_aggr_parse(NULL) != error)
		retval = TEST_FAIL;

	test_report("otelc_meter_aggr_parse", retval);
}


/***
 * NAME
 *   test_meter_set_flush_timeout - tests the destroy-time flush budget
 *
 * SYNOPSIS
 *   static void test_meter_set_flush_timeout(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context whose configuration allows a second meter
 *
 * DESCRIPTION
 *   Starts a meter of its own and verifies that the budget starts at the
 *   library default, that set_flush_timeout() rejects a negative value and a
 *   value above OTELC_FLUSH_TIMEOUT_MS_MAX with a message, and that it stores
 *   the maximum and zero.  The meter is destroyed with a zero budget after a
 *   value was recorded, which takes the exporter shutdown path of destroy.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_set_flush_timeout(struct otelc_ctx *ctx)
{
	const struct otelc_value        value = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(3) };
	const otelc_metric_instrument_t type = OTELC_METRIC_INSTRUMENT_COUNTER_UINT64;
	struct otelc_meter             *meter;
	char                           *err = NULL;
	int64_t                         id;
	int                             retval = TEST_PASS;

	meter = otelc_meter_create(ctx, &err);
	if (_NULL(meter) || (OTELC_OPS(meter, start) != OTELC_RET_OK)) {
		retval = TEST_FAIL;
	} else {
		if (meter->flush_timeout != OTELC_FLUSH_TIMEOUT_MS)
			retval = TEST_FAIL;
		if ((OTELC_OPS(meter, set_flush_timeout, -1) != OTELC_RET_ERROR) || _NULL(meter->err))
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, set_flush_timeout, OTELC_FLUSH_TIMEOUT_MS_MAX + 1) != OTELC_RET_ERROR)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, set_flush_timeout, OTELC_FLUSH_TIMEOUT_MS_MAX) != OTELC_RET_OK)
			retval = TEST_FAIL;
		if (meter->flush_timeout != OTELC_FLUSH_TIMEOUT_MS_MAX)
			retval = TEST_FAIL;
		if ((OTELC_OPS(meter, set_flush_timeout, 0) != OTELC_RET_OK) || (meter->flush_timeout != 0))
			retval = TEST_FAIL;

		/* A recorded value gives the zero-budget destroy work to drop. */
		id = OTELC_OPS(meter, create_instrument, "zero_budget_counter", "", "", type, NULL);
		if ((id < 0) || (OTELC_OPS(meter, update_instrument, (int)id, &value) != (int)id))
			retval = TEST_FAIL;
	}

	if (_nNULL(meter))
		OTELC_OPSR(meter, destroy);
	OTELC_SFREE(err);

	test_report("meter set_flush_timeout", retval);
}


/***
 * NAME
 *   test_meter_restart_and_export - tests a repeated start and the exported values
 *
 * SYNOPSIS
 *   static void test_meter_restart_and_export(struct otelc_ctx *ctx)
 *
 * ARGUMENTS
 *   ctx - library context whose configuration writes to a file of its own
 *
 * DESCRIPTION
 *   Starts a meter of its own, adds a view that renames an instrument which
 *   does not exist yet, starts the meter again and then creates a plain
 *   counter, the renamed counter and an observable counter with a fixed
 *   callback value.  After a flush and the destroy the exporter file is read
 *   back: the plain counter must carry its total, the renamed counter must
 *   appear under the view name, which shows that the view survived the
 *   restart, and the observable counter must carry the callback value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_restart_and_export(struct otelc_ctx *ctx)
{
	static struct otelc_metric_observable_cb cb_fixed = { .func = observable_int64_fixed_cb };
	const struct otelc_value                 val_7 = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(7) };
	const struct otelc_value                 val_5 = { .u_type = OTELC_VALUE_UINT64, .u.value_uint64 = UINT64_C(5) };
	const otelc_metric_instrument_t          type = OTELC_METRIC_INSTRUMENT_COUNTER_UINT64;
	const otelc_metric_instrument_t          obs_type = OTELC_METRIC_INSTRUMENT_OBSERVABLE_COUNTER_INT64;
	const otelc_metric_aggregation_type_t    aggr = OTELC_METRIC_AGGREGATION_SUM;
	struct otelc_meter                      *meter;
	char                                    *err = NULL;
	int64_t                                  id_plain, id_viewed, id_obs;
	int                                      retval = TEST_PASS;

	meter = otelc_meter_create(ctx, &err);
	if (_NULL(meter) || (OTELC_OPS(meter, start) != OTELC_RET_OK)) {
		retval = TEST_FAIL;
	} else {
		/* The view goes in before the restart and its instrument. */
		if (OTELC_OPS(meter, add_view, "restart_renamed", "", "restart_source", "", type, aggr, NULL, 0) < 0)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, start) != OTELC_RET_OK)
			retval = TEST_FAIL;

		id_plain  = OTELC_OPS(meter, create_instrument, "restart_plain", "", "", type, NULL);
		id_viewed = OTELC_OPS(meter, create_instrument, "restart_source", "", "", type, NULL);
		id_obs    = OTELC_OPS(meter, create_instrument, "restart_observable", "", "", obs_type, &cb_fixed);
		if ((id_plain < 0) || (id_viewed < 0) || (id_obs < 0))
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, update_instrument, (int)id_plain, &val_7) < 0)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, update_instrument, (int)id_plain, &val_5) < 0)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, update_instrument, (int)id_viewed, &val_5) < 0)
			retval = TEST_FAIL;
		if (OTELC_OPS(meter, force_flush, NULL) != OTELC_RET_OK)
			retval = TEST_FAIL;
	}

	/* The destroy flushes and closes the file, which is then read back. */
	if (_nNULL(meter))
		OTELC_OPSR(meter, destroy);
	OTELC_SFREE(err);

	if (test_metric_value(METRICS_RESTART_FILE, "restart_plain") != 12)
		retval = TEST_FAIL;
	if (test_metric_value(METRICS_RESTART_FILE, "restart_renamed") != 5)
		retval = TEST_FAIL;
	if (test_metric_value(METRICS_RESTART_FILE, "restart_observable") != 42)
		retval = TEST_FAIL;

	test_report("meter restart keeps views and exports values", retval);
}


/***
 * NAME
 *   test_handle_statistics - checks that every issued handle is still live
 *
 * SYNOPSIS
 *   static int test_handle_statistics(const struct otelc_meter *meter)
 *
 * ARGUMENTS
 *   meter - meter instance whose instrument and view maps are inspected
 *
 * DESCRIPTION
 *   Parses the instrument and view sections of the otelc_statistics() string
 *   and verifies the invariant that holds at the end of a clean run whatever
 *   the number of tests: each map holds exactly the handles that were issued,
 *   no handle allocation failed, and nothing was erased or destroyed, since
 *   the handles live until the meter is destroyed.
 *
 * RETURN VALUE
 *   Returns TEST_PASS when the invariant holds, TEST_FAIL otherwise.
 */
static int test_handle_statistics(const struct otelc_meter *meter)
{
	static const char *const sections[] = { "instrument:{", "view:{" };
	const char              *ptr;
	char                     buffer[BUFSIZ] = "";
	size_t                   total, buckets, shards, peak, i;
	int64_t                  id, alloc_fail, erase, destroy;
	int                      retval = TEST_PASS;

	otelc_statistics(meter, buffer, sizeof(buffer));

	for (i = 0; i < OTELC_TABLESIZE(sections); i++) {
		ptr = strstr(buffer, sections[i]);
		if (_NULL(ptr) || (sscanf(ptr + strlen(sections[i]), " < %zu/%zu/%zu > %" SCNd64 " %zu %" SCNd64 " %" SCNd64 " %" SCNd64,
		                          &total, &buckets, &shards, &id, &peak, &alloc_fail, &erase, &destroy) != 8)) {
			OTELC_LOG(stderr, "  unexpected statistics: %s", buffer);

			return TEST_FAIL;
		}

		/* Every issued handle is live: nothing leaked or was erased. */
		if ((total != (size_t)id) || (alloc_fail != 0) || (erase != 0) || (destroy != 0))
			retval = TEST_FAIL;
	}

	OTELC_LOG(stdout, "  %s", buffer);

	return retval;
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
 *   Initializes the OpenTelemetry library, creates a meter, runs all meter
 *   tests, and reports the results.  A second context loads the 'restart'
 *   entry of the configuration, which writes to a file of its own, for the
 *   tests that start a meter of their own and read its exports back.
 *
 * RETURN VALUE
 *   Returns EX_OK if all tests pass, or EX_SOFTWARE if any test fails.
 */
int main(int argc, char **argv)
{
	struct otelc_ctx   *ctx   = NULL, *ctx_aux = NULL;
	struct otelc_meter *meter = NULL;
	const char         *cfg_file;
	char               *otel_err = NULL;
	int64_t             id_counter_u64 = -1, id_counter_dbl = -1;
	int64_t             id_histogram_u64 = -1, id_histogram_dbl = -1;
	int64_t             id_udcounter_i64 = -1, id_udcounter_dbl = -1;
	int64_t             id_obs_counter_i64 = -1, id_obs_counter_dbl = -1;
	int64_t             id_obs_gauge_i64 = -1, id_obs_gauge_dbl = -1;
	int64_t             id_obs_udcounter_i64 = -1, id_obs_udcounter_dbl = -1;
	int                 retval;

	retval = test_init(argc, argv, "meter tests", &cfg_file);
	if (retval >= 0)
		return retval;

#if !OTELC_HAVE_EXPORTER_OSTREAM || !OTELC_HAVE_EXPORTER_OTLP_FILE
	/* The suite's configuration entries write through both exporters. */
	test_skip("meter tests", "the build lacks the ostream or the OTLP file exporter");

	return test_summary(EX_OK);
#endif

	retval = EX_OK;
	OTELC_LOG(stdout, "");

	test_set_ctx(&ctx);

	ctx = otelc_init(cfg_file, test_get_ctx_name(), &otel_err);
	if (_NULL(ctx)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init library" : otel_err);

		return test_done(EX_SOFTWARE, otel_err);
	}

	ctx_aux = otelc_init(cfg_file, "restart", &otel_err);
	if (_NULL(ctx_aux)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init the auxiliary context" : otel_err);

		return test_done(EX_SOFTWARE, otel_err);
	}

	/***
	 * Tests that create and destroy meters in isolation.  These must run
	 * before the main meter is created because meter destruction tears
	 * down global state (instrument/view maps).
	 */
	OTELC_LOG(stdout, "[meter lifecycle]");
	test_meter_create_destroy(ctx);
	test_meter_create_err_null(ctx);
	test_meter_create_null_ctx();
	test_meter_unstarted(ctx);

	/***
	 * Create and start the main meter for the remaining tests.
	 */
	meter = otelc_meter_create(ctx, &otel_err);
	if (_NULL(meter)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to create meter" : otel_err);
		otelc_deinit(&ctx_aux, NULL, NULL, NULL);

		return test_done(EX_SOFTWARE, otel_err);
	}

	test_set_meter(&meter);
	test_meter_start(meter);

	/***
	 * Synchronous instrument creation tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[synchronous instruments]");
	test_create_counter_uint64(meter, &id_counter_u64);
	test_create_counter_double(meter, &id_counter_dbl);
	test_create_histogram_uint64(meter, &id_histogram_u64);
	test_create_histogram_double(meter, &id_histogram_dbl);
	test_create_udcounter_int64(meter, &id_udcounter_i64);
	test_create_udcounter_double(meter, &id_udcounter_dbl);
	test_create_duplicate_instrument(meter, id_counter_u64);
	test_get_instrument(meter, id_counter_u64);

	/***
	 * Observable instrument creation tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[observable instruments]");
	test_create_observable_counter_int64(meter, &id_obs_counter_i64);
	test_create_observable_counter_double(meter, &id_obs_counter_dbl);
	test_create_observable_gauge_int64(meter, &id_obs_gauge_i64);
	test_create_observable_gauge_double(meter, &id_obs_gauge_dbl);
	test_create_observable_udcounter_int64(meter, &id_obs_udcounter_i64);
	test_create_observable_udcounter_double(meter, &id_obs_udcounter_dbl);
	test_create_instrument_invalid_args(meter, id_obs_counter_i64);

	/***
	 * Instrument update tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[instrument updates]");
	test_update_counter_uint64(meter, id_counter_u64);
	test_update_counter_double(meter, id_counter_dbl);
	test_update_histogram_uint64(meter, id_histogram_u64);
	test_update_histogram_double(meter, id_histogram_dbl);
	test_update_udcounter_int64(meter, id_udcounter_i64);
	test_update_udcounter_double(meter, id_udcounter_dbl);
	test_update_counter_uint64_with_int64(meter, id_counter_u64);
	test_update_histogram_uint64_with_int64(meter, id_histogram_u64);
	test_update_uint64_with_negative_int64(meter, id_counter_u64, id_histogram_u64);
	test_update_instrument(meter, id_counter_u64, id_histogram_dbl);
	test_update_instrument_udcounter(meter, id_udcounter_i64, id_udcounter_dbl);
	test_update_instrument_uint64_with_int64(meter, id_counter_u64, id_histogram_u64);
	test_instrument_invalid_handle(meter, id_counter_u64);
	test_update_value_type_mismatch(meter, id_counter_u64, id_counter_dbl, id_udcounter_i64);
	test_update_invalid_args(meter, id_counter_u64, id_obs_counter_i64);

	/***
	 * View tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[views]");
	test_add_view_histogram(meter);
	test_add_view_default(meter);
	test_add_view_duplicate(meter);
	test_add_view_invalid_args(meter);

	/***
	 * Observable callback management tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[observable callbacks]");
	test_add_instrument_callback(meter, id_obs_counter_i64);
	test_remove_instrument_callback(meter, id_obs_counter_i64);

	/***
	 * Gauge instrument tests (ABI v2).
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[gauge instruments]");
	(void)test_gauge_instruments(meter);

	/***
	 * Meter operations.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[meter operations]");
	test_meter_enabled(meter);
	test_meter_set_enabled(meter, id_counter_u64);
	test_aggr_parse();
	test_meter_set_flush_timeout(ctx_aux);
	test_meter_restart_and_export(ctx_aux);
	test_force_flush(meter);
	test_shutdown(meter);

	/***
	 * Handle statistics verification.  Every instrument and view created
	 * during the run is still live at this point, so the maps must hold
	 * exactly the handles that were issued, with nothing erased and no
	 * allocation failure, whatever the number of tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[handle statistics]");

	if (test_handle_statistics(meter) != TEST_PASS)
		retval = TEST_FAIL;
	test_report("handle statistics", retval);

	otelc_deinit(&ctx_aux, NULL, NULL, NULL);

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
