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
#ifndef _OPENTELEMETRY_C_WRAPPER_INCLUDE_H_
#define _OPENTELEMETRY_C_WRAPPER_INCLUDE_H_

#include "config.h"

#ifdef HAVE_MALLOC_H
#  include <malloc.h>
#endif
#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdarg.h>
#include <sys/syscall.h>

#ifdef HAVE_LIBFYAML_H
#  include <libfyaml.h>
#endif

#include <cinttypes>
#include <fstream>
#include <utility>
#include <stdexcept>

/* Common */
#include <opentelemetry/baggage/propagation/baggage_propagator.h>
#include <opentelemetry/context/propagation/composite_propagator.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/sdk/resource/resource_detector.h>
#include <opentelemetry/sdk/common/thread_instrumentation.h>

/* Traces */
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/span_context_kv_iterable.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/samplers/parent.h>
#include <opentelemetry/sdk/trace/samplers/trace_id_ratio.h>

/* Metrics */
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/provider.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/metrics/instrument_metadata_validator.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>

#ifdef HAVE_OTEL_EXPORTER_IN_MEMORY
#  include <opentelemetry/exporters/memory/in_memory_span_exporter.h>
#  include <opentelemetry/exporters/memory/in_memory_metric_exporter_factory.h>
#endif
#ifdef HAVE_OTEL_EXPORTER_OSTREAM
#  include <opentelemetry/exporters/ostream/span_exporter.h>
#  include <opentelemetry/exporters/ostream/metric_exporter.h>
#endif
#ifdef HAVE_OTEL_EXPORTER_OTLP_FILE
#  include <opentelemetry/exporters/otlp/otlp_file_client_options.h>
#  include <opentelemetry/exporters/otlp/otlp_file_exporter.h>
#  include <opentelemetry/exporters/otlp/otlp_file_metric_exporter.h>
#endif
#ifdef HAVE_OTEL_EXPORTER_OTLP_GRPC
#  include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>
#  include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter.h>
#endif
#ifdef HAVE_OTEL_EXPORTER_OTLP_HTTP
#  include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#  include <opentelemetry/exporters/otlp/otlp_http_metric_exporter.h>
#endif
#ifdef HAVE_OTEL_EXPORTER_ZIPKIN
#  include <opentelemetry/exporters/zipkin/zipkin_exporter.h>
#endif

#include "configuration.h"
#include "compiler.h"
#include "define.h"
#ifdef DEBUG
#  include "dbg_malloc.h"
#endif
#include "opentelemetry-c-wrapper/include.h"
#include "std.h"
#include "util.h"
#include "exporter.h"
#include "processor.h"
#include "propagation.h"
#include "provider.h"
#include "resource.h"
#include "sampler.h"
#include "span.h"
#include "meter.h"
#include "threads.h"
#include "tracer.h"
#include "yaml.h"
#include "static_assert.h"

#endif /* _OPENTELEMETRY_C_WRAPPER_INCLUDE_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
