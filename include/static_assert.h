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
#ifndef _OPENTELEMETRY_C_WRAPPER_STATIC_ASSERT_H_
#define _OPENTELEMETRY_C_WRAPPER_STATIC_ASSERT_H_

/***
 * Verify that otelc_span_kind_t values (offset by 1) match the C++ SpanKind
 * enum.
 */
#ifdef OTELC_STATIC_ASSERT_TRACER
static_assert((OTELC_SPAN_KIND_INTERNAL - 1) == OTEL_CAST_STATIC(int, otel_trace::SpanKind::kInternal), "INTERNAL mismatch");
static_assert((OTELC_SPAN_KIND_SERVER   - 1) == OTEL_CAST_STATIC(int, otel_trace::SpanKind::kServer),   "SERVER mismatch"  );
static_assert((OTELC_SPAN_KIND_CLIENT   - 1) == OTEL_CAST_STATIC(int, otel_trace::SpanKind::kClient),   "CLIENT mismatch"  );
static_assert((OTELC_SPAN_KIND_PRODUCER - 1) == OTEL_CAST_STATIC(int, otel_trace::SpanKind::kProducer), "PRODUCER mismatch");
static_assert((OTELC_SPAN_KIND_CONSUMER - 1) == OTEL_CAST_STATIC(int, otel_trace::SpanKind::kConsumer), "CONSUMER mismatch");
#endif

/***
 * Verify that otelc_metric_aggregation_type_t values match the C++
 * AggregationType enum.
 */
#ifdef OTELC_STATIC_ASSERT_METER
#  define OTELC_METRIC_AGGREGATION_DEF(a,b,c)   static_assert(OTELC_METRIC_AGGREGATION_##a == OTEL_CAST_STATIC(int, otel_sdk_metrics::AggregationType::b), #a " mismatch");
OTELC_METRIC_AGGREGATION_DEFINES
#  undef OTELC_METRIC_AGGREGATION_DEF
#endif

#endif /* _OPENTELEMETRY_C_WRAPPER_STATIC_ASSERT_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
