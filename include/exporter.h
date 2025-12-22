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


int  otel_tracer_exporter_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter);
void otel_tracer_exporter_destroy(void);
int  otel_meter_exporter_create(struct otelc_meter *meter, std::unique_ptr<otel_sdk_metrics::PushMetricExporter> &exporter);
void otel_meter_exporter_destroy(void);
int  otel_logger_exporter_create(struct otelc_logger *logger, std::unique_ptr<otel_sdk_logs::LogRecordExporter> &exporter);
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
