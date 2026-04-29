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
#ifndef _OPENTELEMETRY_C_WRAPPER_PROVIDER_H_
#define _OPENTELEMETRY_C_WRAPPER_PROVIDER_H_

#define OTEL_IMPL(t,p)             OTEL_CAST_STATIC(struct otel_##t##_impl *, (p)->impl)

/***
 * Downcast helpers that obtain the concrete SDK provider pointer from a
 * shared_ptr to the API-level provider, so that SDK-only operations such as
 * ForceFlush() and Shutdown() can be invoked.  The argument is the shared_ptr
 * itself rather than the impl, so callers can pass a local snapshot whose
 * lifetime is bounded by the call instead of the impl member that a
 * concurrent destroy may reset.
 */
#define OTEL_TRACER_PROVIDER(s)    OTEL_CAST_DYNAMIC(otel_sdk_trace::TracerProvider *, (s).get())
#define OTEL_METER_PROVIDER(s)     OTEL_CAST_DYNAMIC(otel_sdk_metrics::MeterProvider *, (s).get())
#define OTEL_LOGGER_PROVIDER(s)    OTEL_CAST_DYNAMIC(otel_sdk_logs::LoggerProvider *, (s).get())

/***
 * Generates a provider operation function (ForceFlush or Shutdown) that
 * forwards the call to the per-instance OpenTelemetry SDK provider held in the
 * instance's implementation state.  The shared_ptr is copied to a local before
 * the SDK pointer is obtained, so a concurrent destroy that resets the impl
 * member cannot release the SDK provider mid-call.
 */
#define OTEL_PROVIDER_OP(arg_signal, arg_ptr, arg_operation, arg_msg)                                                     \
	OTELC_FUNC("%p, %p", (arg_ptr), timeout);                                                                         \
	                                                                                                                  \
	if (OTEL_NULL(arg_ptr))                                                                                           \
		OTELC_RETURN_INT(OTELC_RET_ERROR);                                                                        \
	                                                                                                                  \
	auto *impl = OTEL_IMPL(arg_ptr, arg_ptr);                                                                         \
	if (OTEL_NULL(impl))                                                                                              \
		OTEL_##arg_signal##_RETURN_INT(arg_msg);                                                                  \
	                                                                                                                  \
	/* Copy the provider so it cannot be released mid-call. */                                                        \
	auto       provider_shared = impl->provider;                                                                      \
	const auto provider_sdk    = OTEL_##arg_signal##_PROVIDER(provider_shared);                                       \
	if (!OTEL_NULL(provider_sdk)) {                                                                                   \
		const auto us = OTEL_NULL(timeout) ? std::chrono::microseconds::max() : timespec_to_duration_us(timeout); \
		                                                                                                          \
		if (provider_sdk->arg_operation(us))                                                                      \
			OTELC_RETURN_INT(OTELC_RET_OK);                                                                   \
	}                                                                                                                 \
	                                                                                                                  \
	OTEL_##arg_signal##_RETURN_INT(arg_msg);


int  otel_tracer_provider_create(struct otelc_tracer *tracer, std::vector<std::unique_ptr<otel_sdk_trace::SpanProcessor>> &processors, std::unique_ptr<otel_sdk_trace::Sampler> &sampler, std::unique_ptr<otel_trace::TracerProvider> &provider);
int  otel_tracer_provider_get(struct otelc_tracer *tracer, otel_nostd::shared_ptr<otel_trace::TracerProvider> &provider);
int  otel_meter_reader_create(struct otelc_meter *meter, std::unique_ptr<otel_sdk_metrics::PushMetricExporter> &exporter, std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader> &reader, const char *name = nullptr);
int  otel_meter_provider_create(struct otelc_meter *meter, std::vector<std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader>> &readers, std::shared_ptr<otel_metrics::MeterProvider> &provider);
int  otel_logger_provider_create(struct otelc_logger *logger, std::vector<std::unique_ptr<otel_sdk_logs::LogRecordProcessor>> &processors, std::shared_ptr<otel_logs::LoggerProvider> &provider);

#endif /* _OPENTELEMETRY_C_WRAPPER_PROVIDER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
