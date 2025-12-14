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

#define OTEL_SPAN_ERROR(f, ...)            do { (void)otelc_sprintf(&(span->tracer->err), f, ##__VA_ARGS__); OTELC_DBG(OTEL, "%s", span->tracer->err); } while (0)
#define OTEL_SPAN_ERETURN(f, ...)          do { OTEL_SPAN_ERROR(f, ##__VA_ARGS__); OTELC_RETURN(); } while (0)
#define OTEL_SPAN_ERETURN_EX(t,r,f, ...)   do { OTEL_SPAN_ERROR(f, ##__VA_ARGS__); OTELC_RETURN##t(r); } while (0)
#define OTEL_SPAN_ERETURN_INT(f, ...)      OTEL_SPAN_ERETURN_EX(_INT, OTELC_RET_ERROR, f, ##__VA_ARGS__)
#define OTEL_SPAN_ERETURN_PTR(f, ...)      OTEL_SPAN_ERETURN_EX(_PTR, nullptr, f, ##__VA_ARGS__)

#define OTEL_SPAN_HANDLE(a)                (OTEL_HANDLE(otel_span, find)((a)->idx))
#define OTEL_SPAN_CONTEXT_HANDLE(a)        (OTEL_HANDLE(otel_span_context, find)((a)->idx))

#define OTEL_DBG_SPAN()                    OTEL_DBG_HANDLE(OTEL, "otel_span", otel_span)
#define OTEL_DBG_SPAN_CONTEXT()            OTEL_DBG_HANDLE(OTEL, "otel_span_context", otel_span_context)

#define T   otel_span_handle
struct T {
	/* otel_nostd::shared_ptr has no use_count() member. */
#ifdef OTELC_USE_RUNTIME_CONTEXT
	otel_nostd::shared_ptr<otel_trace::Scope>     scope;   /* RAII scope controlling the span's active lifetime. */
#endif
	otel_nostd::shared_ptr<otel_trace::Span>      span;    /* Span associated with this handle. */
	otel_nostd::shared_ptr<otel_context::Context> context; /* Context propagated with this span. */

#ifdef OTELC_USE_RUNTIME_CONTEXT
	T(otel_nostd::shared_ptr<otel_trace::Scope> scope_, otel_nostd::shared_ptr<otel_trace::Span> span_, otel_nostd::shared_ptr<otel_context::Context> context_) noexcept
		: scope(std::move(scope_)), span(std::move(span_)), context(std::move(context_))
#else
	T(otel_nostd::shared_ptr<otel_trace::Span> span_, otel_nostd::shared_ptr<otel_context::Context> context_) noexcept
		: span(std::move(span_)), context(std::move(context_))
#endif
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(T));

		OTELC_RETURN();
	}

	~T() noexcept
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(T));

		/***
		 * Setting the number of references to zero will cause implicit
		 * deletion of the allocated memory pointed to by shared
		 * pointers.
		 */
		context = nullptr;
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
		OTELC_FUNCPP("", OTELC_STRINGIFY(T));

		OTELC_RETURN();
	}

	~T() noexcept
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(T));

		context = nullptr;

		OTELC_RETURN();
	}
};
#undef T

/***
 * Updates the baggage context after modifying baggage entries, then returns
 * from the calling function.  This is a macro because it must call
 * OTEL_SPAN_ERETURN_INT, which returns from the caller on error.
 *
 * SetBaggage() produces a new Context that does not carry the span token, so
 * SetSpan() is called to re-associate the original span with the new Context.
 * Without this step, child spans that reference this span as a parent receive
 * a Context with no valid SpanContext, which causes a SIGSEGV in
 * ParentBasedSampler::ShouldSample().
 */
#define OTEL_SPAN_UPDATE_BAGGAGE(arg_handle, arg_baggage, arg_retval)                          \
	OTEL_DBG_BAGGAGE(arg_baggage);                                                         \
	                                                                                       \
	if ((arg_retval) <= 0)                                                                 \
		OTELC_RETURN_INT(arg_retval);                                                  \
	                                                                                       \
	auto c1_ = otel_baggage::SetBaggage(*((arg_handle)->context), std::move(arg_baggage)); \
	auto c2_ = otel_trace::SetSpan(c1_, otel_trace::GetSpan(*((arg_handle)->context)));    \
	auto c3_ = otel::make_shared_nothrow<otel_context::Context>(std::move(c2_));           \
	if (OTEL_NULL(c3_))                                                                    \
		OTEL_SPAN_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("baggage context"));               \
	                                                                                       \
	(arg_handle)->context = std::move(c3_);                                                \
	                                                                                       \
	OTELC_DBG(OTEL, "new span baggage context set");                                       \
	                                                                                       \
	OTELC_RETURN_INT(arg_retval);                                                          \


#ifdef OTELC_USE_STATIC_HANDLE
extern THREAD_LOCAL struct otel_handle<struct otel_span_handle *, OTEL_HANDLE_SHARED>          otel_span;
extern THREAD_LOCAL struct otel_handle<struct otel_span_context_handle *, OTEL_HANDLE_SHARED>  otel_span_context;
#else
extern THREAD_LOCAL struct otel_handle<struct otel_span_handle *, OTEL_HANDLE_SHARED>         *otel_span;
extern THREAD_LOCAL struct otel_handle<struct otel_span_context_handle *, OTEL_HANDLE_SHARED> *otel_span_context;
#endif


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
