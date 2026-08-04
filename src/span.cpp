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
THREAD_LOCAL struct otel_handle<struct otel_span_handle *, OTEL_HANDLE_SHARED>          otel_span{otel_handle_map_shards.load()};
THREAD_LOCAL struct otel_handle<struct otel_span_context_handle *, OTEL_HANDLE_SHARED>  otel_span_context{otel_handle_map_shards.load()};
#else
THREAD_LOCAL struct otel_handle<struct otel_span_handle *, OTEL_HANDLE_SHARED>         *otel_span = nullptr;
THREAD_LOCAL struct otel_handle<struct otel_span_context_handle *, OTEL_HANDLE_SHARED> *otel_span_context = nullptr;
#endif


/***
 * NAME
 *   otel_span_err - returns the target for span error reporting
 *
 * SYNOPSIS
 *   char **otel_span_err(const struct otelc_span *span)
 *
 * ARGUMENTS
 *   span - span instance
 *
 * DESCRIPTION
 *   Returns the address of the owning tracer's error string, the target that the
 *   span error macros write through.  In a debug build the tracer's liveness
 *   marker is probed first: when the tracer was already destroyed, the access is
 *   reported loudly and the error text is diverted into a sacrificial slot of
 *   its own instead of the freed tracer structure.  The probe reads freed memory
 *   and is best-effort by design, mirroring the accepted risk of the allocator
 *   magic probe in src/dbg_malloc.cpp.  The last message written to that slot is
 *   still allocated when the thread ends, so the tracker reports it as a leak;
 *   that is accepted, since the slot is only ever reached by a caller that
 *   already uses a destroyed tracer.
 *
 * RETURN VALUE
 *   Returns the address of the error-string pointer to write through.
 */
char **otel_span_err(const struct otelc_span *span)
{
#ifdef OTELC_DBG_MEM
	/* Deliberately not THREAD_LOCAL: the slot must stay per thread in
	 * every build, including the one where that macro expands to nothing. */
	static thread_local char *dead_slot = nullptr;

	if (OTEL_NULL(span->tracer) || OTEL_TRACER_DEAD(span->tracer)) {
		OTELC_DBG(ERROR, "SPAN_ERROR: %p: tracer %p is destroyed or invalid", span, span->tracer);

		return &dead_slot;
	}
#endif

	return &(span->tracer->err);
}


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
 *   to hold the respective identifiers.  A buffer smaller than its identifier
 *   is skipped silently: the buffer is left unmodified and the function still
 *   reports success.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_get_id(const struct otelc_span *span, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
{
	OTELC_FUNC("%p, %p, %zu, %p, %zu, %p", span, span_id, span_id_size, trace_id, trace_id_size, trace_flags);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	const auto span_ctx = handle->span->GetContext();
	if (!span_ctx.IsValid())
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_SPAN_CTX);

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
	OTELC_FUNC("%p", span);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	OTELC_RETURN_INT(handle->span->IsRecording());
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
	OTELC_FUNC("%p, %d, \"%s\"", span, status, OTELC_STR_ARG(desc));

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (!OTELC_IN_RANGE(status, OTELC_SPAN_STATUS_UNSET, OTELC_SPAN_STATUS_ERROR))
		OTEL_SPAN_RETURN_INT("Invalid span status: %d", status);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

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
 *   map and HTTP headers injection.  Any content left in the writer's text
 *   map by a previous inject is released before the new content is stored.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK if the context is injected into the carrier,
 *   or OTELC_RET_ERROR in case of an error.
 */
template <template <typename> class C, typename W>
static int otel_span_inject_carrier(const struct otelc_span *span, W *carrier, const char *carrier_name, const char *dump_label __maybe_unused)
{
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
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_CARRIER);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	/* Inject the span context into the carrier via the per-tracer propagator. */
	auto *impl = (OTEL_NULL(span->tracer) || OTEL_TRACER_DEAD(span->tracer)) ? nullptr : OTEL_CAST_STATIC(struct otel_tracer_impl *, span->tracer->impl);
	if (OTEL_NULL(impl) || OTEL_NULL(impl->propagator))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_NO_PROPAGATOR);
	/* Snapshot the propagator reference for the duration of the call. */
	auto propagator = impl->propagator;
	propagator->Inject(otel_carrier, *(handle->context));

#ifndef OTELC_USE_COMPOSITE_PROPAGATOR
	/* Without a composite propagator, baggage is injected separately. */
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
		OTEL_SPAN_RETURN_INT("No injected data");

	/* Release the content a previous inject left in the writer's map. */
	otelc_text_map_free(&(carrier->text_map));

	/***
	 * The map is pre-sized even when the writer supplies a set callback:
	 * the known callbacks store their entries into this same text map, so
	 * the exact-size allocation spares them the incremental growth steps.
	 */
	if (OTEL_NULL(OTELC_TEXT_MAP_NEW(&(carrier->text_map), otel_carrier.data().size())))
		OTEL_SPAN_RETURN_INT("Unable to allocate memory for %s carrier", carrier_name);

	/* Copy injected data from the internal carrier to the caller's carrier. */
	for (const auto &it : otel_carrier.data()) {
		int rc = OTELC_RET_OK;

		if (!OTEL_NULL(carrier->set))
			rc = carrier->set(carrier, it.first.c_str(), it.second.c_str());
		else
			rc = OTELC_TEXT_MAP_ADD(&(carrier->text_map), it.first.c_str(), 0, it.second.c_str(), 0, OTELC_TEXT_MAP_AUTO);

		if (rc == OTELC_RET_ERROR) {
			otelc_text_map_free(&(carrier->text_map));
			OTEL_SPAN_RETURN_INT("Unable to copy injected data to %s carrier", carrier_name);
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
 *   instance.  The span handle is destroyed and the *span pointer is set to
 *   null.  The optional <ts_steady> argument sets the end time of the span.
 *   <status> is used to set the status of the span and its setting can be
 *   avoided if OTELC_SPAN_STATUS_IGNORE is used as the argument.  <desc> is
 *   used as a text description that can be set for the span status.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_span_end_with_options(struct otelc_span **span, const struct timespec *ts_steady, otelc_span_status_t status, const char *desc)
{
	OTELC_FUNC("%p:%p, %p, %d, \"%s\"", OTELC_DPTR_ARGS(span), ts_steady, status, OTELC_STR_ARG(desc));

	if (OTEL_NULL(span) || OTEL_NULL(*span))
		OTELC_RETURN();

#ifndef OTELC_USE_STATIC_HANDLE
	/* The maps are gone: skip the shard lock, the nolock path frees the structure. */
	if (OTEL_NULL(otel_span)) {
		otel_nolock_span_destroy(span);

		OTELC_RETURN();
	}
#endif

	OTEL_LOCK_TRACER(span, (*span)->idx);

	/* End the span with optional status and steady-clock timestamp. */
	const auto handle = OTEL_SPAN_HANDLE(*span);
	if (!OTEL_NULL(handle)) {
		otel_trace::EndSpanOptions end_options{};

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
 *   instance.  The span handle is destroyed and the *span pointer is set to
 *   null.  This function calls the function otel_span_end_with_options(),
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
	OTELC_FUNC("%p, \"%s\"", span, OTELC_STR_ARG(operation_name));

	if (OTEL_NULL(span))
		OTELC_RETURN();
	else if (!OTELC_STR_IS_VALID(operation_name))
		OTEL_SPAN_RETURN(OTEL_ERROR_MSG_INVALID_OP_NAME);

	OTEL_LOCK_SPAN_HANDLE( , span);

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
 *   string, with the restriction that neither the name nor the value may be
 *   a null pointer or an empty character string.  A pair that the SDK rejects
 *   as invalid W3C baggage (e.g. a name containing invalid characters) is
 *   dropped silently and is still included in the returned count.
 *
 * RETURN VALUE
 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in case of
 *   an error.
 */
static int otel_span_set_baggage_var(const struct otelc_span *span, const char *key, const char *value, ...)
{
	va_list ap;
	int     retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", \"%s\", ...", span, OTELC_STR_ARG(key), OTELC_STR_ARG(value));

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_NAME);
	else if (!OTELC_STR_IS_VALID(value))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_VALUE);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	auto baggage = otel_baggage::GetBaggage(*(handle->context));

	/* Iterate over the variadic key-value pairs and set each in baggage. */
	OTEL_VA_AUTO(ap, value);
	for (retval = 0; !OTEL_NULL(key) && !OTEL_NULL(value); retval++) {
		if (!OTELC_STR_IS_VALID(key))
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_NAME);
		else if (!OTELC_STR_IS_VALID(value))
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_VALUE);

		baggage = baggage->Set(key, value);

		key = va_arg(ap, decltype(key));
		if (!OTEL_NULL(key))
			value = va_arg(ap, decltype(value));
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
 *   string, with the restriction that neither the name nor the value may be
 *   a null pointer or an empty character string.  A pair that the SDK rejects
 *   as invalid W3C baggage (e.g. a name containing invalid characters) is
 *   dropped silently and is still included in the returned count.
 *
 * RETURN VALUE
 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in case of
 *   an error.
 */
static int otel_span_set_baggage_kv_var(const struct otelc_span *span, const struct otelc_kv *kv, ...)
{
	va_list ap;
	int     retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %p, ...", span, kv);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_KV);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	auto baggage = otel_baggage::GetBaggage(*(handle->context));

	/* Iterate over the variadic kv pairs and set each in baggage. */
	OTEL_VA_AUTO(ap, kv);
	for (retval = 0; !OTEL_NULL(kv); retval++) {
		if (!OTELC_STR_IS_VALID(kv->key)) {
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_NAME);
		}
		else if (kv->value.u_type == OTELC_VALUE_STRING) {
			if (!OTELC_STR_IS_VALID(kv->value.u.value_string))
				OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_VALUE);

			baggage = baggage->Set(kv->key, kv->value.u.value_string);
		}
		else if (kv->value.u_type == OTELC_VALUE_DATA) {
			if (!OTELC_STR_IS_VALID(OTEL_CAST_REINTERPRET(const char *, kv->value.u.value_data)))
				OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_VALUE);

			baggage = baggage->Set(kv->key, OTEL_CAST_REINTERPRET(const char *, kv->value.u.value_data));
		}
		else {
			OTELC_DBG(ERROR, "invalid value data type: %d", kv->value.u_type);

			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_VALUE_TYPE);
		}

		kv = va_arg(ap, decltype(kv));
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
 *   string, with the restriction that neither the name nor the value may be
 *   a null pointer or an empty character string.  A pair that the SDK rejects
 *   as invalid W3C baggage (e.g. a name containing invalid characters) is
 *   dropped silently and is still included in the returned count.
 *
 * RETURN VALUE
 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in case of
 *   an error.
 */
static int otel_span_set_baggage_kv_n(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
{
	int retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %p, %zu", span, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_KV);
	else if (kv_len == 0)
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_KV " array size");

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	auto baggage = otel_baggage::GetBaggage(*(handle->context));

	/* Iterate over the kv array and set each entry in baggage. */
	for (retval = 0; retval < OTEL_CAST_STATIC(int, kv_len); retval++)
		if (!OTELC_STR_IS_VALID(kv[retval].key)) {
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_NAME);
		}
		else if (kv[retval].value.u_type == OTELC_VALUE_STRING) {
			if (!OTELC_STR_IS_VALID(kv[retval].value.u.value_string))
				OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_VALUE);

			baggage = baggage->Set(kv[retval].key, kv[retval].value.u.value_string);
		}
		else if (kv[retval].value.u_type == OTELC_VALUE_DATA) {
			if (!OTELC_STR_IS_VALID(OTEL_CAST_REINTERPRET(const char *, kv[retval].value.u.value_data)))
				OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_VALUE);

			baggage = baggage->Set(kv[retval].key, OTEL_CAST_REINTERPRET(const char *, kv[retval].value.u.value_data));
		}
		else {
			OTELC_DBG(ERROR, "invalid value data type: %d", kv[retval].value.u_type);

			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_VALUE_TYPE);
		}

	OTEL_SPAN_UPDATE_BAGGAGE(handle, baggage, retval);
}


/***
 * NAME
 *   otel_span_set_baggage - sets a single baggage entry
 *
 * SYNOPSIS
 *   static int otel_span_set_baggage(const struct otelc_span *span, const char *key, const char *value)
 *
 * ARGUMENTS
 *   span  - span instance
 *   key   - baggage name
 *   value - baggage value
 *
 * DESCRIPTION
 *   Stores a single entry in the baggage key-value store.  Unlike
 *   set_baggage_var which accepts a NULL-terminated variadic list, this
 *   function sets exactly one key-value pair per call.  A pair that the SDK
 *   rejects as invalid W3C baggage (e.g. a name containing invalid
 *   characters) is dropped silently while the function still reports success.
 *
 * RETURN VALUE
 *   Returns 1 on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_set_baggage(const struct otelc_span *span, const char *key, const char *value)
{
	OTELC_FUNC("%p, \"%s\", \"%s\"", span, OTELC_STR_ARG(key), OTELC_STR_ARG(value));

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_NAME);
	else if (!OTELC_STR_IS_VALID(value))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_BAGGAGE_VALUE);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	auto baggage = otel_baggage::GetBaggage(*(handle->context));
	baggage      = baggage->Set(key, value);

	OTEL_SPAN_UPDATE_BAGGAGE(handle, baggage, 1);
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
 *   case of error.  The caller is responsible for freeing the returned string
 *   with OTELC_SFREE().
 */
static char *otel_span_get_baggage(const struct otelc_span *span, const char *key)
{
	std::string  value;
	char        *retptr = nullptr;

	OTELC_FUNC("%p, \"%s\"", span, OTELC_STR_ARG(key));

	if (OTEL_NULL(span))
		OTELC_RETURN_PTR(nullptr);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_RETURN_PTR(OTEL_ERROR_MSG_INVALID_BAGGAGE_NAME);

	OTEL_LOCK_SPAN_HANDLE(_PTR, span);

	const auto baggage = otel_baggage::GetBaggage(*(handle->context));
	OTEL_DBG_BAGGAGE(baggage);

	if (baggage->GetValue(key, value)) {
		retptr = OTELC_STRNDUP(__func__, __LINE__, value.c_str(), value.length());
		if (OTEL_NULL(retptr))
			OTEL_SPAN_RETURN_PTR(OTEL_ERROR_MSG_ENOMEM("baggage value"));
	}

	OTELC_RETURN_EX(retptr, decltype(retptr), "\"%s\"");
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
 *   there was an error; the caller releases it with otelc_text_map_destroy().
 */
static struct otelc_text_map *otel_span_get_baggage_var(const struct otelc_span *span, const char *key, ...)
{
	va_list                ap;
	struct otelc_text_map *retptr = nullptr;
	int                    i, m, n;

	OTELC_FUNC("%p, \"%s\", ...", span, OTELC_STR_ARG(key));

	if (OTEL_NULL(span))
		OTELC_RETURN_PTR(nullptr);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_RETURN_PTR(OTEL_ERROR_MSG_INVALID_BAGGAGE_NAME);

	OTEL_SPAN_MAP_GUARD(_PTR, span, span);

	va_start(ap, key);
	for (n = 1; !OTEL_NULL(va_arg(ap, decltype(key))); n++);
	va_end(ap);

	if (OTEL_NULL(retptr = OTELC_TEXT_MAP_NEW(nullptr, n)))
		OTEL_SPAN_RETURN_PTR(OTEL_ERROR_MSG_ENOMEM("baggage map"));

	OTEL_LOCK_TRACER(span, span->idx);

	const auto handle = OTEL_SPAN_HANDLE(span);
	if (OTEL_NULL(handle)) {
		otelc_text_map_destroy(&retptr);

		OTEL_SPAN_RETURN_PTR(OTEL_ERROR_MSG_INVALID_SPAN);
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

				OTEL_SPAN_RETURN_PTR("Unable to copy baggage data to text map");
			}

			OTELC_DBG(OTELC, "get baggage[%d]: \"%s\" -> \"%s\"", i, retptr->key[m], retptr->value[m]);

			m++;
		} else {
			OTELC_DBG(OTELC, "get baggage[%d]: \"%s\" -> UNSET", i, key);
		}

		key = va_arg(ap, decltype(key));
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
	OTELC_FUNC("<span>, \"%s\", %p", OTELC_STR_ARG(key), value);

	if (OTEL_NULL(key) || OTEL_NULL(value))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTELC_DBG(DEBUG, "'%s' -> %s", key, otelc_value_dump(value, ""));

	if (value->u_type == OTELC_VALUE_NULL)
		span->SetAttribute(key, "");
	else if (OTELC_IN_RANGE(value->u_type, OTELC_VALUE_BOOL, OTELC_VALUE_DATA))
		otelc_value_visit(value, [&](auto val) { span->SetAttribute(key, val); });
	else
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTELC_RETURN_INT(OTELC_RET_OK);
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
	va_list ap;
	int     retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", %p, ...", span, OTELC_STR_ARG(key), value);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_RETURN_INT("Invalid attribute key");
	else if (OTEL_NULL(value))
		OTEL_SPAN_RETURN_INT("Invalid attribute value");

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	/* Iterate over the variadic key-value pairs and set each attribute. */
	OTEL_VA_AUTO(ap, value);
	for (retval = 0; !OTEL_NULL(key) && !OTEL_NULL(value); retval++) {
		if (otel_span_set_one_attribute(handle->span, key, value) == OTELC_RET_ERROR)
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_SET_SPAN_ATTR);

		key = va_arg(ap, decltype(key));
		if (!OTEL_NULL(key))
			value = va_arg(ap, decltype(value));
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
	va_list ap;
	int     retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %p, ...", span, kv);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_ATTR_KV);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	/* Iterate over the variadic kv pairs and set each attribute. */
	OTEL_VA_AUTO(ap, kv);
	for (retval = 0; !OTEL_NULL(kv); retval++) {
		if (otel_span_set_one_attribute(handle->span, kv->key, &(kv->value)) == OTELC_RET_ERROR)
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_SET_SPAN_ATTR);

		kv = va_arg(ap, decltype(kv));
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
	int retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %p, %zu", span, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_ATTR_KV);
	else if (kv_len == 0)
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_ATTR_KV " array size");

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	/* Iterate over the kv array and set each attribute. */
	for (retval = 0; retval < OTEL_CAST_STATIC(int, kv_len); retval++)
		if (otel_span_set_one_attribute(handle->span, kv[retval].key, &(kv[retval].value)) == OTELC_RET_ERROR)
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_SET_SPAN_ATTR);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_span_add_one_event - adds an attribute to an event's attribute list
 *
 * SYNOPSIS
 *   static int otel_span_add_one_event(const struct otelc_span *span, otel_attributes &attr, const char *key, const struct otelc_value *value)
 *
 * ARGUMENTS
 *   span  - span instance
 *   attr  - list of attribute key-value pairs
 *   key   - key of the attribute being added to the list
 *   value - value of the attribute being added to the list
 *
 * DESCRIPTION
 *   Appends a single attribute to the event's attribute list.  The list keeps
 *   the caller-supplied key and value without copying them, so it avoids the
 *   per-entry allocations of a keyed map; a duplicate key is resolved by the
 *   SDK when the event is recorded.  The attribute list can later be used as
 *   an argument when adding the event to the span.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK if the attribute has been added, or OTELC_RET_ERROR in
 *   case of an error.
 */
static int otel_span_add_one_event(const struct otelc_span *span, otel_attributes &attr, const char *key, const struct otelc_value *value)
{
	OTELC_FUNC("%p, <attr>, \"%s\", %p", span, OTELC_STR_ARG(key), value);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(key))
		OTEL_SPAN_RETURN_INT("Invalid event attribute");

	OTELC_DBG(DEBUG, "'%s' -> %s", key, otelc_value_dump(value, ""));

	OTEL_VALUE_ADD(_INT, attr, emplace_back, key, value, otel_span_err(span), "Unable to add event");

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
	va_list               ap;
	otel_attributes       attr{};
	otel_system_timestamp timestamp = std::chrono::system_clock::now();
	int                   retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", %p, \"%s\", %p, ...", span, OTELC_STR_ARG(name), ts_system, OTELC_STR_ARG(key), value);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(name))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_EVENT_NAME);
	else if (!OTELC_STR_IS_VALID(key))
		OTEL_SPAN_RETURN_INT("Invalid event key");
	else if (OTEL_NULL(value))
		OTEL_SPAN_RETURN_INT("Invalid event value");

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	if (!OTEL_NULL(ts_system))
		timestamp = otel_system_timestamp(timespec_to_duration(ts_system));

	/* Iterate over the variadic key-value pairs and add each to the event. */
	OTEL_VA_AUTO(ap, value);
	for (retval = 0; !OTEL_NULL(key) && !OTEL_NULL(value); retval++) {
		if (otel_span_add_one_event(span, attr, key, value) == OTELC_RET_ERROR)
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_SET_EVENT_ATTR);

		key = va_arg(ap, decltype(key));
		if (!OTEL_NULL(key))
			value = va_arg(ap, decltype(value));
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
	va_list               ap;
	otel_attributes       attr{};
	otel_system_timestamp timestamp = std::chrono::system_clock::now();
	int                   retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", %p, %p, ...", span, OTELC_STR_ARG(name), ts_system, kv);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(name))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_EVENT_NAME);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_EVENT_KV);

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	if (!OTEL_NULL(ts_system))
		timestamp = otel_system_timestamp(timespec_to_duration(ts_system));

	/* Iterate over the variadic kv pairs and add each to the event. */
	OTEL_VA_AUTO(ap, kv);
	for (retval = 0; !OTEL_NULL(kv); retval++) {
		if (otel_span_add_one_event(span, attr, kv->key, &(kv->value)) == OTELC_RET_ERROR)
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_SET_EVENT_ATTR);

		kv = va_arg(ap, decltype(kv));
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
	otel_attributes       attr{};
	otel_system_timestamp timestamp = std::chrono::system_clock::now();
	int                   retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, \"%s\", %p, %p, %zu", span, OTELC_STR_ARG(name), ts_system, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(name))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_EVENT_NAME);
	else if (OTEL_NULL(kv))
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_EVENT_KV);
	else if (kv_len == 0)
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_EVENT_KV " array size");

	try {
		OTEL_DBG_THROW();
		attr.reserve(kv_len);
	}
	OTEL_CATCH_SIGNAL_RETURN( , OTEL_SPAN_RETURN_INT, "Unable to allocate event attributes")

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	if (!OTEL_NULL(ts_system))
		timestamp = otel_system_timestamp(timespec_to_duration(ts_system));

	/* Iterate over the kv array and add each to the event. */
	for (retval = 0; retval < OTEL_CAST_STATIC(int, kv_len); retval++)
		if (otel_span_add_one_event(span, attr, kv[retval].key, &(kv[retval].value)) == OTELC_RET_ERROR)
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_SET_EVENT_ATTR);

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
	OTELC_FUNC("%p, %p, %p, %p, %zu", span, link_span, link_context, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO >= 2)
	auto            target = otel_trace::SpanContext::GetInvalid();
	otel_attributes attribute{};

	if (!OTEL_NULL(link_span) && !OTEL_NULL(link_context))
		OTEL_SPAN_RETURN_INT("Parameters link_span and link_context are mutually exclusive");

	if (OTEL_NULL(link_span) && OTEL_NULL(link_context))
		OTEL_SPAN_RETURN_INT("One of link_span or link_context must be specified");

	if (!OTEL_NULL(kv) && (kv_len > 0)) {
		try {
			OTEL_DBG_THROW();
			attribute.reserve(kv_len);
		}
		OTEL_CATCH_SIGNAL_RETURN( , OTEL_SPAN_RETURN_INT, OTEL_ERROR_MSG_LINK_ATTRS)

		for (size_t i = 0; i < kv_len; ++i)
			OTEL_VALUE_ADD(_INT, attribute, emplace_back, kv[i].key, &(kv[i].value), otel_span_err(span), "Unable to add link");
	}

	/* Resolve the link target from either a span or a span context. */
	if (!OTEL_NULL(link_span)) {
		OTEL_LOCK_SPAN_HANDLE(_INT, link_span, "Invalid linked span");

		target = handle->span->GetContext();
	} else {
		OTEL_SPAN_MAP_GUARD(_INT, span_context, link_context);

		OTEL_LOCK_TRACER(span_context, link_context->idx);

		const auto context_handle = OTEL_SPAN_CONTEXT_HANDLE(link_context);
		if (OTEL_NULL(context_handle))
			OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_LINKED_CTX);

		target = otel_trace::GetSpan(*(context_handle->context))->GetContext();
	}

	/* Add the resolved link to the span. */
	if (target.IsValid()) {
		OTEL_LOCK_SPAN_HANDLE(_INT, span);

		handle->span->AddLink(target, attribute);
	} else {
		OTEL_SPAN_RETURN_INT(OTEL_ERROR_MSG_INVALID_LINKED_CTX);
	}

	OTELC_RETURN_INT(OTELC_RET_OK);
#else
	OTEL_SPAN_RETURN_INT("AddLink is not supported in this OpenTelemetry ABI version");
#endif /* OPENTELEMETRY_ABI_VERSION_NO && (OPENTELEMETRY_ABI_VERSION_NO >= 2) */
}


/***
 * NAME
 *   otel_span_record_exception - records an exception event on the span
 *
 * SYNOPSIS
 *   static int otel_span_record_exception(const struct otelc_span *span, const char *type, const char *message, const char *stacktrace, const struct timespec *ts_system, const struct otelc_kv *kv, size_t kv_len)
 *
 * ARGUMENTS
 *   span       - span instance
 *   type       - exception type or class name
 *   message    - exception message, or NULL to omit
 *   stacktrace - stack trace string, or NULL to omit
 *   ts_system  - time of the exception, or NULL for now
 *   kv         - an array of additional key-value attributes
 *   kv_len     - size of key-value pair array
 *
 * DESCRIPTION
 *   Records an exception as an event on the span, following the OpenTelemetry
 *   semantic conventions.  The event is named "exception" and carries the
 *   standard attributes exception.type, exception.message, and
 *   exception.stacktrace.  Additional attributes can be provided via the kv
 *   array.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_record_exception(const struct otelc_span *span, const char *type, const char *message, const char *stacktrace, const struct timespec *ts_system, const struct otelc_kv *kv, size_t kv_len)
{
	otel_attributes       attr{};
	otel_system_timestamp timestamp = std::chrono::system_clock::now();

	OTELC_FUNC("%p, \"%s\", \"%s\", \"%s\", %p, %p, %zu", span, OTELC_STR_ARG(type), OTELC_STR_ARG(message), OTELC_STR_ARG(stacktrace), ts_system, kv, kv_len);

	if (OTEL_NULL(span))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (!OTELC_STR_IS_VALID(type))
		OTEL_SPAN_RETURN_INT("Invalid exception type");

	OTEL_LOCK_SPAN_HANDLE(_INT, span);

	if (!OTEL_NULL(ts_system))
		timestamp = otel_system_timestamp(timespec_to_duration(ts_system));

	try {
		OTEL_DBG_THROW();
		(void)attr.emplace_back("exception.type", otel_nostd::string_view{type});
	}
	OTEL_CATCH_SIGNAL_RETURN( , OTEL_SPAN_RETURN_INT, "Unable to add exception type")

	/* Add the exception message attribute if provided. */
	if (OTELC_STR_IS_VALID(message))
		try {
			OTEL_DBG_THROW();
			(void)attr.emplace_back("exception.message", otel_nostd::string_view{message});
		}
		OTEL_CATCH_SIGNAL_RETURN( , OTEL_SPAN_RETURN_INT, "Unable to add exception message")

	/* Add the exception stacktrace attribute if provided. */
	if (OTELC_STR_IS_VALID(stacktrace))
		try {
			OTEL_DBG_THROW();
			(void)attr.emplace_back("exception.stacktrace", otel_nostd::string_view{stacktrace});
		}
		OTEL_CATCH_SIGNAL_RETURN( , OTEL_SPAN_RETURN_INT, "Unable to add exception stacktrace")

	/* Add any extra user-supplied attributes to the exception event. */
	if (!OTEL_NULL(kv) && (kv_len > 0))
		for (size_t i = 0; i < kv_len; i++)
			OTEL_VALUE_ADD(_INT, attr, emplace_back, kv[i].key, &(kv[i].value), otel_span_err(span), "Unable to add exception attribute");

	handle->span->AddEvent(otel_nostd::string_view{"exception"}, timestamp, attr);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_nolock_span_destroy - destroys all references related to the specified span
 *
 * SYNOPSIS
 *   void otel_nolock_span_destroy(struct otelc_span **span)
 *
 * ARGUMENTS
 *   span - address of a span instance pointer to be destroyed
 *
 * DESCRIPTION
 *   Non-locking version of otel_span_destroy().  The caller must hold
 *   the per-shard mutex for the span's index before calling this function.
 *   Looks up the span handle in the otel_span handle map, deletes the
 *   associated C++ object, erases the map entry, and frees the C-style
 *   otelc_span structure.  After this function, the *span pointer is set
 *   to null.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_nolock_span_destroy(struct otelc_span **span)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(span));

	if (OTEL_NULL(span) || OTEL_NULL(*span))
		OTELC_RETURN();

#ifndef OTELC_USE_STATIC_HANDLE
	/***
	 * The handle maps are torn down together with the last tracer.  A
	 * span that outlived that teardown has no handle map to consult and
	 * its handle has already been deleted; release only the C structure.
	 */
	if (OTEL_NULL(otel_span)) {
		OTEL_EXT_FREE_CLEAR(*span);

		OTELC_RETURN();
	}
#endif

	/* Look up the span handle, delete it, and erase it from the map. */
	const auto handle = OTEL_SPAN_HANDLE(*span);
	if (!OTEL_NULL(handle)) {
		delete handle;

		(void)OTEL_HANDLE(otel_span, get_shard((*span)->idx).map).erase((*span)->idx);
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
 *   span - address of a span instance pointer to be destroyed
 *
 * DESCRIPTION
 *   Destroys all references associated with a specific span, as well as
 *   deleting the span from the otel_span handle map.  After this function
 *   is executed, all data related to the span is deleted and the *span
 *   pointer is set to null.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_span_destroy(struct otelc_span **span)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(span));

	if (OTEL_NULL(span) || OTEL_NULL(*span))
		OTELC_RETURN();

#ifndef OTELC_USE_STATIC_HANDLE
	/* The maps are gone: skip the shard lock, the nolock path frees the structure. */
	if (OTEL_NULL(otel_span)) {
		otel_nolock_span_destroy(span);

		OTELC_RETURN();
	}
#endif

	OTEL_LOCK_TRACER(span, (*span)->idx);

	otel_nolock_span_destroy(span);

	OTELC_RETURN();
}


/* The span operations vtable. */
const static struct otelc_span_ops otel_span_ops = {
	.get_id               = otel_span_get_id,               /* lock span */
	.is_recording         = otel_span_is_recording,         /* lock span */
	.end                  = otel_span_end,                  /* lock span */
	.end_with_options     = otel_span_end_with_options,     /* lock span */
	.set_operation_name   = otel_span_set_operation_name,   /* lock span */
	.set_baggage_var      = otel_span_set_baggage_var,      /* lock span */
	.set_baggage_kv_var   = otel_span_set_baggage_kv_var,   /* lock span */
	.set_baggage_kv_n     = otel_span_set_baggage_kv_n,     /* lock span */
	.set_baggage          = otel_span_set_baggage,          /* lock span */
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
	.record_exception     = otel_span_record_exception,     /* lock span */
	.destroy              = otel_span_destroy,              /* lock span */
};


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
	struct otelc_span *retptr = nullptr;

	OTELC_FUNC("%p", tracer);

	if (OTEL_NULL(tracer))
		OTELC_RETURN_PTR(retptr);

	if (!OTEL_NULL(retptr = OTEL_CAST_TYPEOF(retptr, OTEL_EXT_MALLOC(sizeof(*retptr))))) {
		retptr->idx    = OTEL_HANDLE(otel_span, id++);
		retptr->tracer = tracer;
		retptr->ops    = &otel_span_ops;
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
 *   must be large enough to hold the respective identifiers.  A buffer smaller
 *   than its identifier is skipped silently: the buffer is left unmodified and
 *   the function still reports success.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_get_id(const struct otelc_span_context *context, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
{
	OTELC_FUNC("%p, %p, %zu, %p, %zu, %p", context, span_id, span_id_size, trace_id, trace_id_size, trace_flags);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

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
	OTELC_FUNC("%p", context);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();

	OTELC_RETURN_INT(span_ctx.IsValid());
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
	OTELC_FUNC("%p", context);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();

	OTELC_RETURN_INT(span_ctx.IsSampled());
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
	OTELC_FUNC("%p", context);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();

	OTELC_RETURN_INT(span_ctx.IsRemote());
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
	OTELC_FUNC("%p, \"%s\", %p, %zu", context, OTELC_STR_ARG(key), value, value_size);

	if (OTEL_NULL(context) || OTEL_NULL(key))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

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
 *   to the provided text map.  The text map must be initialized before the
 *   call, either created by otelc_text_map_new() or zero-filled.  The caller
 *   is responsible for freeing the map contents with otelc_text_map_destroy().
 *
 * RETURN VALUE
 *   Returns the number of entries added to the text map, or OTELC_RET_ERROR in
 *   case of an error.
 */
static int otel_span_context_trace_state_entries(const struct otelc_span_context *context, struct otelc_text_map *text_map)
{
	OTELC_FUNC("%p, %p", context, text_map);

	if (OTEL_NULL(context) || OTEL_NULL(text_map))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

	/* Iterate over all trace state entries and copy them to the text map. */
	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();
	int count = 0;

	(void)ts->GetAllEntries([&](otel_nostd::string_view key, otel_nostd::string_view val) -> bool {
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
	OTELC_FUNC("%p, %p, %zu", context, header, header_size);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

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
	OTELC_FUNC("%p", context);

	if (OTEL_NULL(context))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

	/* Query whether the trace state contains any entries. */
	const auto span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();

	OTELC_RETURN_INT(ts->Empty());
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
 *   A key or value that fails W3C validation makes the SDK return the empty
 *   trace state, so the function returns 0 with an empty header; this outcome
 *   is indistinguishable from a genuinely empty trace state.
 *
 * RETURN VALUE
 *   Returns the length of the resulting header string (excluding NUL), 0 if
 *   the trace state is empty, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_trace_state_set(const struct otelc_span_context *context, const char *key, const char *value, char *header, size_t header_size)
{
	OTELC_FUNC("%p, \"%s\", \"%s\", %p, %zu", context, OTELC_STR_ARG(key), OTELC_STR_ARG(value), header, header_size);

	if (OTEL_NULL(context) || OTEL_NULL(key) || OTEL_NULL(value))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

	/* Set the key-value pair in the trace state and serialize the result. */
	const auto  span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();
PRAGMA_DIAG_IGNORE("-Walloc-size-larger-than=")
	auto        new_ts = ts->Set(key, value);
PRAGMA_DIAG_RESTORE

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
 *   be zero/null when only the length is needed.  A key that fails W3C
 *   validation makes the SDK return the empty trace state, so the function
 *   returns 0 with an empty header; this outcome is indistinguishable from a
 *   genuinely empty trace state.
 *
 * RETURN VALUE
 *   Returns the length of the resulting header string (excluding NUL), 0 if
 *   the trace state is empty, or OTELC_RET_ERROR in case of an error.
 */
static int otel_span_context_trace_state_delete(const struct otelc_span_context *context, const char *key, char *header, size_t header_size)
{
	OTELC_FUNC("%p, \"%s\", %p, %zu", context, OTELC_STR_ARG(key), header, header_size);

	if (OTEL_NULL(context) || OTEL_NULL(key))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	OTEL_LOCK_SPAN_CONTEXT_HANDLE(_INT, context);

	/* Remove the key from the trace state and serialize the result. */
	const auto  span_ctx = otel_trace::GetSpan(*(handle->context))->GetContext();
	const auto &ts = span_ctx.trace_state();
PRAGMA_DIAG_IGNORE("-Walloc-size-larger-than=")
	auto        new_ts = ts->Delete(key);
PRAGMA_DIAG_RESTORE

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
 *   context - address of a pointer to the span context instance to be destroyed
 *
 * DESCRIPTION
 *   Non-locking version of otel_span_context_destroy().  The caller must
 *   hold the per-shard mutex for the context's index before calling this
 *   function.  Looks up the span context handle in the otel_span_context
 *   handle map, deletes the associated C++ object, erases the map entry,
 *   and frees the C-style otelc_span_context structure.  After this
 *   function, the *context pointer is set to null.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_nolock_span_context_destroy(struct otelc_span_context **context)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(context));

	if (OTEL_NULL(context) || OTEL_NULL(*context))
		OTELC_RETURN();

#ifndef OTELC_USE_STATIC_HANDLE
	/***
	 * The handle maps are torn down together with the last tracer.  A
	 * span context that outlived that teardown has no handle map to
	 * consult and its handle has already been deleted; release only the
	 * C structure.
	 */
	if (OTEL_NULL(otel_span_context)) {
		OTEL_EXT_FREE_CLEAR(*context);

		OTELC_RETURN();
	}
#endif

	/* Look up the context handle, delete it, and erase it from the map. */
	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(*context);
	if (!OTEL_NULL(handle)) {
		delete handle;

		(void)OTEL_HANDLE(otel_span_context, get_shard((*context)->idx).map).erase((*context)->idx);
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
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(context));

	if (OTEL_NULL(context) || OTEL_NULL(*context))
		OTELC_RETURN();

#ifndef OTELC_USE_STATIC_HANDLE
	/* The maps are gone: skip the shard lock, the nolock path frees the structure. */
	if (OTEL_NULL(otel_span_context)) {
		otel_nolock_span_context_destroy(context);

		OTELC_RETURN();
	}
#endif

	OTEL_LOCK_TRACER(span_context, (*context)->idx);

	otel_nolock_span_context_destroy(context);

	OTELC_RETURN();
}


/* The span context operations vtable. */
const static struct otelc_span_context_ops otel_span_context_ops = {
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
	struct otelc_span_context *retptr = nullptr;

	OTELC_FUNC("");

#ifndef OTELC_USE_STATIC_HANDLE
	/***
	 * The handle maps exist only while at least one tracer does; without
	 * the map no index can be assigned nor the handle registered later.
	 */
	if (OTEL_NULL(otel_span_context))
		OTELC_RETURN_PTR(retptr);
#endif

	if (!OTEL_NULL(retptr = OTEL_CAST_TYPEOF(retptr, OTEL_EXT_MALLOC(sizeof(*retptr))))) {
		retptr->idx = OTEL_HANDLE(otel_span_context, id++);
		retptr->ops = &otel_span_context_ops;
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
 *   string.  A trace_id or span_id argument that is null or whose size is
 *   smaller than the documented identifier size leaves that identifier
 *   zero-filled, producing a span context whose is_valid() returns false
 *   without an error; a malformed trace_state_header is replaced by an empty
 *   trace state.  At least one tracer must exist at the time of the call
 *   because span context handles live in the handle maps that are created
 *   together with the first tracer and destroyed together with the last one.
 *   An error message stored in *err is allocated by the library and must be
 *   released with OTELC_SFREE().
 *
 * RETURN VALUE
 *   Returns a pointer to a newly created span context on success, or nullptr
 *   on failure.
 */
struct otelc_span_context *otelc_span_context_create(const uint8_t *trace_id, size_t trace_id_size, const uint8_t *span_id, size_t span_id_size, uint8_t trace_flags, int is_remote, const char *trace_state_header, char **err)
{
	uint8_t                    tid[otel_trace::TraceId::kSize] = { 0 };
	uint8_t                    sid[otel_trace::SpanId::kSize]  = { 0 };
	struct otelc_span_context *retptr = nullptr;

	OTELC_FUNC("%p, %zu, %p, %zu, 0x%02x, %d, \"%s\", %p:%p", trace_id, trace_id_size, span_id, span_id_size, trace_flags, is_remote, OTELC_STR_ARG(trace_state_header), OTELC_DPTR_ARGS(err));

#ifndef OTELC_USE_STATIC_HANDLE
	/* Outside the map's lifetime the lookup below would dereference null. */
	if (OTEL_NULL(otel_span_context))
		OTEL_ERR_RETURN_PTR("Unable to create span context: no active tracer");
#endif

	/* Copy raw ID bytes into fixed-size arrays. */
	if (!OTEL_NULL(trace_id) && (trace_id_size >= sizeof(tid)))
		(void)memcpy(tid, trace_id, sizeof(tid));
	if (!OTEL_NULL(span_id) && (span_id_size >= sizeof(sid)))
		(void)memcpy(sid, span_id, sizeof(sid));

	/* Parse optional trace state. */
	auto ts = OTEL_NULL(trace_state_header) ? otel_trace::TraceState::GetDefault() : otel_trace::TraceState::FromHeader(trace_state_header);

	/* Construct C++ SpanContext. */
	otel_trace::SpanContext span_ctx(otel_trace::TraceId{tid}, otel_trace::SpanId{sid}, otel_trace::TraceFlags{trace_flags}, is_remote != 0, ts);

	/***
	 * Wrap in a DefaultSpan and Context.  make_shared_nothrow is used
	 * because the nostd::shared_ptr(pointer) constructor is not noexcept:
	 * its internal control-block allocation may throw bad_alloc, which
	 * must not escape this C API function.
	 */
	otel_nostd::shared_ptr<otel_trace::Span> default_span(otel::make_shared_nothrow<otel_trace::DefaultSpan>(span_ctx));
	if (OTEL_NULL(default_span))
		OTEL_ERR_RETURN_PTR(OTEL_ERROR_MSG_ENOMEM("default span"));

	otel_context::Context empty_ctx{};
	auto context = otel::make_shared_nothrow<otel_context::Context>(otel_trace::SetSpan(empty_ctx, default_span));
	if (OTEL_NULL(context))
		OTEL_ERR_RETURN_PTR(OTEL_ERROR_MSG_ENOMEM("span context"));

	/* Allocate C handle. */
	if (OTEL_NULL(retptr = otel_span_context_new()))
		OTEL_ERR_RETURN_PTR(OTEL_ERROR_MSG_ENOMEM("span context handle"));

	const auto span_context_handle = new(std::nothrow) otel_span_context_handle{std::move(context)};
	if (OTEL_NULL(span_context_handle)) {
		OTEL_LOCK_TRACER(span_context, retptr->idx);
		otel_nolock_span_context_destroy(&retptr);

		OTEL_ERR_RETURN_PTR(OTEL_ERROR_MSG_ENOMEM("span context handle"));
	}

	OTEL_LOCK_TRACER(span_context, retptr->idx);

	/* Register the span context handle in the shared map. */
	OTEL_HANDLE_EMPLACE(otel_span_context, retptr->idx, span_context_handle,
		{ delete span_context_handle; OTEL_EXT_FREE_CLEAR(retptr); },
		OTEL_ERR_RETURN_PTR, OTEL_ERROR_MSG_ADD_SPAN_CTX ": duplicate id", OTEL_ERROR_MSG_ADD_SPAN_CTX
	);

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
