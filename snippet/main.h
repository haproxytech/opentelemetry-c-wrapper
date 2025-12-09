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
#ifndef TEST_CMAKE_MAIN_H
#define TEST_CMAKE_MAIN_H

#define HAVE_OTEL_EXPORTER_OSTREAM
#define HAVE_OTEL_EXPORTER_OTLP_HTTP


#include <cinttypes>
#include <fstream>

#include <opentelemetry/trace/provider.h>
#include <opentelemetry/baggage/propagation/baggage_propagator.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/sdk/common/global_log_handler.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/samplers/trace_id_ratio.h>
#include <opentelemetry/sdk/resource/resource_detector.h>

#ifdef HAVE_OTEL_EXPORTER_OSTREAM
#  include <opentelemetry/exporters/ostream/span_exporter.h>
#endif
#ifdef HAVE_OTEL_EXPORTER_OTLP_HTTP
#  include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#endif


#define OTEL_NULL(a)             ((a) == nullptr)
#define OTELC_STR_ARG(s)         (OTEL_NULL(s) ? "(null)" : (s))

#define OTELC_SCOPE_VERSION      "1.2.0"
#define OTELC_SCOPE_SCHEMA_URL   "https://opentelemetry.io/schemas/1.2.0"

#define OTELC_DBG(f, ...)        (void)printf(f "\n", ##__VA_ARGS__)
#define OTELC_FUNC(f, ...)       OTELC_DBG("%s(" f ") {", __func__, ##__VA_ARGS__)
#define OTELC_RETURN()           do { OTELC_DBG("}"); return; } while (0)
#define OTELC_RETURN_EX(a,t,f)   do { t _r = (a); OTELC_DBG("} = " f, _r); return _r; } while (0)
#define OTELC_RETURN_INT(a)      OTELC_RETURN_EX((a), int, "%d")
#define OTELC_RETURN_PTR(a)      OTELC_RETURN_EX((a), typeof(a), "%p")


namespace otel_baggage          = OPENTELEMETRY_NAMESPACE::baggage;
namespace otel_common           = OPENTELEMETRY_NAMESPACE::common;
namespace otel_context          = OPENTELEMETRY_NAMESPACE::context;
namespace otel_exporter         = OPENTELEMETRY_NAMESPACE::exporter;
namespace otel_nostd            = OPENTELEMETRY_NAMESPACE::nostd;
namespace otel_trace            = OPENTELEMETRY_NAMESPACE::trace;
namespace otel_sdk_common       = OPENTELEMETRY_NAMESPACE::sdk::common;
namespace otel_sdk_trace        = OPENTELEMETRY_NAMESPACE::sdk::trace;
namespace otel_sdk_resource     = OPENTELEMETRY_NAMESPACE::sdk::resource;
namespace otel_sdk_internal_log = otel_sdk_common::internal_log;
#ifdef HAVE_OTEL_EXPORTER_OTLP_HTTP
namespace otel_exporter_otlp    = otel_exporter::otlp;
#endif

using otel_system_timestamp = otel_common::SystemTimestamp;
using otel_steady_timestamp = otel_common::SteadyTimestamp;


#define T   otel_span_handle
struct T {
	otel_nostd::shared_ptr<otel_trace::Span>      span;
	otel_nostd::shared_ptr<otel_trace::Scope>     scope;
	otel_nostd::shared_ptr<otel_context::Context> context;

	~T() noexcept
	{
		OTELC_FUNC("");

		context = nullptr;
		scope   = nullptr;
		span    = nullptr;

		OTELC_RETURN();
	}
};
#undef T

class otel_hash_function {
	public:
	size_t operator() (int64_t id) const noexcept { return id; }
};
class otel_key_eq {
	public:
	bool operator() (int64_t id_a, int64_t id_b) const noexcept { return id_a == id_b; }
};
template<typename T>
struct otel_handle {
	std::unordered_map<
		int64_t,
		T,
		otel_hash_function,
		otel_key_eq
	>          handle;
	int64_t    id;
	int64_t    alloc_fail_cnt;
	int64_t    erase_cnt;
	int64_t    destroy_cnt;
	bool       is_initialized;
	std::mutex mutex;

	bool is_valid(int64_t key) noexcept
	{
		if (!is_initialized)
			return false;

		auto iter = handle.find(key);
		return iter != handle.end();
	}
};

#define OTEL_SPAN_HANDLE(a)            (otel_span.handle.at(a))
#define OTEL_SPAN_HANDLE_SPAN(a)       (OTEL_SPAN_HANDLE(a)->span)
#define OTEL_SPAN_HANDLE_CONTEXT(a)    (OTEL_SPAN_HANDLE(a)->context)
#define OTEL_HANDLE_IS_VALID(h,n)      ((h).is_valid(n))

#endif /* TEST_CMAKE_MAIN_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
