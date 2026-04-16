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
#ifndef _OPENTELEMETRY_C_WRAPPER_EXPORTER_H_
#define _OPENTELEMETRY_C_WRAPPER_EXPORTER_H_

#define OTEL_EXPORTER_ELASTICSEARCH               "elasticsearch"
#define OTEL_EXPORTER_IN_MEMORY                   "memory"
#define OTEL_EXPORTER_OSTREAM                     "ostream"
#define OTEL_EXPORTER_OTLP_FILE                   "otlp_file"
#define OTEL_EXPORTER_OTLP_GRPC                   "otlp_grpc"
#define OTEL_EXPORTER_OTLP_HTTP                   "otlp_http"
#define OTEL_EXPORTER_ZIPKIN                      "zipkin"

#define OTEL_EXPORTER_OSTREAM_STDERR              "stderr"
#define OTEL_EXPORTER_OSTREAM_STDOUT              "stdout"
#define OTEL_EXPORTER_OTLP_FILE_PATTERN           "otel-logfile-%F-%N.txt"

#define OTEL_TRACER_EXPORTER_DESC                 "OpenTelemetry traces exporter"
#define OTEL_TRACER_EXPORTER_NOT_SUPPORTED(s)     "OpenTelemetry " s " traces exporter is not supported"
#define OTEL_TRACER_EXPORTER_FAILED(s)            "Unable to init OpenTelemetry " s " traces exporter"
#define OTEL_TRACER_EXPORTER_OTLP_GRPC_ENDPOINT   "http://localhost:4317/v1/traces"
#define OTEL_TRACER_EXPORTER_OTLP_HTTP_ENDPOINT   "http://localhost:4318/v1/traces"
#define OTEL_TRACER_EXPORTER_ZIPKIN_ENDPOINT      "http://localhost:9411/api/v2/spans"

#define OTEL_METER_EXPORTER_DESC                  "OpenTelemetry metrics exporter"
#define OTEL_METER_EXPORTER_NOT_SUPPORTED(s)      "OpenTelemetry " s " metrics exporter is not supported"
#define OTEL_METER_EXPORTER_FAILED(s)             "Unable to init OpenTelemetry " s " metrics exporter"
#define OTEL_METER_EXPORTER_OTLP_GRPC_ENDPOINT    "http://localhost:4317/v1/metrics"
#define OTEL_METER_EXPORTER_OTLP_HTTP_ENDPOINT    "http://localhost:4318/v1/metrics"

#define OTEL_LOGGER_EXPORTER_DESC                 "OpenTelemetry logs exporter"
#define OTEL_LOGGER_EXPORTER_NOT_SUPPORTED(s)     "OpenTelemetry " s " logs exporter is not supported"
#define OTEL_LOGGER_EXPORTER_FAILED(s)            "Unable to init OpenTelemetry " s " logs exporter"
#define OTEL_LOGGER_EXPORTER_OTLP_GRPC_ENDPOINT   "http://localhost:4317/v1/logs"
#define OTEL_LOGGER_EXPORTER_OTLP_HTTP_ENDPOINT   "http://localhost:4318/v1/logs"

/***
 * Exporter dispatch macros for shared backends.  Each macro expands to an
 * else-if branch that creates the corresponding exporter.  The ifdef-guarded
 * pair selects the supported or unsupported variant at compile time so that
 * callers do not need ifdef guards at the expansion site.
 *
 * All macros assume these locals exist in the caller:
 *   type, exporter_maybe, name
 */
#ifdef HAVE_OTEL_EXPORTER_OSTREAM
  #define OTEL_EXPORTER_CASE_OSTREAM(arg_sig, arg_type, arg_logfile, arg_err)                                                       \
	else if (strcasecmp(type, OTEL_EXPORTER_OSTREAM) == 0) {                                                                    \
		if (otel_exporter_set_ostream_options<arg_type>(OTEL_##arg_sig##_EXPORTER_DESC,                                     \
		                                                OTEL_YAML_##arg_sig##_PREFIX OTEL_YAML_EXPORTERS,                   \
		                                                (arg_logfile), exporter_maybe, (arg_err), name) == OTELC_RET_ERROR) \
			OTELC_RETURN_INT(OTELC_RET_ERROR);                                                                          \
	}
#else
  #define OTEL_EXPORTER_CASE_OSTREAM(arg_sig, arg_type, arg_logfile, arg_err)               \
	else if (strcasecmp(type, OTEL_EXPORTER_OSTREAM) == 0) {                            \
		OTEL_##arg_sig##_ERROR(OTEL_##arg_sig##_EXPORTER_NOT_SUPPORTED("ostream")); \
	}
#endif /* HAVE_OTEL_EXPORTER_OSTREAM */

#ifdef HAVE_OTEL_EXPORTER_OTLP_FILE
  #define OTEL_EXPORTER_CASE_OTLP_FILE(arg_sig, arg_base, arg_err)                                                \
	else if (strcasecmp(type, OTEL_EXPORTER_OTLP_FILE) == 0) {                                                \
		otel_exporter_otlp::arg_base##Options        options{};                                           \
		otel_exporter_otlp::arg_base##RuntimeOptions rt_options{};                                        \
		                                                                                                  \
		if (otel_exporter_set_otlp_file_options(OTEL_##arg_sig##_EXPORTER_DESC,                           \
		                                        OTEL_YAML_##arg_sig##_PREFIX OTEL_YAML_EXPORTERS,         \
		                                        options, rt_options, (arg_err), name) == OTELC_RET_ERROR) \
			OTELC_RETURN_INT(OTELC_RET_ERROR);                                                        \
		exporter_maybe = otel::make_unique_nothrow<otel_exporter_otlp::arg_base>(options, rt_options);    \
		if (OTEL_NULL(exporter_maybe))                                                                    \
			OTEL_##arg_sig##_ERROR(OTEL_##arg_sig##_EXPORTER_FAILED("OTLP File"));                    \
		else                                                                                              \
			OTEL_CAST_STATIC(otel_exporter_otlp::arg_base *, exporter_maybe.get())->MaybeSpawnBackgroundThread(); \
	}
#else
  #define OTEL_EXPORTER_CASE_OTLP_FILE(arg_sig, arg_base, arg_err)                            \
	else if (strcasecmp(type, OTEL_EXPORTER_OTLP_FILE) == 0) {                            \
		OTEL_##arg_sig##_ERROR(OTEL_##arg_sig##_EXPORTER_NOT_SUPPORTED("OTLP File")); \
	}
#endif /* HAVE_OTEL_EXPORTER_OTLP_FILE */

/* <opentelemetry/exporters/otlp/otlp_grpc_client_options.h> */
#ifdef HAVE_OTEL_EXPORTER_OTLP_GRPC
  #define OTEL_EXPORTER_CASE_OTLP_GRPC(arg_sig, arg_base, arg_err)                                                               \
	else if (strcasecmp(type, OTEL_EXPORTER_OTLP_GRPC) == 0) {                                                               \
		otel_exporter_otlp::arg_base##Options options{};                                                                 \
		char                                  endpoint[OTEL_YAML_BUFSIZ] = OTEL_##arg_sig##_EXPORTER_OTLP_GRPC_ENDPOINT; \
		                                                                                                                 \
		if (otel_exporter_set_otlp_grpc_options(OTEL_##arg_sig##_EXPORTER_DESC,                                          \
		                                        OTEL_YAML_##arg_sig##_PREFIX OTEL_YAML_EXPORTERS,                        \
		                                        endpoint, options, (arg_err), name) == OTELC_RET_ERROR)                  \
			OTELC_RETURN_INT(OTELC_RET_ERROR);                                                                       \
		exporter_maybe = otel::make_unique_nothrow<otel_exporter_otlp::arg_base>(options);                               \
		if (OTEL_NULL(exporter_maybe))                                                                                   \
			OTEL_##arg_sig##_ERROR(OTEL_##arg_sig##_EXPORTER_FAILED("OTLP gRPC"));                                   \
		else                                                                                                             \
			OTEL_CAST_STATIC(otel_exporter_otlp::arg_base *, exporter_maybe.get())->MaybeSpawnBackgroundThread();    \
	}
#else
  #define OTEL_EXPORTER_CASE_OTLP_GRPC(arg_sig, arg_base, arg_err)                            \
	else if (strcasecmp(type, OTEL_EXPORTER_OTLP_GRPC) == 0) {                            \
		OTEL_##arg_sig##_ERROR(OTEL_##arg_sig##_EXPORTER_NOT_SUPPORTED("OTLP gRPC")); \
	}
#endif /* HAVE_OTEL_EXPORTER_OTLP_GRPC */

/***
 * <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
 * <opentelemetry/exporters/otlp/otlp_http_exporter_runtime_options.h>
 */
#ifdef HAVE_OTEL_EXPORTER_OTLP_HTTP
  #define OTEL_EXPORTER_CASE_OTLP_HTTP(arg_sig, arg_base, arg_err)                                                                      \
	else if (strcasecmp(type, OTEL_EXPORTER_OTLP_HTTP) == 0) {                                                                      \
		otel_exporter_otlp::arg_base##Options        options{};                                                                 \
		otel_exporter_otlp::arg_base##RuntimeOptions rt_options{};                                                              \
		char                                         endpoint[OTEL_YAML_BUFSIZ] = OTEL_##arg_sig##_EXPORTER_OTLP_HTTP_ENDPOINT; \
		int64_t                                      background_thread_wait_for = 0;                                            \
		                                                                                                                        \
		if (otel_exporter_set_otlp_http_options(OTEL_##arg_sig##_EXPORTER_DESC,                                                 \
		                                        OTEL_YAML_##arg_sig##_PREFIX OTEL_YAML_EXPORTERS,                               \
		                                        endpoint, options, rt_options, &background_thread_wait_for,                     \
		                                        (arg_err), name) == OTELC_RET_ERROR)                                            \
			OTELC_RETURN_INT(OTELC_RET_ERROR);                                                                              \
		exporter_maybe = otel::make_unique_nothrow<otel_exporter_otlp::arg_base>(options, rt_options);                          \
		if (OTEL_NULL(exporter_maybe))                                                                                          \
			OTEL_##arg_sig##_ERROR(OTEL_##arg_sig##_EXPORTER_FAILED("OTLP HTTP"));                                          \
		else {                                                                                                                  \
			auto *exporter_ptr = OTEL_CAST_STATIC(otel_exporter_otlp::arg_base *, exporter_maybe.get());                    \
			exporter_ptr->SetBackgroundWaitFor(std::chrono::milliseconds((background_thread_wait_for > 0) ?                 \
			                                   background_thread_wait_for : std::chrono::milliseconds::max().count()));     \
			exporter_ptr->MaybeSpawnBackgroundThread();                                                                     \
		}                                                                                                                       \
	}
#else
  #define OTEL_EXPORTER_CASE_OTLP_HTTP(arg_sig, arg_base, arg_err)                            \
	else if (strcasecmp(type, OTEL_EXPORTER_OTLP_HTTP) == 0) {                            \
		OTEL_##arg_sig##_ERROR(OTEL_##arg_sig##_EXPORTER_NOT_SUPPORTED("OTLP HTTP")); \
	}
#endif /* HAVE_OTEL_EXPORTER_OTLP_HTTP */


int  otel_tracer_exporter_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter, const char *name = nullptr);
void otel_tracer_exporter_destroy(void);
int  otel_meter_exporter_create(struct otelc_meter *meter, std::unique_ptr<otel_sdk_metrics::PushMetricExporter> &exporter, const char *name = nullptr);
void otel_meter_exporter_destroy(void);
int  otel_logger_exporter_create(struct otelc_logger *logger, std::unique_ptr<otel_sdk_logs::LogRecordExporter> &exporter, const char *name = nullptr);
void otel_logger_exporter_destroy(void);

#endif /* _OPENTELEMETRY_C_WRAPPER_EXPORTER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
