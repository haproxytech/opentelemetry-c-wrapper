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
 * Downcast helpers used by OTEL_PROVIDER_OP to obtain the concrete SDK
 * provider pointer from the API-level provider held in the per-instance
 * implementation state, so that SDK-only operations such as ForceFlush()
 * and Shutdown() can be invoked.
 */
#define OTEL_TRACER_PROVIDER(p)    OTEL_CAST_DYNAMIC(otel_sdk_trace::TracerProvider *, (p)->provider.get())
#define OTEL_METER_PROVIDER(p)     OTEL_CAST_DYNAMIC(otel_sdk_metrics::MeterProvider *, (p)->provider.get())
#define OTEL_LOGGER_PROVIDER(p)    OTEL_CAST_DYNAMIC(otel_sdk_logs::LoggerProvider *, (p)->provider.get())

/***
 * Generates a provider operation function (ForceFlush or Shutdown) that
 * forwards the call to the per-instance OpenTelemetry SDK provider held in the
 * instance's implementation state.
 */
#define OTEL_PROVIDER_OP(arg_signal, arg_ptr, arg_operation, arg_msg)                                                     \
	OTELC_FUNC("%p, %p", (arg_ptr), timeout);                                                                         \
	                                                                                                                  \
	if (OTEL_NULL(arg_ptr))                                                                                           \
		OTELC_RETURN_INT(OTELC_RET_ERROR);                                                                        \
	                                                                                                                  \
	auto *impl              = OTEL_IMPL(arg_ptr, arg_ptr);                                                            \
	const auto provider_sdk = OTEL_NULL(impl) ? nullptr : OTEL_##arg_signal##_PROVIDER(impl);                         \
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
