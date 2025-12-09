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


#ifdef OTELC_USE_STATIC_HANDLE
THREAD_LOCAL struct otel_handle<struct otel_span_handle *>          otel_span{};
THREAD_LOCAL struct otel_handle<struct otel_span_context_handle *>  otel_span_context{};
#else
THREAD_LOCAL struct otel_handle<struct otel_span_handle *>         *otel_span = nullptr;
THREAD_LOCAL struct otel_handle<struct otel_span_context_handle *> *otel_span_context = nullptr;
#endif


/***
 * NAME
 *   otel_span_get_id - retrieves the identifiers associated with a span
 *
 * SYNOPSIS
 *   static int otel_span_get_id(const struct otelc_span *span, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
 *
 * ARGUMENTS
 *   span          - span instance
 *   span_id       - buffer to store the span ID
 *   span_id_size  - size of the span ID buffer
 *   trace_id      - buffer to store the trace ID
 *   trace_id_size - size of the trace ID buffer
 *   trace_flags   - buffer to store the trace flags
 *
 * DESCRIPTION
 *   Retrieves the unique identifiers associated with a span's context.  These
 *   identifiers are essential for linking spans together into a single trace.
 *   Any of the output pointers can be null if that specific identifier is not
 *   needed.  The provided buffers for span_id and trace_id must be large enough
 *   to hold the respective identifiers.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_get_id(const struct otelc_span *span, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
{
	OTEL_LOCK_TRACER(span);

	OTELC_FUNC("%p, %p, %zu, %p, %zu, %p", span, span_id, span_id_size, trace_id, trace_id_size, trace_flags);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	const auto span_ctx = handle->span->GetContext();
	if (!span_ctx.IsValid())
		OTEL_SPAN_ERETURN_INT("Invalid span context");

	/* Copy the span ID, trace ID, and trace flags to the caller's buffers. */
	if (!OTEL_NULL(span_id) && (span_id_size >= otel_trace::SpanId::kSize))
		span_ctx.span_id().CopyBytesTo(otel_nostd::span<uint8_t, otel_trace::SpanId::kSize>(span_id, otel_trace::SpanId::kSize));
	if (!OTEL_NULL(trace_id) && (trace_id_size >= otel_trace::TraceId::kSize))
		span_ctx.trace_id().CopyBytesTo(otel_nostd::span<uint8_t, otel_trace::TraceId::kSize>(trace_id, otel_trace::TraceId::kSize));
	if (!OTEL_NULL(trace_flags))
		span_ctx.trace_flags().CopyBytesTo(otel_nostd::span<uint8_t, 1>(trace_flags, 1));

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_span_is_recording - checks whether the span is recording
 *
 * SYNOPSIS
 *   static int otel_span_is_recording(const struct otelc_span *span)
 *
 * ARGUMENTS
 *   span - span instance
 *
 * DESCRIPTION
 *   Queries the underlying span to determine whether it is currently recording
 *   events and attributes.  A span that is not recording (e.g. not sampled)
 *   will silently discard any data added to it, so callers can use this check
 *   to skip expensive attribute or event construction.
 *
 * RETURN VALUE
 *   Returns true if the span is recording, false if it is not,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_is_recording(const struct otelc_span *span)
{
	OTEL_LOCK_TRACER(span);

	OTELC_FUNC("%p", span);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	OTELC_RETURN_INT(handle->span->IsRecording() ? true : false);
}


/***
 * NAME
 *   otel_span_set_status - sets the status of the span
 *
 * SYNOPSIS
 *   static int otel_span_set_status(const struct otelc_span *span, otelc_span_status_t status, const char *desc)
 *
 * ARGUMENTS
 *   span   - span instance
 *   status - status set on a span
 *   desc   - description of the status
 *
 * DESCRIPTION
 *   Used to set the status of the span and usually to determine that the span
 *   has not been successfully completed - that is, that some error has occurred
 *   (using code OTELC_SPAN_STATUS_ERROR).  By default, all spans have a status
 *   of OTELC_SPAN_STATUS_UNSET, which means that the span operation completed
 *   without error.  The OTELC_SPAN_STATUS_OK status is reserved for situations
 *   where a span needs to be explicitly marked as successful.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_set_status(const struct otelc_span *span, otelc_span_status_t status, const char *desc)
{
	OTEL_LOCK_TRACER(span);

	OTELC_FUNC("%p, %d, \"%s\"", span, status, OTELC_STR_ARG(desc));

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (!OTELC_IN_RANGE(status, OTELC_SPAN_STATUS_UNSET, OTELC_SPAN_STATUS_ERROR))
		OTEL_SPAN_ERETURN_INT("Invalid span status: %d", status);

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	handle->span->SetStatus(OTEL_CAST_STATIC(otel_trace::StatusCode, status), otel_nostd::string_view{OTEL_NULL(desc) ? "" : desc});

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_span_inject_carrier - injects span context into a carrier
 *
 * SYNOPSIS
 *   template <template <typename> class C, typename W>
 *   static int otel_span_inject_carrier(const struct otelc_span *span, W *carrier, const char *carrier_name, const char *dump_label)
 *
 * ARGUMENTS
 *   span         - span instance
 *   carrier      - medium used by propagators to write values to
 *   carrier_name - human-readable carrier name for error messages
 *   dump_label   - label for text map dump
 *
 * DESCRIPTION
 *   Used to store context information in a carrier used to propagate traces
 *   between services.  This template consolidates the common logic for text
 *   map and HTTP headers injection.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK if the context is injected into the carrier,
 *   or OTELC_RET_ERROR in case of an error.
 */
template <template <typename> class C, typename W>
static int otel_span_inject_carrier(const struct otelc_span *span, W *carrier, const char *carrier_name, const char *dump_label __maybe_unused)
{
	OTEL_LOCK_TRACER(span);
	/***
	 * NOTE: the carrier data may contain initially some predefined content
	 * to which the injected content will be added.
	 */
	std::map<std::string, std::string>    carrier_data{};
	C<std::map<std::string, std::string>> otel_carrier(carrier_data);

	OTELC_FUNC("%p, %p", span, carrier);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(carrier))
		OTEL_SPAN_ERETURN_INT("Invalid carrier");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	/* Inject the span context into the carrier via the global propagator. */
	const auto propagator = otel_context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
	propagator->Inject(otel_carrier, *(handle->context));

#ifndef OTELC_USE_COMPOSITE_PROPAGATOR
	/***
	 * Baggage content is also added to the carrier.
	 */
	const auto baggage = otel_baggage::GetBaggage(*(handle->context));
	if (!OTEL_NULL(baggage)) {
		OTEL_DBG_BAGGAGE(baggage);

		const auto header = baggage->ToHeader();
		if (header.size() > 0)
			otel_carrier.Set(otel_baggage::kBaggageHeader, header);
	}
#endif

	OTELC_DBG_IFDEF((void)otel_carrier.Keys(nullptr), );

	if (otel_carrier.data().empty())
		OTEL_SPAN_ERETURN_INT("No injected data");
	else if (OTEL_NULL(OTELC_TEXT_MAP_NEW(&(carrier->text_map), otel_carrier.data().size())))
		OTEL_SPAN_ERETURN_INT("Unable to allocate memory for %s carrier", carrier_name);

	/* Copy injected data from the internal carrier to the caller's carrier. */
	for (const auto &it : otel_carrier.data()) {
		int rc = OTELC_RET_OK;

		if (!OTEL_NULL(carrier->set))
			rc = carrier->set(carrier, it.first.c_str(), it.second.c_str());
		else
			rc = OTELC_TEXT_MAP_ADD(&(carrier->text_map), it.first.c_str(), 0, it.second.c_str(), 0, OTELC_TEXT_MAP_AUTO);

		if (rc == OTELC_RET_ERROR) {
			otelc_text_map_free(&(carrier->text_map));
			OTEL_SPAN_ERETURN_INT("Unable to copy injected data to %s carrier", carrier_name);
		}
	}

	OTEL_DBG_SPAN_CONTEXT();
	OTELC_DBG_CARRIER_WRITER(OTELC, "carrier", carrier);
	OTELC_TEXT_MAP_DUMP(&(carrier->text_map), dump_label);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_span_inject_text_map - injects span context into text map carrier
 *
 * SYNOPSIS
 *   static int otel_span_inject_text_map(const struct otelc_span *span, struct otelc_text_map_writer *carrier)
 *
 * ARGUMENTS
 *   span    - span instance
 *   carrier - medium used by propagators to write values to
 *
 * DESCRIPTION
 *   Used to store context information in a text map carrier used to propagate
 *   traces between services.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK if the context is injected into the carrier,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_inject_text_map(const struct otelc_span *span, struct otelc_text_map_writer *carrier)
{
	return otel_span_inject_carrier<OTEL_MAP_CARRIER, struct otelc_text_map_writer>(span, carrier, "text map", "text_map inject");
}


/***
 * NAME
 *   otel_span_inject_http_headers - injects span context into HTTP headers carrier
 *
 * SYNOPSIS
 *   static int otel_span_inject_http_headers(const struct otelc_span *span, struct otelc_http_headers_writer *carrier)
 *
 * ARGUMENTS
 *   span    - span instance
 *   carrier - medium used by propagators to write values to
 *
 * DESCRIPTION
 *   Used to store context information in an HTTP headers carrier used to
 *   propagate traces between services.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK if the context is injected into the carrier,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_inject_http_headers(const struct otelc_span *span, struct otelc_http_headers_writer *carrier)
{
	return otel_span_inject_carrier<OTEL_HTTP_CARRIER, struct otelc_http_headers_writer>(span, carrier, "HTTP headers", "http_headers inject");
}


/***
 * NAME
 *   otel_span_end_with_options - marks the end of the span
 *
 * SYNOPSIS
 *   static void otel_span_end_with_options(struct otelc_span **span, const struct timespec *ts_steady, otelc_span_status_t status, const char *desc)
 *
 * ARGUMENTS
 *   span      - address of a span instance pointer to be ended and destroyed
 *   ts_steady - time when the span finished (monotonic clock)
 *   status    - status code of a finished span
 *   desc      - description of the status
 *
 * DESCRIPTION
 *   Finalizes the operations with span.  It must be the last call for the span
 *   instance.  The optional <ts_steady> argument sets the end time of the span.
 *   <status> is used to set the status of the span and its setting can be
 *   avoided if OTELC_SPAN_STATUS_IGNORE is used as the argument.  <desc> is
 *   used as a text description that can be set for the span status.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_span_end_with_options(struct otelc_span **span, const struct timespec *ts_steady, otelc_span_status_t status, const char *desc)
{
	OTEL_LOCK_TRACER(span);

	OTELC_FUNC("%p:%p, %p, %d, \"%s\"", OTELC_DPTR_ARGS(span), ts_steady, status, OTELC_STR_ARG(desc));

	if (OTEL_NULL(span) || OTEL_NULL(*span))
		OTELC_RETURN();

	/* End the span with optional status and steady-clock timestamp. */
	const auto handle = OTEL_SPAN_HANDLE(*span);
	if (!OTEL_NULL(handle)) {
		otel_trace::EndSpanOptions end_options{};

		/***
		 * end_steady_time - sets the end time of a Span
		 */
		if (!OTEL_NULL(ts_steady))
			end_options.end_steady_time = otel_steady_timestamp(timespec_to_duration(ts_steady));

		if (OTELC_IN_RANGE(status, OTELC_SPAN_STATUS_UNSET, OTELC_SPAN_STATUS_ERROR)) {
			static constexpr const char *description[] = { "Status UNSET", "Status OK", "Status ERROR" };

			handle->span->SetStatus(OTEL_CAST_STATIC(otel_trace::StatusCode, status), otel_nostd::string_view{OTEL_NULL(desc) ? description[status] : desc});
		}
		else
			OTELC_DBG(OTEL, "span status not set: %d", status);

		handle->span->End(end_options);
	} else {
		OTELC_DBG(OTEL, "invalid otel_span[%" PRId64 "]", (*span)->idx);
	}

	otel_nolock_span_destroy(span);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_span_end - marks the end of the span
 *
 * SYNOPSIS
 *   static void otel_span_end(struct otelc_span **span)
 *
 * ARGUMENTS
 *   span - address of a span instance pointer to be ended and destroyed
 *
 * DESCRIPTION
 *   Finalizes the operations with span.  It must be the last call for the span
 *   instance.  This function calls the function otel_span_end_with_options(),
 *   which offers additional control over span finalization.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_span_end(struct otelc_span **span)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(span));

	otel_span_end_with_options(span, nullptr, OTELC_SPAN_STATUS_IGNORE, nullptr);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_span_set_operation_name - sets the span name
 *
 * SYNOPSIS
 *   static void otel_span_set_operation_name(const struct otelc_span *span, const char *operation_name)
 *
 * ARGUMENTS
 *   span           - span instance
 *   operation_name - span name
 *
 * DESCRIPTION
 *   Sets the span name.  If used, this will override the name set during
 *   creation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_span_set_operation_name(const struct otelc_span *span, const char *operation_name)
{
	OTEL_LOCK_TRACER(span);

	OTELC_FUNC("%p, \"%s\"", span, OTELC_STR_ARG(operation_name));

	if (OTEL_NULL(span))
		OTELC_RETURN();
	else if (!OTELC_STR_IS_VALID(operation_name))
		OTEL_SPAN_ERETURN("Invalid operation name");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN("Invalid span");

	handle->span->UpdateName(operation_name);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_span_set_baggage_var - sets baggage key-value pairs
 *
 * SYNOPSIS
 *   static int otel_span_set_baggage_var(const struct otelc_span *span, const char *key, const char *value, ...)
 *
 * ARGUMENTS
 *   span  - span instance
 *   key   - baggage name
 *   value - baggage value
 *   ...   - additional key-value string pairs, terminated by a NULL key
 *
 * DESCRIPTION
 *   Stores data in a baggage key-value store that can be propagated alongside
 *   context.  The baggage name and value can be any valid UTF-8 character
 *   string, with the restriction that the name cannot be an empty character
 *   string.
 *
 * RETURN VALUE
 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in case of
 *   an error.
 */
static int otel_span_set_baggage_var(const struct otelc_span *span, const char *key, const char *value, ...)
{
	OTEL_LOCK_TRACER(span);
	va_list ap;
	int     retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", \"%s\", ...", span, OTELC_STR_ARG(key), OTELC_STR_ARG(value));

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_ERETURN_INT("Invalid baggage name");
	else if (!OTELC_STR_IS_VALID(value))
		OTEL_SPAN_ERETURN_INT("Invalid baggage value");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	auto baggage = otel_baggage::GetBaggage(*(handle->context));

	/* Iterate over the variadic key-value pairs and set each in baggage. */
	OTEL_VA_AUTO(ap, value);
	for (retval = 0; !OTEL_NULL(key) && !OTEL_NULL(value); retval++) {
		baggage = baggage->Set(key, value);

		key = va_arg(ap, typeof(key));
		if (!OTEL_NULL(key))
			value = va_arg(ap, typeof(value));
	}

	OTEL_SPAN_UPDATE_BAGGAGE(handle, baggage, retval);
}


/***
 * NAME
 *   otel_span_set_baggage_kv_var - sets baggage key-value pairs
 *
 * SYNOPSIS
 *   static int otel_span_set_baggage_kv_var(const struct otelc_span *span, const struct otelc_kv *kv, ...)
 *
 * ARGUMENTS
 *   span - span instance
 *   kv   - baggage key-value pair
 *   ...  - additional key-value pairs, terminated by NULL
 *
 * DESCRIPTION
 *   Stores data in a baggage key-value store that can be propagated alongside
 *   context.  The baggage name and value can be any valid UTF-8 character
 *   string, with the restriction that the name cannot be an empty character
 *   string.
 *
 * RETURN VALUE
 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in case of
 *   an error.
 */
static int otel_span_set_baggage_kv_var(const struct otelc_span *span, const struct otelc_kv *kv, ...)
{
	OTEL_LOCK_TRACER(span);
	va_list ap;
	int     retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %p, ...", span, kv);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_ERETURN_INT("Invalid baggage key-value");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	auto baggage = otel_baggage::GetBaggage(*(handle->context));

	/* Iterate over the variadic kv pairs and set each in baggage. */
	OTEL_VA_AUTO(ap, kv);
	for (retval = 0; !OTEL_NULL(kv); retval++) {
		if (kv->value.u_type == OTELC_VALUE_STRING) {
			baggage = baggage->Set(kv->key, kv->value.u.value_string);
		}
		else if (kv->value.u_type == OTELC_VALUE_DATA) {
			baggage = baggage->Set(kv->key, OTEL_CAST_REINTERPRET(const char *, kv->value.u.value_data));
		}
		else {
			OTELC_DBG(ERROR, "invalid value data type: %d", kv->value.u_type);

			OTEL_SPAN_ERETURN_INT("Invalid value data type");
		}

		kv = va_arg(ap, typeof(kv));
	}

	OTEL_SPAN_UPDATE_BAGGAGE(handle, baggage, retval);
}


/***
 * NAME
 *   otel_span_set_baggage_kv_n - sets baggage key-value pairs
 *
 * SYNOPSIS
 *   static int otel_span_set_baggage_kv_n(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
 *
 * ARGUMENTS
 *   span   - span instance
 *   kv     - an array of key-value pairs of baggage to be set
 *   kv_len - size of key-value pair array
 *
 * DESCRIPTION
 *   Stores data in a baggage key-value store that can be propagated alongside
 *   context.  The baggage name and value can be any valid UTF-8 character
 *   string, with the restriction that the name cannot be an empty character
 *   string.
 *
 * RETURN VALUE
 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in case of
 *   an error.
 */
static int otel_span_set_baggage_kv_n(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
{
	OTEL_LOCK_TRACER(span);
	int retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %p, %zu", span, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_ERETURN_INT("Invalid baggage key-value");
	else if (kv_len == 0)
		OTEL_SPAN_ERETURN_INT("Invalid baggage key-value array size");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	auto baggage = otel_baggage::GetBaggage(*(handle->context));

	/* Iterate over the kv array and set each entry in baggage. */
	for (retval = 0; retval < OTEL_CAST_STATIC(int, kv_len); retval++)
		if (kv[retval].value.u_type == OTELC_VALUE_STRING) {
			baggage = baggage->Set(kv[retval].key, kv[retval].value.u.value_string);
		}
		else if (kv[retval].value.u_type == OTELC_VALUE_DATA) {
			baggage = baggage->Set(kv[retval].key, OTEL_CAST_REINTERPRET(const char *, kv[retval].value.u.value_data));
		}
		else {
			OTELC_DBG(ERROR, "invalid value data type: %d", kv[retval].value.u_type);

			OTEL_SPAN_ERETURN_INT("Invalid value data type");
		}

	OTEL_SPAN_UPDATE_BAGGAGE(handle, baggage, retval);
}


/***
 * NAME
 *   otel_span_get_baggage - gets the value associated with the requested baggage name
 *
 * SYNOPSIS
 *   static char *otel_span_get_baggage(const struct otelc_span *span, const char *key)
 *
 * ARGUMENTS
 *   span - span instance
 *   key  - baggage name
 *
 * DESCRIPTION
 *   Used to access the value of a baggage key-value pair set by a previous
 *   event.
 *
 * RETURN VALUE
 *   Returns a newly allocated string containing the value associated with the
 *   specified name, or NULL if the specified name is not present and also in
 *   case of error.  The caller is responsible for freeing the returned string.
 */
static char *otel_span_get_baggage(const struct otelc_span *span, const char *key)
{
	OTEL_LOCK_TRACER(span);
	std::string  value;
	char        *retptr = nullptr;

	OTELC_FUNC("%p, \"%s\"", span, OTELC_STR_ARG(key));

	if (OTEL_NULL(span))
		OTELC_RETURN_PTR(nullptr);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_ERETURN_PTR("Invalid baggage name");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_PTR("Invalid span");

	const auto baggage = otel_baggage::GetBaggage(*(handle->context));
	OTEL_DBG_BAGGAGE(baggage);

	if (baggage->GetValue(key, value)) {
		retptr = OTELC_STRNDUP(__func__, __LINE__, value.c_str(), value.length());
		if (OTEL_NULL(retptr))
			OTEL_SPAN_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("baggage value"));
	}

	OTELC_RETURN_EX(retptr, typeof(retptr), "\"%s\"");
}


/***
 * NAME
 *   otel_span_get_baggage_var - gets the value associated with the requested baggage name
 *
 * SYNOPSIS
 *   static struct otelc_text_map *otel_span_get_baggage_var(const struct otelc_span *span, const char *key, ...)
 *
 * ARGUMENTS
 *   span - span instance
 *   key  - baggage name
 *   ...  - additional baggage names, terminated by NULL
 *
 * DESCRIPTION
 *   Used to access the values of baggage key-value pairs set by a previous
 *   event.  Returns a text map containing the baggage key-value pairs that
 *   correspond to the baggage names given via the function arguments.  The
 *   size of the text map (the maximum number of contained key-value pairs)
 *   is always equal to the number of input arguments, regardless of how many
 *   values are actually found for the given names.  This means that the number
 *   of found key-value pairs can be less than this amount.
 *
 * RETURN VALUE
 *   Returns a text map containing the baggage key-value pairs, or NULL if
 *   there was an error.
 */
static struct otelc_text_map *otel_span_get_baggage_var(const struct otelc_span *span, const char *key, ...)
{
	OTEL_LOCK_TRACER(span);
	va_list                ap;
	struct otelc_text_map *retptr = nullptr;
	int                    i, m, n;

	OTELC_FUNC("%p, \"%s\", ...", span, OTELC_STR_ARG(key));

	if (OTEL_NULL(span))
		OTELC_RETURN_PTR(nullptr);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_ERETURN_PTR("Invalid baggage name");

	va_start(ap, key);
	for (n = 1; !OTEL_NULL(va_arg(ap, typeof(key))); n++);
	va_end(ap);

	if (OTEL_NULL(retptr = OTELC_TEXT_MAP_NEW(nullptr, n)))
		OTEL_SPAN_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("baggage map"));

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle)) {
		otelc_text_map_destroy(&retptr);

		OTEL_SPAN_ERETURN_PTR("Invalid span");
	}

	const auto baggage = otel_baggage::GetBaggage(*(handle->context));
	OTEL_DBG_BAGGAGE(baggage);

	/* Look up each requested key in the baggage and add matches to the map. */
	OTEL_VA_AUTO(ap, key);
	for (i = m = 0; (i < n) && !OTEL_NULL(key); i++) {
		std::string value;

		if (baggage->GetValue(key, value)) {
			if (OTELC_TEXT_MAP_ADD(retptr, key, 0, value.c_str(), value.length(), OTELC_TEXT_MAP_AUTO) == OTELC_RET_ERROR) {
				otelc_text_map_destroy(&retptr);

				OTEL_SPAN_ERETURN_PTR("Unable to copy baggage data to text map");
			}

			OTELC_DBG(OTELC, "get baggage[%d]: \"%s\" -> \"%s\"", i, retptr->key[m], retptr->value[m]);

			m++;
		} else {
			OTELC_DBG(OTELC, "get baggage[%d]: \"%s\" -> UNSET", i, key);
		}

		key = va_arg(ap, typeof(key));
	}

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otel_span_set_one_attribute - sets an attribute on the span
 *
 * SYNOPSIS
 *   static int otel_span_set_one_attribute(otel_nostd::shared_ptr<otel_trace::Span> span, const char *key, const struct otelc_value *value)
 *
 * ARGUMENTS
 *   span  - span instance
 *   key   - key of the attribute being set
 *   value - value of the attribute being set
 *
 * DESCRIPTION
 *   Sets a single attribute on the span.  An attribute is a key-value pair with
 *   a unique key that provides additional information about the span.  If an
 *   attribute with the same key has already been set, its value will be updated
 *   with the new one.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK if the attribute is set, or OTELC_RET_ERROR in case of
 *   an error.
 */
static int otel_span_set_one_attribute(otel_nostd::shared_ptr<otel_trace::Span> span, const char *key, const struct otelc_value *value)
{
	if (OTEL_NULL(key) || OTEL_NULL(value))
		return OTELC_RET_ERROR;

	OTELC_DBG(DEBUG, "'%s' -> %s", key, otelc_value_dump(value, ""));

	if (value->u_type == OTELC_VALUE_NULL)
		span->SetAttribute(key, "");
	else if (value->u_type == OTELC_VALUE_BOOL)
		span->SetAttribute(key, value->u.value_bool);
	else if (value->u_type == OTELC_VALUE_INT32)
		span->SetAttribute(key, value->u.value_int32);
	else if (value->u_type == OTELC_VALUE_INT64)
		span->SetAttribute(key, value->u.value_int64);
	else if (value->u_type == OTELC_VALUE_UINT32)
		span->SetAttribute(key, value->u.value_uint32);
	else if (value->u_type == OTELC_VALUE_UINT64)
		span->SetAttribute(key, value->u.value_uint64);
	else if (value->u_type == OTELC_VALUE_DOUBLE)
		span->SetAttribute(key, value->u.value_double);
	else if (value->u_type == OTELC_VALUE_STRING)
		span->SetAttribute(key, value->u.value_string);
	else if (value->u_type == OTELC_VALUE_DATA)
		span->SetAttribute(key, OTEL_CAST_REINTERPRET(const char *, value->u.value_data));
	else
		return OTELC_RET_ERROR;

	return OTELC_RET_OK;
}


/***
 * NAME
 *   otel_span_set_attribute_var - sets an attribute on the span
 *
 * SYNOPSIS
 *   static int otel_span_set_attribute_var(const struct otelc_span *span, const char *key, const struct otelc_value *value, ...)
 *
 * ARGUMENTS
 *   span  - span instance
 *   key   - key of the attribute being set
 *   value - value of the attribute being set
 *   ...   - additional key-value pairs, terminated by a NULL key
 *
 * DESCRIPTION
 *   Sets attributes on the span.  An attribute is a key-value pair with a
 *   unique key that provides additional information about the span.  If an
 *   attribute with the same key has already been set, its value will be
 *   updated with the new one.
 *
 * RETURN VALUE
 *   Returns the number of attributes set, or OTELC_RET_ERROR in case of an
 *   error.
 */
static int otel_span_set_attribute_var(const struct otelc_span *span, const char *key, const struct otelc_value *value, ...)
{
	OTEL_LOCK_TRACER(span);
	va_list ap;
	int     retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", %p, ...", span, OTELC_STR_ARG(key), value);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_ERETURN_INT("Invalid attribute key");
	else if (OTEL_NULL(value))
		OTEL_SPAN_ERETURN_INT("Invalid attribute value");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	/* Iterate over the variadic key-value pairs and set each attribute. */
	OTEL_VA_AUTO(ap, value);
	for (retval = 0; !OTEL_NULL(key) && !OTEL_NULL(value); retval++) {
		if (otel_span_set_one_attribute(handle->span, key, value) == OTELC_RET_ERROR)
			OTEL_SPAN_ERETURN_INT("Unable to set span attribute");

		key = va_arg(ap, typeof(key));
		if (!OTEL_NULL(key))
			value = va_arg(ap, typeof(value));
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_span_set_attribute_kv_var - sets an attribute on the span
 *
 * SYNOPSIS
 *   static int otel_span_set_attribute_kv_var(const struct otelc_span *span, const struct otelc_kv *kv, ...)
 *
 * ARGUMENTS
 *   span - span instance
 *   kv   - key-value pair of the attribute being set
 *   ...  - additional key-value pairs, terminated by NULL
 *
 * DESCRIPTION
 *   Sets attributes on the span.  An attribute is a key-value pair with a
 *   unique key that provides additional information about the span.  If an
 *   attribute with the same key has already been set, its value will be
 *   updated with the new one.
 *
 * RETURN VALUE
 *   Returns the number of attributes set, or OTELC_RET_ERROR in case of an
 *   error.
 */
static int otel_span_set_attribute_kv_var(const struct otelc_span *span, const struct otelc_kv *kv, ...)
{
	OTEL_LOCK_TRACER(span);
	va_list ap;
	int     retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %p, ...", span, kv);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_ERETURN_INT("Invalid attribute key-value");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	/* Iterate over the variadic kv pairs and set each attribute. */
	OTEL_VA_AUTO(ap, kv);
	for (retval = 0; !OTEL_NULL(kv); retval++) {
		if (otel_span_set_one_attribute(handle->span, kv->key, &(kv->value)) == OTELC_RET_ERROR)
			OTEL_SPAN_ERETURN_INT("Unable to set span attribute");

		kv = va_arg(ap, typeof(kv));
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_span_set_attribute_kv_n - sets an attribute on the span
 *
 * SYNOPSIS
 *   static int otel_span_set_attribute_kv_n(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
 *
 * ARGUMENTS
 *   span   - span instance
 *   kv     - an array of key-value pairs of attributes to be set
 *   kv_len - size of key-value pair array
 *
 * DESCRIPTION
 *   Sets attributes on the span.  An attribute is a key-value pair with a
 *   unique key that provides additional information about the span.  If an
 *   attribute with the same key has already been set, its value will be
 *   updated with the new one.
 *
 * RETURN VALUE
 *   Returns the number of attributes set, or OTELC_RET_ERROR in case of an
 *   error.
 */
static int otel_span_set_attribute_kv_n(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
{
	OTEL_LOCK_TRACER(span);
	int retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %p, %zu", span, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_ERETURN_INT("Invalid attribute key-value");
	else if (kv_len == 0)
		OTEL_SPAN_ERETURN_INT("Invalid attribute key-value array size");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	/* Iterate over the kv array and set each attribute. */
	for (retval = 0; retval < OTEL_CAST_STATIC(int, kv_len); retval++)
		if (otel_span_set_one_attribute(handle->span, kv[retval].key, &(kv[retval].value)) == OTELC_RET_ERROR)
			OTEL_SPAN_ERETURN_INT("Unable to set span attribute");

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_span_add_one_event - adds an attribute to an event's attribute map
 *
 * SYNOPSIS
 *   static int otel_span_add_one_event(const struct otelc_span *span, std::map<std::string, otel_attribute_value> &attr, const char *key, const struct otelc_value *value)
 *
 * ARGUMENTS
 *   span  - span instance
 *   attr  - map of attributes containing key-value pairs with unique keys
 *   key   - key of the attribute being added to the map
 *   value - value of the attribute being added to the map
 *
 * DESCRIPTION
 *   Adds a single attribute to the event's attribute map.  An attribute is a
 *   key-value pair with a unique key.  The attribute map can later be used as
 *   an argument when adding the event to the span.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK if the attribute has been added, or OTELC_RET_ERROR in
 *   case of an error.
 */
static int otel_span_add_one_event(const struct otelc_span *span, std::map<std::string, otel_attribute_value> &attr, const char *key, const struct otelc_value *value)
{
	OTELC_FUNC("%p, <attr>, \"%s\", %p", span, OTELC_STR_ARG(key), value);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(key))
		OTEL_SPAN_ERETURN_INT("Invalid event attribute");

	OTELC_DBG(DEBUG, "'%s' -> %s", key, otelc_value_dump(value, ""));

	OTEL_VALUE_ADD(_INT, attr, emplace, key, value, &(span->tracer->err), "Unable to add event");

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_span_add_event_var - adds an event to the span
 *
 * SYNOPSIS
 *   static int otel_span_add_event_var(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const char *key, const struct otelc_value *value, ...)
 *
 * ARGUMENTS
 *   span      - span instance
 *   name      - name of the event being added
 *   ts_system - time of the event being added
 *   key       - attribute key for the event being added
 *   value     - attribute value for the event being added
 *   ...       - additional attribute key-value pairs, terminated by a NULL key
 *
 * DESCRIPTION
 *   Adds an event to the span.  An event can be customized with a timestamp
 *   and a set of attributes, which are key-value pairs providing additional
 *   information.
 *
 * RETURN VALUE
 *   Returns the number of attributes that the added event contains,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_add_event_var(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const char *key, const struct otelc_value *value, ...)
{
	OTEL_LOCK_TRACER(span);
	va_list                                     ap;
	std::map<std::string, otel_attribute_value> attr{};
	otel_system_timestamp                       timestamp = std::chrono::system_clock::now();
	int                                         retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", %p, \"%s\", %p, ...", span, OTELC_STR_ARG(name), ts_system, OTELC_STR_ARG(key), value);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(name))
		OTEL_SPAN_ERETURN_INT("Invalid event name");
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_ERETURN_INT("Invalid event key");
	else if (OTEL_NULL(value))
		OTEL_SPAN_ERETURN_INT("Invalid event value");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	if (!OTEL_NULL(ts_system))
		timestamp = otel_system_timestamp(timespec_to_duration(ts_system));

	/* Iterate over the variadic key-value pairs and add each to the event. */
	OTEL_VA_AUTO(ap, value);
	for (retval = 0; !OTEL_NULL(key) && !OTEL_NULL(value); retval++) {
		if (otel_span_add_one_event(span, attr, key, value) == OTELC_RET_ERROR)
			OTEL_SPAN_ERETURN_INT("Unable to set span event attribute");

		key = va_arg(ap, typeof(key));
		if (!OTEL_NULL(key))
			value = va_arg(ap, typeof(value));
	}

	if (retval > 0)
		handle->span->AddEvent(otel_nostd::string_view{name}, timestamp, attr);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_span_add_event_kv_var - adds an event to the span
 *
 * SYNOPSIS
 *   static int otel_span_add_event_kv_var(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const struct otelc_kv *kv, ...)
 *
 * ARGUMENTS
 *   span      - span instance
 *   name      - name of the event being added
 *   ts_system - time of the event being added
 *   kv        - key-value pair of the attribute being set
 *   ...       - additional key-value pairs, terminated by NULL
 *
 * DESCRIPTION
 *   Adds an event to the span.  An event can be customized with a timestamp
 *   and a set of attributes, which are key-value pairs providing additional
 *   information.
 *
 * RETURN VALUE
 *   Returns the number of attributes that the added event contains,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_add_event_kv_var(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const struct otelc_kv *kv, ...)
{
	OTEL_LOCK_TRACER(span);
	va_list                                     ap;
	std::map<std::string, otel_attribute_value> attr{};
	otel_system_timestamp                       timestamp = std::chrono::system_clock::now();
	int                                         retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", %p, %p, ...", span, OTELC_STR_ARG(name), ts_system, kv);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(name))
		OTEL_SPAN_ERETURN_INT("Invalid event name");
	else if (OTEL_NULL(kv))
		OTEL_SPAN_ERETURN_INT("Invalid event key-value");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	if (!OTEL_NULL(ts_system))
		timestamp = otel_system_timestamp(timespec_to_duration(ts_system));

	/* Iterate over the variadic kv pairs and add each to the event. */
	OTEL_VA_AUTO(ap, kv);
	for (retval = 0; !OTEL_NULL(kv); retval++) {
		if (otel_span_add_one_event(span, attr, kv->key, &(kv->value)) == OTELC_RET_ERROR)
			OTEL_SPAN_ERETURN_INT("Unable to set span event attribute");

		kv = va_arg(ap, typeof(kv));
	}

	if (retval > 0)
		handle->span->AddEvent(otel_nostd::string_view{name}, timestamp, attr);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_span_add_event_kv_n - adds an event to the span
 *
 * SYNOPSIS
 *   static int otel_span_add_event_kv_n(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const struct otelc_kv *kv, size_t kv_len)
 *
 * ARGUMENTS
 *   span      - span instance
 *   name      - name of the event being added
 *   ts_system - time of the event being added
 *   kv        - an array of key-value pairs of attributes to be set
 *   kv_len    - size of key-value pair array
 *
 * DESCRIPTION
 *   Adds an event to the span.  An event can be customized with a timestamp
 *   and a set of attributes, which are key-value pairs providing additional
 *   information.
 *
 * RETURN VALUE
 *   Returns the number of attributes that the added event contains,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_add_event_kv_n(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const struct otelc_kv *kv, size_t kv_len)
{
	OTEL_LOCK_TRACER(span);
	std::map<std::string, otel_attribute_value> attr{};
	otel_system_timestamp                       timestamp = std::chrono::system_clock::now();
	int                                         retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", %p, %p, %zu", span, OTELC_STR_ARG(name), ts_system, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(name))
		OTEL_SPAN_ERETURN_INT("Invalid event name");
	else if (OTEL_NULL(kv))
		OTEL_SPAN_ERETURN_INT("Invalid event key-value");
	else if (kv_len == 0)
		OTEL_SPAN_ERETURN_INT("Invalid event key-value array size");

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

	if (!OTEL_NULL(ts_system))
		timestamp = otel_system_timestamp(timespec_to_duration(ts_system));

	/* Iterate over the kv array and add each to the event. */
	for (retval = 0; retval < OTEL_CAST_STATIC(int, kv_len); retval++)
		if (otel_span_add_one_event(span, attr, kv[retval].key, &(kv[retval].value)) == OTELC_RET_ERROR)
			OTEL_SPAN_ERETURN_INT("Unable to set span event attribute");

	if (retval > 0)
		handle->span->AddEvent(otel_nostd::string_view{name}, timestamp, attr);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_span_add_link - adds a link to the span
 *
 * SYNOPSIS
 *   static int otel_span_add_link(const struct otelc_span *span, const struct otelc_span *link_span, const struct otelc_span_context *link_context, const struct otelc_kv *kv, size_t kv_len)
 *
 * ARGUMENTS
 *   span         - span instance
 *   link_span    - span to link to
 *   link_context - span context to link to
 *   kv           - an array of key-value pairs of attributes for the link
 *   kv_len       - size of key-value pair array
 *
 * DESCRIPTION
 *   Adds a link to another span, identified by its span context.  The link can
 *   be specified either by a span instance (link_span) or by a span context
 *   (link_context), but not both.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_add_link(const struct otelc_span *span, const struct otelc_span *link_span, const struct otelc_span_context *link_context, const struct otelc_kv *kv, size_t kv_len)
{
	OTEL_LOCK_TRACER(span);

	OTELC_FUNC("%p, %p, %p, %p, %zu", span, link_span, link_context, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle))
		OTEL_SPAN_ERETURN_INT("Invalid span");

#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
	auto                                        target = otel_trace::SpanContext::GetInvalid();
	std::map<std::string, otel_attribute_value> attribute{};

	if (!OTEL_NULL(link_span) && !OTEL_NULL(link_context))
		OTEL_SPAN_ERETURN_INT("Parameters link_span and link_context are mutually exclusive");

	if (OTEL_NULL(link_span) && OTEL_NULL(link_context))
		OTEL_SPAN_ERETURN_INT("One of link_span or link_context must be specified");

	if (!OTEL_NULL(kv) && (kv_len > 0))
		for (size_t i = 0; i < kv_len; ++i)
			OTEL_VALUE_ADD(_INT, attribute, emplace, kv[i].key, &(kv[i].value), &(span->tracer->err), "Unable to add link");

	/* Resolve the link target from either a span or a span context. */
	if (!OTEL_NULL(link_span)) {
		const auto link_handle = OTEL_SPAN_HANDLE(link_span);
		if (OTEL_NULL(link_handle))
			OTEL_SPAN_ERETURN_INT("Invalid linked span");

		target = link_handle->span->GetContext();
	} else {
		const auto context_handle = OTEL_SPAN_CONTEXT_HANDLE(link_context);
		if (OTEL_NULL(context_handle))
			OTEL_SPAN_ERETURN_INT("Invalid linked span context");

		target = otel_trace::GetSpan(*(context_handle->context))->GetContext();
	}

	/* Add the resolved link to the span. */
	if (target.IsValid())
		handle->span->AddLink(target, attribute);
	else
		OTEL_SPAN_ERETURN_INT("Invalid linked span context");

	OTELC_RETURN_INT(OTELC_RET_OK);
#else
	OTEL_SPAN_ERETURN_INT("AddLink is not supported in this OpenTelemetry ABI version");
#endif /* OPENTELEMETRY_ABI_VERSION_NO && (OPENTELEMETRY_ABI_VERSION_NO >= 2) */
}


/***
 * NAME
 *   otel_nolock_span_destroy - destroys all references related to the specified span
 *
 * SYNOPSIS
 *   void otel_nolock_span_destroy(struct otelc_span **span)
 *
 * ARGUMENTS
 *   span - span instance
 *
 * DESCRIPTION
 *   This is the non-locking version of the otel_span_destroy() function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_nolock_span_destroy(struct otelc_span **span)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(span));

	if (OTEL_NULL(span) || OTEL_NULL(*span))
		OTELC_RETURN();

	/* Look up the span handle, delete it, and erase it from the map. */
	const auto handle = OTEL_SPAN_HANDLE(*span);
	if (!OTEL_NULL(handle)) {
		delete handle;

		OTEL_HANDLE(otel_span, map).erase((*span)->idx);
		OTEL_HANDLE(otel_span, erase_cnt++);

		OTELC_DBG(OTEL, "otel_span[%" PRId64 "] erased", (*span)->idx);
	} else {
		OTELC_DBG(OTEL, "invalid otel_span[%" PRId64 "]", (*span)->idx);
	}

	OTEL_HANDLE(otel_span, destroy_cnt++);
	OTEL_DBG_SPAN();

	OTEL_EXT_FREE_CLEAR(*span);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_span_destroy - destroys all references related to the specified span
 *
 * SYNOPSIS
 *   static void otel_span_destroy(struct otelc_span **span)
 *
 * ARGUMENTS
 *   span - span instance
 *
 * DESCRIPTION
 *   Destroying all references associated with a specific span, as well as
 *   deleting the span from the span array defined in the otel_span handle.
 *   After this function is executed, all data related to the span is deleted.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_span_destroy(struct otelc_span **span)
{
	OTEL_LOCK_TRACER(span);

	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(span));

	if (OTEL_NULL(span) || OTEL_NULL(*span))
		OTELC_RETURN();

	otel_nolock_span_destroy(span);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_span_new - creates a new span instance
 *
 * SYNOPSIS
 *   struct otelc_span *otel_span_new(struct otelc_tracer *tracer)
 *
 * ARGUMENTS
 *   tracer - tracer instance
 *
 * DESCRIPTION
 *   A new span instance is created, i.e. memory is allocated for the data
 *   structure otelc_span which is used for span manipulation.
 *
 * RETURN VALUE
 *   Returns the memory address assigned to the span instance, or nullptr in
 *   case of an error.
 */
struct otelc_span *otel_span_new(struct otelc_tracer *tracer)
{
	const static struct otelc_span span_init = {
		.idx                  = 0,
		.tracer               = nullptr,
		.get_id               = otel_span_get_id,               /* lock span */
		.is_recording         = otel_span_is_recording,         /* lock span */
		.end                  = otel_span_end,                  /* lock span */
		.end_with_options     = otel_span_end_with_options,     /* lock span */
		.set_operation_name   = otel_span_set_operation_name,   /* lock span */
		.set_baggage_var      = otel_span_set_baggage_var,      /* lock span */
		.set_baggage_kv_var   = otel_span_set_baggage_kv_var,   /* lock span */
		.set_baggage_kv_n     = otel_span_set_baggage_kv_n,     /* lock span */
		.get_baggage          = otel_span_get_baggage,          /* lock span */
		.get_baggage_var      = otel_span_get_baggage_var,      /* lock span */
		.set_attribute_var    = otel_span_set_attribute_var,    /* lock span */
		.set_attribute_kv_var = otel_span_set_attribute_kv_var, /* lock span */
		.set_attribute_kv_n   = otel_span_set_attribute_kv_n,   /* lock span */
		.add_event_var        = otel_span_add_event_var,        /* lock span */
		.add_event_kv_var     = otel_span_add_event_kv_var,     /* lock span */
		.add_event_kv_n       = otel_span_add_event_kv_n,       /* lock span */
		.add_link             = otel_span_add_link,             /* lock span */
		.set_status           = otel_span_set_status,           /* lock span */
		.inject_text_map      = otel_span_inject_text_map,      /* lock span */
		.inject_http_headers  = otel_span_inject_http_headers,  /* lock span */
		.destroy              = otel_span_destroy,              /* lock span */
	};
	struct otelc_span *retptr = nullptr;

	OTELC_FUNC("%p", tracer);

	if (OTEL_NULL(tracer))
		OTELC_RETURN_PTR(retptr);

	if (!OTEL_NULL(retptr = OTEL_CAST_TYPEOF(retptr, OTEL_EXT_MALLOC(sizeof(*retptr))))) {
		(void)memcpy(retptr, &span_init, sizeof(*retptr));

		retptr->tracer = tracer;
		retptr->idx    = OTEL_HANDLE(otel_span, id++);
	} else {
		OTEL_HANDLE(otel_span, alloc_fail_cnt++);
	}

	OTELC_DBG_SPAN(OTEL, "span", retptr);

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otel_span_context_get_id - gets span context identifiers
 *
 * SYNOPSIS
 *   static int otel_span_context_get_id(const struct otelc_span_context *context, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
 *
 * ARGUMENTS
 *   context       - instance of span context
 *   span_id       - buffer to store the span ID
 *   span_id_size  - size of the span ID buffer
 *   trace_id      - buffer to store the trace ID
 *   trace_id_size - size of the trace ID buffer
 *   trace_flags   - buffer to store the trace flags
 *
 * DESCRIPTION
 *   Retrieves the span identifier, trace identifier, and trace flags from the
 *   span context.  Any of the output pointers can be null if that specific
 *   identifier is not needed.  The provided buffers for span_id and trace_id
 *   must be large enough to hold the respective identifiers.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_get_id(const struct otelc_span_context *context, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p, %p, %zu, %p, %zu, %p", context, span_id, span_id_size, trace_id, trace_id_size, trace_flags);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	/* Retrieve and validate the span context from the handle. */
	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	if (!span_ctx.IsValid()) {
		OTELC_DBG(OTEL, "invalid span context for otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	/* Copy the span ID, trace ID, and trace flags to the caller's buffers. */
	if (!OTEL_NULL(span_id) && (span_id_size >= otel_trace::SpanId::kSize))
		span_ctx.span_id().CopyBytesTo(otel_nostd::span<uint8_t, otel_trace::SpanId::kSize>(span_id, otel_trace::SpanId::kSize));
	if (!OTEL_NULL(trace_id) && (trace_id_size >= otel_trace::TraceId::kSize))
		span_ctx.trace_id().CopyBytesTo(otel_nostd::span<uint8_t, otel_trace::TraceId::kSize>(trace_id, otel_trace::TraceId::kSize));
	if (!OTEL_NULL(trace_flags))
		span_ctx.trace_flags().CopyBytesTo(otel_nostd::span<uint8_t, 1>(trace_flags, 1));

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_span_context_is_valid - checks whether the span context is valid
 *
 * SYNOPSIS
 *   static int otel_span_context_is_valid(const struct otelc_span_context *context)
 *
 * ARGUMENTS
 *   context - instance of span context
 *
 * DESCRIPTION
 *   Queries the span context to determine whether it contains valid trace and
 *   span identifiers.  A span context is valid when both its trace ID and span
 *   ID are non-zero.
 *
 * RETURN VALUE
 *   Returns true if the span context is valid, false if it is not,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_is_valid(const struct otelc_span_context *context)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p", context);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();

	OTELC_RETURN_INT(span_ctx.IsValid() ? true : false);
}


/***
 * NAME
 *   otel_span_context_is_sampled - checks whether the span context is sampled
 *
 * SYNOPSIS
 *   static int otel_span_context_is_sampled(const struct otelc_span_context *context)
 *
 * ARGUMENTS
 *   context - instance of span context
 *
 * DESCRIPTION
 *   Queries the span context to determine whether the sampled flag is set in
 *   the trace flags.  A sampled span context indicates that the trace data
 *   should be exported.
 *
 * RETURN VALUE
 *   Returns true if the span context is sampled, false if it is not,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_is_sampled(const struct otelc_span_context *context)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p", context);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();

	OTELC_RETURN_INT(span_ctx.IsSampled() ? true : false);
}


/***
 * NAME
 *   otel_span_context_is_remote - checks whether the span context was propagated from a remote parent
 *
 * SYNOPSIS
 *   static int otel_span_context_is_remote(const struct otelc_span_context *context)
 *
 * ARGUMENTS
 *   context - instance of span context
 *
 * DESCRIPTION
 *   Queries the span context to determine whether it was propagated from a
 *   remote parent service.  A remote span context is one that was extracted
 *   from a carrier (e.g. HTTP headers or text map) rather than created
 *   locally.
 *
 * RETURN VALUE
 *   Returns true if the span context is remote, false if it is not,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_is_remote(const struct otelc_span_context *context)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p", context);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();

	OTELC_RETURN_INT(span_ctx.IsRemote() ? true : false);
}


/***
 * NAME
 *   otel_span_context_trace_state_get - gets a trace state value by key
 *
 * SYNOPSIS
 *   static int otel_span_context_trace_state_get(const struct otelc_span_context *context, const char *key, char *value, size_t value_size)
 *
 * ARGUMENTS
 *   context    - instance of span context
 *   key        - trace state key to look up
 *   value      - buffer to store the value
 *   value_size - size of the value buffer
 *
 * DESCRIPTION
 *   Retrieves the value associated with a key in the W3C trace state embedded
 *   in the span context.  Follows the snprintf convention: writes up to
 *   value_size-1 characters plus a NUL terminator when value_size is greater
 *   than zero, and always returns the total length of the value regardless of
 *   the buffer size.  The value and value_size arguments can be zero/null when
 *   only the length is needed.
 *
 * RETURN VALUE
 *   Returns the length of the value (excluding NUL) if the key is found, 0 if
 *   the key is not present, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_trace_state_get(const struct otelc_span_context *context, const char *key, char *value, size_t value_size)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p, \"%s\", %p, %zu", context, OTELC_STR_ARG(key), value, value_size);

	if (OTEL_NULL(context) || OTEL_NULL(key))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	/* Retrieve the trace state and look up the requested key. */
	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();
	std::string value_str;

	if (!ts->Get(key, value_str))
		OTELC_RETURN_INT(0);

	const auto len = OTEL_CAST_STATIC(int, value_str.length());

	if (!OTEL_NULL(value) && (value_size > 0))
		(void)otelc_strlcpy(value, value_size, value_str.c_str(), value_str.length());

	OTELC_RETURN_INT(len);
}


/***
 * NAME
 *   otel_span_context_trace_state_entries - gets all trace state key-value pairs
 *
 * SYNOPSIS
 *   static int otel_span_context_trace_state_entries(const struct otelc_span_context *context, struct otelc_text_map *text_map)
 *
 * ARGUMENTS
 *   context  - instance of span context
 *   text_map - text map to fill with trace state entries
 *
 * DESCRIPTION
 *   Iterates over all key-value pairs in the W3C trace state and appends them
 *   to the provided text map.  The caller is responsible for freeing the text
 *   map contents with otelc_text_map_destroy() when done.
 *
 * RETURN VALUE
 *   Returns the number of entries added to the text map, or OTELC_RET_ERROR in
 *   case of an error.
 */
static int otel_span_context_trace_state_entries(const struct otelc_span_context *context, struct otelc_text_map *text_map)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p, %p", context, text_map);

	if (OTEL_NULL(context) || OTEL_NULL(text_map))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	/* Iterate over all trace state entries and copy them to the text map. */
	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();
	int count = 0;

	ts->GetAllEntries([&](otel_nostd::string_view key, otel_nostd::string_view val) -> bool {
		if (OTELC_TEXT_MAP_ADD(text_map, key.data(), key.size(), val.data(), val.size(), OTELC_TEXT_MAP_AUTO) != OTELC_RET_ERROR)
			count++;

		return true;
	});

	OTELC_RETURN_INT(count);
}


/***
 * NAME
 *   otel_trace_state_copy_header - copies a trace state header string into a buffer
 *
 * SYNOPSIS
 *   static inline void otel_trace_state_copy_header(const std::string &str, char *header, size_t header_size)
 *
 * ARGUMENTS
 *   str         - the header string to copy
 *   header      - destination buffer, or NULL if only the length is needed
 *   header_size - size of the destination buffer
 *
 * DESCRIPTION
 *   Copies a serialized W3C trace state header string into a caller-provided
 *   buffer.  Follows the snprintf convention: writes up to header_size-1
 *   characters plus a NUL terminator when header_size is greater than zero.
 *   Does nothing if header is NULL or header_size is zero.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static inline void otel_trace_state_copy_header(const std::string &str, char *header, size_t header_size)
{
	if (OTEL_NULL(header) || (header_size == 0))
		/* Do nothing. */;
	else if (str.empty())
		*header = '\0';
	else
		(void)otelc_strlcpy(header, header_size, str.c_str(), str.length());
}


/***
 * NAME
 *   otel_span_context_trace_state_header - serializes the trace state to a W3C header string
 *
 * SYNOPSIS
 *   static int otel_span_context_trace_state_header(const struct otelc_span_context *context, char *header, size_t header_size)
 *
 * ARGUMENTS
 *   context     - instance of span context
 *   header      - buffer to store the serialized header
 *   header_size - size of the header buffer
 *
 * DESCRIPTION
 *   Serializes the W3C trace state to its header string representation.
 *   Follows the snprintf convention: writes up to header_size-1 characters plus
 *   a NUL terminator when header_size is greater than zero, and always returns
 *   the total length of the header regardless of the buffer size.  The header
 *   and header_size arguments can be zero/null when only the length is needed.
 *
 * RETURN VALUE
 *   Returns the length of the header string (excluding NUL), 0 if the trace
 *   state is empty, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_trace_state_header(const struct otelc_span_context *context, char *header, size_t header_size)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p, %p, %zu", context, header, header_size);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	/* Serialize the trace state to a W3C header string. */
	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();

	const auto header_str = ts->ToHeader();
	otel_trace_state_copy_header(header_str, header, header_size);

	OTELC_RETURN_INT(header_str.length());
}


/***
 * NAME
 *   otel_span_context_trace_state_empty - checks whether the trace state is empty
 *
 * SYNOPSIS
 *   static int otel_span_context_trace_state_empty(const struct otelc_span_context *context)
 *
 * ARGUMENTS
 *   context - instance of span context
 *
 * DESCRIPTION
 *   Queries the span context to determine whether its W3C trace state contains
 *   any key-value pairs.
 *
 * RETURN VALUE
 *   Returns true if the trace state is empty, false if it is not,
 *   or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_trace_state_empty(const struct otelc_span_context *context)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p", context);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	/* Query whether the trace state contains any entries. */
	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();

	OTELC_RETURN_INT(ts->Empty() ? true : false);
}


/***
 * NAME
 *   otel_span_context_trace_state_set - sets a key-value pair in the trace state
 *
 * SYNOPSIS
 *   static int otel_span_context_trace_state_set(const struct otelc_span_context *context, const char *key, const char *value, char *header, size_t header_size)
 *
 * ARGUMENTS
 *   context     - instance of span context
 *   key         - trace state key to set
 *   value       - trace state value to set
 *   header      - buffer to store the resulting W3C header string
 *   header_size - size of the header buffer
 *
 * DESCRIPTION
 *   Creates a modified copy of the W3C trace state with the given key-value
 *   pair added or updated, then serializes it to a W3C header string.  The
 *   original trace state in the span context is not modified.  Follows the
 *   snprintf convention: writes up to header_size-1 characters plus a NUL
 *   terminator when header_size is greater than zero, and always returns the
 *   total length of the header regardless of the buffer size.  The header and
 *   header_size arguments can be zero/null when only the length is needed.
 *
 * RETURN VALUE
 *   Returns the length of the resulting header string (excluding NUL), 0 if
 *   the trace state is empty, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_trace_state_set(const struct otelc_span_context *context, const char *key, const char *value, char *header, size_t header_size)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p, \"%s\", \"%s\", %p, %zu", context, OTELC_STR_ARG(key), OTELC_STR_ARG(value), header, header_size);

	if (OTEL_NULL(context) || OTEL_NULL(key) || OTEL_NULL(value))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	/* Set the key-value pair in the trace state and serialize the result. */
	const auto  span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();
	auto        new_ts = ts->Set(key, value);

	const auto header_str = new_ts->ToHeader();
	otel_trace_state_copy_header(header_str, header, header_size);

	OTELC_RETURN_INT(header_str.length());
}


/***
 * NAME
 *   otel_span_context_trace_state_delete - removes a key from the trace state
 *
 * SYNOPSIS
 *   static int otel_span_context_trace_state_delete(const struct otelc_span_context *context, const char *key, char *header, size_t header_size)
 *
 * ARGUMENTS
 *   context     - instance of span context
 *   key         - trace state key to remove
 *   header      - buffer to store the resulting W3C header string
 *   header_size - size of the header buffer
 *
 * DESCRIPTION
 *   Creates a modified copy of the W3C trace state with the given key removed,
 *   then serializes it to a W3C header string.  The original trace state in
 *   the span context is not modified.  Follows the snprintf convention: writes
 *   up to header_size-1 characters plus a NUL terminator when header_size is
 *   greater than zero, and always returns the total length of the header
 *   regardless of the buffer size.  The header and header_size arguments can
 *   be zero/null when only the length is needed.
 *
 * RETURN VALUE
 *   Returns the length of the resulting header string (excluding NUL), 0 if
 *   the trace state is empty, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_trace_state_delete(const struct otelc_span_context *context, const char *key, char *header, size_t header_size)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p, \"%s\", %p, %zu", context, OTELC_STR_ARG(key), header, header_size);

	if (OTEL_NULL(context) || OTEL_NULL(key))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(context);
	if (OTEL_NULL(handle)) {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", context->idx);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	/* Remove the key from the trace state and serialize the result. */
	const auto  span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();
	auto        new_ts = ts->Delete(key);

	const auto header_str = new_ts->ToHeader();
	otel_trace_state_copy_header(header_str, header, header_size);

	OTELC_RETURN_INT(header_str.length());
}


/***
 * NAME
 *   otel_nolock_span_context_destroy - destroys a span context handle
 *
 * SYNOPSIS
 *   void otel_nolock_span_context_destroy(struct otelc_span_context **context)
 *
 * ARGUMENTS
 *   context - instance of span context
 *
 * DESCRIPTION
 *   This is the non-locking version of the otel_span_context_destroy()
 *   function.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_nolock_span_context_destroy(struct otelc_span_context **context)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(context));

	if (OTEL_NULL(context) || OTEL_NULL(*context))
		OTELC_RETURN();

	/* Look up the context handle, delete it, and erase it from the map. */
	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(*context);
	if (!OTEL_NULL(handle)) {
		delete handle;

		OTEL_HANDLE(otel_span_context, map).erase((*context)->idx);
		OTEL_HANDLE(otel_span_context, erase_cnt++);

		OTELC_DBG(OTEL, "otel_span_context[%" PRId64 "] erased", (*context)->idx);
	} else {
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", (*context)->idx);
	}

	OTEL_HANDLE(otel_span_context, destroy_cnt++);
	OTEL_DBG_SPAN_CONTEXT();

	OTEL_EXT_FREE_CLEAR(*context);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_span_context_destroy - destroys a span context handle
 *
 * SYNOPSIS
 *   static void otel_span_context_destroy(struct otelc_span_context **context)
 *
 * ARGUMENTS
 *   context - address of a pointer to the span context instance to be destroyed
 *
 * DESCRIPTION
 *   Destroys the specified span context handle.  This involves removing the
 *   handle from the internal map and freeing the memory associated with the
 *   C-style otelc_span_context structure.  The underlying C++ context object
 *   is managed by a shared pointer and will be released when its reference
 *   count drops to zero.  After this function, the *context pointer is set
 *   to null.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_span_context_destroy(struct otelc_span_context **context)
{
	OTEL_LOCK_TRACER(span_context);

	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(context));

	if (OTEL_NULL(context) || OTEL_NULL(*context))
		OTELC_RETURN();

	otel_nolock_span_context_destroy(context);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_span_context_new - creates a new span context instance
 *
 * SYNOPSIS
 *   struct otelc_span_context *otel_span_context_new(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   A new span context instance is created, i.e. memory is allocated for
 *   the data structure otelc_span_context which is used for span context
 *   manipulation.
 *
 * RETURN VALUE
 *   Returns the memory address assigned to the span context instance,
 *   or nullptr in case of an error.
 */
struct otelc_span_context *otel_span_context_new(void)
{
	const static struct otelc_span_context span_context_init = {
		.idx                 = 0,
		.is_valid            = otel_span_context_is_valid,            /* lock span_context */
		.is_sampled          = otel_span_context_is_sampled,          /* lock span_context */
		.is_remote           = otel_span_context_is_remote,           /* lock span_context */
		.get_id              = otel_span_context_get_id,              /* lock span_context */
		.trace_state_get     = otel_span_context_trace_state_get,     /* lock span_context */
		.trace_state_entries = otel_span_context_trace_state_entries, /* lock span_context */
		.trace_state_header  = otel_span_context_trace_state_header,  /* lock span_context */
		.trace_state_empty   = otel_span_context_trace_state_empty,   /* lock span_context */
		.trace_state_set     = otel_span_context_trace_state_set,     /* lock span_context */
		.trace_state_delete  = otel_span_context_trace_state_delete,  /* lock span_context */
		.destroy             = otel_span_context_destroy,             /* lock span_context */
	};
	struct otelc_span_context *retptr = nullptr;

	OTELC_FUNC("");

	if (!OTEL_NULL(retptr = OTEL_CAST_TYPEOF(retptr, OTEL_EXT_MALLOC(sizeof(*retptr))))) {
		(void)memcpy(retptr, &span_context_init, sizeof(*retptr));

		retptr->idx = OTEL_HANDLE(otel_span_context, id++);
	} else {
		OTEL_HANDLE(otel_span_context, alloc_fail_cnt++);
	}

	OTELC_DBG_SPAN_CONTEXT(OTEL, "context", retptr);

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_span_context_create - creates a span context from raw identifiers
 *
 * SYNOPSIS
 *   struct otelc_span_context *otelc_span_context_create(const uint8_t *trace_id, size_t trace_id_size, const uint8_t *span_id, size_t span_id_size, uint8_t trace_flags, int is_remote, const char *trace_state_header, char **err)
 *
 * ARGUMENTS
 *   trace_id           - 16-byte trace identifier
 *   trace_id_size      - the size of the trace identifier buffer
 *   span_id            - 8-byte span identifier
 *   span_id_size       - the size of the span identifier buffer
 *   trace_flags        - the W3C trace flags byte (e.g. 0x01 for sampled)
 *   is_remote          - set to true if propagated from a remote parent
 *   trace_state_header - optional W3C tracestate header string, or NULL
 *   err                - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Constructs a new span context from raw trace_id, span_id, and trace_flags
 *   bytes.  This allows callers to link to external spans or restore context
 *   from storage without round-tripping through text map propagation.  The
 *   optional trace_state_header argument is parsed as a W3C tracestate header
 *   string.
 *
 * RETURN VALUE
 *   Returns a pointer to a newly created span context on success, or nullptr
 *   on failure.
 */
struct otelc_span_context *otelc_span_context_create(const uint8_t *trace_id, size_t trace_id_size, const uint8_t *span_id, size_t span_id_size, uint8_t trace_flags, int is_remote, const char *trace_state_header, char **err)
{
	OTEL_LOCK_TRACER(span_context);
	uint8_t                    tid[otel_trace::TraceId::kSize] = { 0 };
	uint8_t                    sid[otel_trace::SpanId::kSize]  = { 0 };
	struct otelc_span_context *retptr = nullptr;

	OTELC_FUNC("%p, %zu, %p, %zu, 0x%02x, %d, \"%s\", %p:%p", trace_id, trace_id_size, span_id, span_id_size, trace_flags, is_remote, OTELC_STR_ARG(trace_state_header), OTELC_DPTR_ARGS(err));

	/* Copy raw ID bytes into fixed-size arrays. */
	if (!OTEL_NULL(trace_id) && (trace_id_size >= sizeof(tid)))
		(void)memcpy(tid, trace_id, sizeof(tid));
	if (!OTEL_NULL(span_id) && (span_id_size >= sizeof(sid)))
		(void)memcpy(sid, span_id, sizeof(sid));

	/* Parse optional trace state. */
	auto ts = OTEL_NULL(trace_state_header) ? otel_trace::TraceState::GetDefault() : otel_trace::TraceState::FromHeader(trace_state_header);

	/* Construct C++ SpanContext. */
	otel_trace::SpanContext span_ctx(otel_trace::TraceId{tid}, otel_trace::SpanId{sid}, otel_trace::TraceFlags{trace_flags}, is_remote != false, ts);

	/* Wrap in a DefaultSpan and Context. */
	otel_nostd::shared_ptr<otel_trace::Span> default_span(new(std::nothrow) otel_trace::DefaultSpan(span_ctx));
	if (OTEL_NULL(default_span))
		OTEL_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("default span"));

	otel_context::Context empty_ctx{};
	auto context = otel::make_shared_nothrow<otel_context::Context>(otel_trace::SetSpan(empty_ctx, default_span));
	if (OTEL_NULL(context))
		OTEL_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("span context"));

	/* Allocate C handle. */
	if (OTEL_NULL(retptr = otel_span_context_new()))
		OTEL_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("span context handle"));

	const auto span_context_handle = new(std::nothrow) otel_span_context_handle{std::move(context)};
	if (OTEL_NULL(span_context_handle)) {
		otel_nolock_span_context_destroy(&retptr);

		OTEL_ERETURN_PTR(OTEL_ERROR_MSG_ENOMEM("span context handle"));
	}

	/* Register the span context handle in the shared map. */
	std::pair<std::unordered_map<int64_t, struct otel_span_context_handle *>::iterator, bool> emplace_status{};
	try {
		OTEL_DBG_THROW();
		emplace_status = OTEL_HANDLE(otel_span_context, map).emplace(retptr->idx, span_context_handle);

		if (!emplace_status.second) {
			delete span_context_handle;

			otel_nolock_span_context_destroy(&retptr);

			OTEL_ERETURN_PTR("Unable to add span context: duplicate id");
		}
	}
	OTEL_CATCH_ERETURN({
		if (emplace_status.second)
			OTEL_HANDLE(otel_span_context, map).erase(retptr->idx);

		delete span_context_handle;
		otel_nolock_span_context_destroy(&retptr);
		}, OTEL_ERETURN_PTR, "Unable to add span context"
	)

	OTEL_HANDLE(otel_span_context, peak_size) = std::max(OTEL_HANDLE(otel_span_context, peak_size), OTEL_HANDLE(otel_span_context, map).size());
	OTEL_DBG_SPAN_CONTEXT();

	OTELC_RETURN_PTR(retptr);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
