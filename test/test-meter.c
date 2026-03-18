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
 *   test_meter_create_destroy - tests meter creation and destruction
 *
 * SYNOPSIS
 *   static void test_meter_create_destroy(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that a meter can be created and destroyed without errors.  Also
 *   verifies that the function pointers are properly initialized.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_create_destroy(void)
{
	struct otelc_meter *meter;
	char               *err = NULL;
	int                 retval = TEST_FAIL;

	meter = otelc_meter_create(&err);
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
 *   static void test_meter_create_err_null(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that a meter can be created when the err argument is NULL.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_create_err_null(void)
{
	struct otelc_meter *meter;
	int                 retval = TEST_FAIL;

	meter = otelc_meter_create(NULL);
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
	static struct otelc_metric_observable_cb cb_data = { .func = observable_int64_cb_2 };
	int                                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, add_instrument_callback, obs_id, &cb_data) == OTELC_RET_OK)
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
 *   Verifies that a previously registered callback can be removed from an
 *   observable instrument.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_remove_instrument_callback(struct otelc_meter *meter, int64_t obs_id)
{
	static struct otelc_metric_observable_cb cb_data = { .func = observable_int64_cb_2 };
	int                                      retval = TEST_FAIL;

	if (OTELC_OPS(meter, remove_instrument_callback, obs_id, &cb_data) == OTELC_RET_OK)
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
 *   Verifies that the enabled() function returns true or false
 *   (not OTELC_RET_ERROR) for a started meter.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_meter_enabled(struct otelc_meter *meter)
{
	int rc, retval = TEST_PASS;

	rc = OTELC_OPS(meter, enabled);
	if (rc == OTELC_RET_ERROR)
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
		/* ABI < 2: creation rejected, expected. */
		/* Do nothing. */;
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
 *   Verifies that the shutdown() function completes without errors using both
 *   an unlimited timeout (NULL) and a specific timeout value.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_shutdown(struct otelc_meter *meter)
{
	const struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
	int                   retval = TEST_FAIL;

	if ((OTELC_OPS(meter, shutdown, NULL) == OTELC_RET_OK) &&
	    (OTELC_OPS(meter, shutdown, &timeout) == OTELC_RET_OK))
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
 *   Initializes the OpenTelemetry library, creates a meter, runs all meter
 *   tests, and reports the results.
 *
 * RETURN VALUE
 *   Returns EX_OK if all tests pass, or EX_SOFTWARE if any test fails.
 */
int main(int argc, char **argv)
{
	struct otelc_meter *meter = NULL;
	const char         *cfg_file;
	char               *otel_err = NULL;
	int64_t             id_counter_u64 = -1, id_counter_dbl = -1;
	int64_t             id_histogram_u64 = -1, id_histogram_dbl = -1;
	int64_t             id_udcounter_i64 = -1, id_udcounter_dbl = -1;
	int64_t             id_obs_counter_i64 = -1, id_obs_counter_dbl = -1;
	int64_t             id_obs_gauge_i64 = -1, id_obs_gauge_dbl = -1;
	int64_t             id_obs_udcounter_i64 = -1, id_obs_udcounter_dbl = -1;
	int                 num_gauges;
	int                 retval;

	retval = test_init(argc, argv, "meter tests", &cfg_file);
	if (retval >= 0)
		return retval;

	retval = EX_OK;
	OTELC_LOG(stdout, "");

	if (otelc_init(cfg_file, &otel_err) == OTELC_RET_ERROR) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to init library" : otel_err);

		return test_done(EX_SOFTWARE, otel_err);
	}

	/***
	 * Tests that create and destroy meters in isolation.  These must run
	 * before the main meter is created because meter destruction tears
	 * down global state (instrument/view maps).
	 */
	OTELC_LOG(stdout, "[meter lifecycle]");
	test_meter_create_destroy();
	test_meter_create_err_null();

	/***
	 * Create and start the main meter for the remaining tests.
	 */
	meter = otelc_meter_create(&otel_err);
	if (_NULL(meter)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to create meter" : otel_err);

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

	/***
	 * View tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[views]");
	test_add_view_histogram(meter);
	test_add_view_default(meter);
	test_add_view_duplicate(meter);

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
	num_gauges = test_gauge_instruments(meter);

	/***
	 * Meter operations.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[meter operations]");
	test_meter_enabled(meter);
	test_force_flush(meter);
	test_shutdown(meter);

	/***
	 * Handle statistics verification.
	 *
	 * 12 + num_gauges unique instruments (6 sync + 6 observable,
	 * plus 0 or 2 gauges depending on ABI version).  3 unique
	 * views.  Duplicates return the existing ID without new
	 * allocations.  All handles are still live at this point
	 * (destroyed with meter).
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[handle statistics]");

	if (otelc_statistics_check(2, 12 + num_gauges, 12 + num_gauges, 0, 0, 0) != 0)
		retval = TEST_FAIL;
	if (otelc_statistics_check(3, 3, 3, 0, 0, 0) != 0)
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
