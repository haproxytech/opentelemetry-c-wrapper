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
#ifndef _OPENTELEMETRY_C_WRAPPER_SPAN_H_
#define _OPENTELEMETRY_C_WRAPPER_SPAN_H_

#define OTEL_SPAN_ERROR(f, ...)            OTEL_SIGNAL_ERROR(*otel_span_err(span), f, ##__VA_ARGS__)
#define OTEL_SPAN_RETURN(f, ...)           OTEL_RETURN(span, f, ##__VA_ARGS__)
#define OTEL_SPAN_RETURN_EX(t,r,f, ...)    OTEL_RETURN_EX(span, t, (r), f, ##__VA_ARGS__)
#define OTEL_SPAN_RETURN_INT(f, ...)       OTEL_RETURN_INT(span, f, ##__VA_ARGS__)
#define OTEL_SPAN_RETURN_PTR(f, ...)       OTEL_RETURN_PTR(span, f, ##__VA_ARGS__)

#define OTEL_SPAN_HANDLE(a)                otel_map_find(OTEL_HANDLE(otel_span, get_shard((a)->idx).map), (a)->idx)
/* The span baggage, or the SDK default (empty) baggage when none is set. */
#define OTEL_SPAN_BAGGAGE(h)               (OTEL_NULL((h)->baggage) ? otel_baggage::Baggage::GetDefault() : (h)->baggage)
#define OTEL_SPAN_CONTEXT_HANDLE(a)        otel_map_find(OTEL_HANDLE(otel_span_context, get_shard((a)->idx).map), (a)->idx)

#define OTEL_DBG_SPAN()                    OTEL_DBG_HANDLE(OTEL, "otel_span", otel_span)
#define OTEL_DBG_SPAN_CONTEXT()            OTEL_DBG_HANDLE(OTEL, "otel_span_context", otel_span_context)

#define OTEL_ERROR_MSG_INVALID_SPAN            "Invalid span"
#define OTEL_ERROR_MSG_INVALID_SPAN_CTX        "Invalid span context"
#define OTEL_ERROR_MSG_INVALID_LINKED_CTX      "Invalid linked span context"
#define OTEL_ERROR_MSG_INVALID_OP_NAME         "Invalid operation name"
#define OTEL_ERROR_MSG_INVALID_EVENT_NAME      "Invalid event name"
#define OTEL_ERROR_MSG_INVALID_EVENT_KV        "Invalid event key-value"
#define OTEL_ERROR_MSG_INVALID_BAGGAGE_NAME    "Invalid baggage name"
#define OTEL_ERROR_MSG_INVALID_BAGGAGE_VALUE   "Invalid baggage value"
#define OTEL_ERROR_MSG_INVALID_BAGGAGE_KV      "Invalid baggage key-value"
#define OTEL_ERROR_MSG_INVALID_ATTR_KV         "Invalid attribute key-value"
#define OTEL_ERROR_MSG_INVALID_VALUE_TYPE      "Invalid value data type"
#define OTEL_ERROR_MSG_SET_SPAN_ATTR           "Unable to set span attribute"
#define OTEL_ERROR_MSG_SET_EVENT_ATTR          "Unable to set span event attribute"
#define OTEL_ERROR_MSG_ADD_SPAN                "Unable to add span"
#define OTEL_ERROR_MSG_ADD_SPAN_CTX            "Unable to add span context"
#define OTEL_ERROR_MSG_LINK_ATTRS              "Unable to allocate link attributes"

#define T   otel_span_handle
struct T {
	/* otel_nostd::shared_ptr has no use_count() member. */
#ifdef OTELC_USE_RUNTIME_CONTEXT
	otel_nostd::shared_ptr<otel_trace::Scope>     scope;   /* RAII scope controlling the span's active lifetime. */
#endif
	otel_nostd::shared_ptr<otel_trace::Span>      span;    /* Span associated with this handle. */
	otel_nostd::shared_ptr<otel_baggage::Baggage> baggage; /* Baggage propagated with this span (may be null). */

#ifdef OTELC_USE_RUNTIME_CONTEXT
	T(otel_nostd::shared_ptr<otel_trace::Scope> scope_, otel_nostd::shared_ptr<otel_trace::Span> span_, otel_nostd::shared_ptr<otel_baggage::Baggage> baggage_) noexcept
		: scope(std::move(scope_)), span(std::move(span_)), baggage(std::move(baggage_))
#else
	T(otel_nostd::shared_ptr<otel_trace::Span> span_, otel_nostd::shared_ptr<otel_baggage::Baggage> baggage_) noexcept
		: span(std::move(span_)), baggage(std::move(baggage_))
#endif
	{
		OTELCPP_FUNC("", OTELC_STRINGIFY(T));

		OTELC_RETURN();
	}

	~T() noexcept
	{
		OTELCPP_FUNC("", OTELC_STRINGIFY(T));

		/***
		 * Setting the number of references to zero will cause implicit
		 * deletion of the allocated memory pointed to by shared
		 * pointers.
		 */
		baggage = nullptr;
		span    = nullptr;
#ifdef OTELC_USE_RUNTIME_CONTEXT
		scope   = nullptr;
#endif

		OTELC_RETURN();
	}
};
#undef T

#define T   otel_span_context_handle
struct T {
	/* otel_nostd::shared_ptr has no use_count() member. */
	otel_nostd::shared_ptr<otel_context::Context> context; /* Context associated with this instance. */

	T(otel_nostd::shared_ptr<otel_context::Context> context_) noexcept
		: context(std::move(context_))
	{
		OTELCPP_FUNC("", OTELC_STRINGIFY(T));

		OTELC_RETURN();
	}

	~T() noexcept
	{
		OTELCPP_FUNC("", OTELC_STRINGIFY(T));

		context = nullptr;

		OTELC_RETURN();
	}
};
#undef T

/***
 * Early return used when the handle maps have already been torn down together
 * with the last tracer.  The owning tracer of a leftover instance may be gone
 * as well, so the error must not be recorded through span->tracer->err; the
 * condition is only debug-logged instead.
 */
#ifndef OTELC_USE_STATIC_HANDLE
#  define OTEL_SPAN_TEARDOWN_RETURN        OTELC_RETURN()
#  define OTEL_SPAN_TEARDOWN_RETURN_INT    OTELC_RETURN_INT(OTELC_RET_ERROR)
#  define OTEL_SPAN_TEARDOWN_RETURN_PTR    OTELC_RETURN_PTR(nullptr)
#  define OTEL_SPAN_MAP_GUARD(arg_type, arg_map, arg_handle)                                  \
	if (OTEL_NULL(otel_##arg_map)) {                                                      \
		OTELC_DBG(OTEL, "invalid otel_" #arg_map "[%" PRId64 "]", (arg_handle)->idx); \
		                                                                              \
		OTEL_SPAN_TEARDOWN_RETURN##arg_type;                                          \
	}
#else
#  define OTEL_SPAN_MAP_GUARD(arg_type, arg_map, arg_handle)   while (0)
#endif /* OTELC_USE_STATIC_HANDLE */

#define OTEL_LOCK_SPAN_HANDLE(...)         OTEL_23(__VA_ARGS__, OTEL_LOCK_SPAN_HANDLE_3, OTEL_LOCK_SPAN_HANDLE_2)(__VA_ARGS__)
#define OTEL_LOCK_SPAN_HANDLE_2(t,h)       OTEL_LOCK_SPAN_HANDLE_3(t, (h), OTEL_ERROR_MSG_INVALID_SPAN)
#define OTEL_LOCK_SPAN_HANDLE_3(arg_type, arg_handle, arg_msg)                        \
	OTEL_SPAN_MAP_GUARD(arg_type, span, arg_handle);                              \
	                                                                              \
	OTEL_LOCK_TRACER(span, (arg_handle)->idx);                                    \
	                                                                              \
	const auto handle = OTEL_SPAN_HANDLE(arg_handle);                             \
	if (OTEL_NULL(handle)) {                                                      \
		OTELC_DBG(OTEL, "invalid otel_span[%" PRId64 "]", (arg_handle)->idx); \
		                                                                      \
		OTEL_SPAN_RETURN##arg_type(arg_msg);                                  \
	}

/***
 * Early-return dispatch used by OTEL_LOCK_SPAN_CONTEXT_HANDLE when the
 * looked-up handle is missing.  A span context carries no error channel, so
 * the condition is only debug-logged; the arg_type suffix selects the failure
 * value: the empty form returns void, _INT returns OTELC_RET_ERROR, and _PTR
 * returns nullptr.
 */
#define OTEL_SPAN_CONTEXT_RETURN           OTELC_RETURN()
#define OTEL_SPAN_CONTEXT_RETURN_INT       OTELC_RETURN_INT(OTELC_RET_ERROR)
#define OTEL_SPAN_CONTEXT_RETURN_PTR       OTELC_RETURN_PTR(nullptr)

#define OTEL_LOCK_SPAN_CONTEXT_HANDLE(arg_type, arg_handle)                                   \
	OTEL_SPAN_MAP_GUARD(arg_type, span_context, arg_handle);                              \
	                                                                                      \
	OTEL_LOCK_TRACER(span_context, (arg_handle)->idx);                                    \
	                                                                                      \
	const auto handle = OTEL_SPAN_CONTEXT_HANDLE(arg_handle);                             \
	if (OTEL_NULL(handle)) {                                                              \
		OTELC_DBG(OTEL, "invalid otel_span_context[%" PRId64 "]", (arg_handle)->idx); \
		                                                                              \
		OTEL_SPAN_CONTEXT_RETURN##arg_type;                                           \
	}

/***
 * Stores the modified baggage in the span handle, then returns from the
 * calling function.  This is a macro because it must call OTELC_RETURN_INT,
 * which returns from the caller.
 *
 * The handle keeps the baggage apart from the span: a child span inherits
 * the baggage pointer, and the propagation Context that carries both is
 * built only when a carrier is injected.
 */
#define OTEL_SPAN_UPDATE_BAGGAGE(arg_handle, arg_baggage, arg_retval) \
	OTEL_DBG_BAGGAGE(arg_baggage);                                \
	                                                              \
	if ((arg_retval) <= 0)                                        \
		OTELC_RETURN_INT(arg_retval);                         \
	                                                              \
	(arg_handle)->baggage = std::move(arg_baggage);               \
	                                                              \
	OTELC_DBG(OTEL, "new span baggage set");                      \
	                                                              \
	OTELC_RETURN_INT(arg_retval);


#ifdef OTELC_USE_STATIC_HANDLE
extern THREAD_LOCAL struct otel_handle<struct otel_span_handle *, OTEL_HANDLE_SHARED>          otel_span;
extern THREAD_LOCAL struct otel_handle<struct otel_span_context_handle *, OTEL_HANDLE_SHARED>  otel_span_context;
#else
extern THREAD_LOCAL struct otel_handle<struct otel_span_handle *, OTEL_HANDLE_SHARED>         *otel_span;
extern THREAD_LOCAL struct otel_handle<struct otel_span_context_handle *, OTEL_HANDLE_SHARED> *otel_span_context;
#endif


char                     **otel_span_err(const struct otelc_span *span);
struct otelc_span         *otel_span_new(struct otelc_tracer *tracer);
void                       otel_nolock_span_destroy(struct otelc_span **span);
struct otelc_span_context *otel_span_context_new(void);
void                       otel_nolock_span_context_destroy(struct otelc_span_context **context);

#endif /* _OPENTELEMETRY_C_WRAPPER_SPAN_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
