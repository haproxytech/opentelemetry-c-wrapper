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


std::atomic<uint64_t> otel_counting_span_processor::dropped_count_{0};
std::atomic<uint64_t> otel_counting_log_processor::dropped_count_{0};
std::atomic<uint64_t> otel_counting_span_exporter::export_ok_{0};
std::atomic<uint64_t> otel_counting_span_exporter::export_fail_{0};
std::atomic<uint64_t> otel_counting_span_exporter::records_ok_{0};
std::atomic<uint64_t> otel_counting_span_exporter::records_fail_{0};
std::atomic<int64_t>  otel_counting_span_exporter::last_export_ms_{0};
std::atomic<uint64_t> otel_counting_log_exporter::export_ok_{0};
std::atomic<uint64_t> otel_counting_log_exporter::export_fail_{0};
std::atomic<uint64_t> otel_counting_log_exporter::records_ok_{0};
std::atomic<uint64_t> otel_counting_log_exporter::records_fail_{0};
std::atomic<int64_t>  otel_counting_log_exporter::last_export_ms_{0};
std::atomic<uint64_t> otel_counting_metric_exporter::export_ok_{0};
std::atomic<uint64_t> otel_counting_metric_exporter::export_fail_{0};
std::atomic<int64_t>  otel_counting_metric_exporter::last_export_ms_{0};

std::atomic<otel_counting_span_processor *> otel_counting_span_processor::instance_{nullptr};
std::atomic<otel_counting_log_processor *>  otel_counting_log_processor::instance_{nullptr};


/***
 * NAME
 *   otel_steady_now_ms - current steady-clock time in milliseconds
 *
 * DESCRIPTION
 *   Returns the steady (monotonic) clock reading in milliseconds.  Used to
 *   stamp the time of the last successful export and to compute its age; the
 *   steady clock is immune to wall-clock adjustments, so the age never goes
 *   backwards.
 *
 * RETURN VALUE
 *   Returns the elapsed milliseconds of the steady clock epoch.
 */
static int64_t otel_steady_now_ms()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}


/***
 * NAME
 *   otel_counting_span_exporter - exporter wrapper that counts exported spans
 *
 * DESCRIPTION
 *   Decorates a SpanExporter to track how many span records the batch processor
 *   has dequeued for export.  Before each inner Export() call, the wrapper
 *   atomically increments a shared consumed counter by the number of spans in
 *   the batch.  Incrementing before the actual export minimizes the window in
 *   which the delegating processor overestimates queue depth, since the batch
 *   processor frees its circular buffer slots before invoking the exporter.
 *
 *   All SpanExporter interface methods are forwarded to the wrapped exporter
 *   unchanged.
 */
otel_counting_span_exporter::otel_counting_span_exporter(std::unique_ptr<otel_sdk_trace::SpanExporter> &&exporter, std::shared_ptr<std::atomic<uint64_t>> consumed)
	: inner_(std::move(exporter)), consumed_(std::move(consumed))
{
}


std::unique_ptr<otel_sdk_trace::Recordable> otel_counting_span_exporter::MakeRecordable() noexcept
{
	return inner_->MakeRecordable();
}


otel_sdk_common::ExportResult otel_counting_span_exporter::Export(const otel_nostd::span<std::unique_ptr<otel_sdk_trace::Recordable>> &spans) noexcept
{
	consumed_->fetch_add(spans.size(), std::memory_order_relaxed);

	otel_sdk_common::ExportResult result = inner_->Export(spans);

	if (result == otel_sdk_common::ExportResult::kSuccess) {
		export_ok_.fetch_add(1, std::memory_order_relaxed);
		records_ok_.fetch_add(spans.size(), std::memory_order_relaxed);
		last_export_ms_.store(otel_steady_now_ms(), std::memory_order_relaxed);
	} else {
		export_fail_.fetch_add(1, std::memory_order_relaxed);
		records_fail_.fetch_add(spans.size(), std::memory_order_relaxed);
	}

	return result;
}


/***
 * NAME
 *   otel_counting_span_exporter::last_export_age_ms - age of the last export
 *
 * DESCRIPTION
 *   Returns the number of milliseconds since the most recent successful span
 *   export, measured on the steady clock.  The value is process-wide across
 *   all counting span exporter instances.
 *
 * RETURN VALUE
 *   Returns the age in milliseconds, or -1 when no export has yet succeeded.
 */
int64_t otel_counting_span_exporter::last_export_age_ms() noexcept
{
	int64_t stamp = last_export_ms_.load(std::memory_order_relaxed);

	if (stamp == 0)
		return -1;

	int64_t now = otel_steady_now_ms();

	return (now > stamp) ? (now - stamp) : 0;
}


bool otel_counting_span_exporter::ForceFlush(std::chrono::microseconds timeout) noexcept
{
	return inner_->ForceFlush(timeout);
}


bool otel_counting_span_exporter::Shutdown(std::chrono::microseconds timeout) noexcept
{
	return inner_->Shutdown(timeout);
}


/***
 * NAME
 *   otel_counting_span_processor - delegating processor that counts dropped spans
 *
 * DESCRIPTION
 *   Wraps a BatchSpanProcessor and delegates all SpanProcessor interface
 *   calls to it.  In OnEnd(), the processor compares a local produced counter
 *   against the shared consumed counter maintained by the counting exporter.
 *   When the difference reaches the configured max_queue_size, the span is
 *   considered dropped and a process-wide static drop counter is incremented;
 *   otherwise the produced counter is incremented.  A guard ensures unsigned
 *   subtraction is skipped when consumed has raced past produced, preventing
 *   wraparound and allowing the counters to self-correct.
 *
 *   The drop counter is shared across all otel_counting_span_processor
 *   instances and can be queried via the static dropped_count() method or
 *   the C-linkage function otelc_pipeline_status_get().
 */
otel_counting_span_processor::otel_counting_span_processor(std::unique_ptr<otel_sdk_trace::BatchSpanProcessor> &&inner, std::shared_ptr<std::atomic<uint64_t>> consumed, size_t max_queue_size)
	: consumed_(std::move(consumed)), max_queue_size_(max_queue_size), inner_(std::move(inner))
{
	instance_.store(this, std::memory_order_release);
}


otel_counting_span_processor::~otel_counting_span_processor()
{
	otel_counting_span_processor *self = this;

	instance_.compare_exchange_strong(self, nullptr, std::memory_order_acq_rel);
}


/***
 * NAME
 *   otel_counting_span_processor::queue_depth - current span queue depth
 *
 * DESCRIPTION
 *   Returns how many spans are currently buffered in the batch processor of the
 *   most recently created counting span processor, computed as produced minus
 *   consumed.  The guard yields 0 when consumed has momentarily raced past
 *   produced.
 *
 * RETURN VALUE
 *   Returns the queue depth, or 0 when no counting span processor exists.
 */
int64_t otel_counting_span_processor::queue_depth() noexcept
{
	otel_counting_span_processor *p = instance_.load(std::memory_order_acquire);
	uint64_t                      prod, cons;

	if (p == nullptr)
		return 0;

	prod = p->produced_.load(std::memory_order_relaxed);
	cons = p->consumed_->load(std::memory_order_relaxed);

	return (prod > cons) ? OTEL_CAST_STATIC(int64_t, prod - cons) : 0;
}


/***
 * NAME
 *   otel_counting_span_processor::queue_capacity - configured span queue size
 *
 * DESCRIPTION
 *   Returns the maximum queue size (max_queue_size) of the most recently
 *   created counting span processor.
 *
 * RETURN VALUE
 *   Returns the queue capacity, or 0 when no counting span processor exists.
 */
int64_t otel_counting_span_processor::queue_capacity() noexcept
{
	otel_counting_span_processor *p = instance_.load(std::memory_order_acquire);

	return (p == nullptr) ? 0 : OTEL_CAST_STATIC(int64_t, p->max_queue_size_);
}


std::unique_ptr<otel_sdk_trace::Recordable> otel_counting_span_processor::MakeRecordable() noexcept
{
	return inner_->MakeRecordable();
}


void otel_counting_span_processor::OnStart(otel_sdk_trace::Recordable &span, const otel_trace::SpanContext &parent_context) noexcept
{
	inner_->OnStart(span, parent_context);
}


void otel_counting_span_processor::OnEnd(std::unique_ptr<otel_sdk_trace::Recordable> &&span) noexcept
{
	if (OTEL_NULL(span)) {
		inner_->OnEnd(std::move(span));
		return;
	}

	uint64_t prod = produced_.load(std::memory_order_relaxed);
	uint64_t cons = consumed_->load(std::memory_order_relaxed);

	if ((prod > cons) && ((prod - cons) >= max_queue_size_))
		dropped_count_.fetch_add(1, std::memory_order_relaxed);
	else
		produced_.fetch_add(1, std::memory_order_relaxed);

	inner_->OnEnd(std::move(span));
}


bool otel_counting_span_processor::ForceFlush(std::chrono::microseconds timeout) noexcept
{
	return inner_->ForceFlush(timeout);
}


bool otel_counting_span_processor::Shutdown(std::chrono::microseconds timeout) noexcept
{
	return inner_->Shutdown(timeout);
}


/***
 * NAME
 *   otel_counting_log_exporter - exporter wrapper that counts exported log records
 *
 * DESCRIPTION
 *   Decorates a LogRecordExporter to track how many log records the batch
 *   processor has dequeued for export.  Before each inner Export() call, the
 *   wrapper atomically increments a shared consumed counter by the number of
 *   records in the batch.  Incrementing before the actual export minimizes the
 *   window in which the delegating processor overestimates queue depth, since
 *   the batch processor frees its buffer slots before invoking the exporter.
 *
 *   All LogRecordExporter interface methods are forwarded to the wrapped
 *   exporter unchanged.
 */
otel_counting_log_exporter::otel_counting_log_exporter(std::unique_ptr<otel_sdk_logs::LogRecordExporter> &&exporter, std::shared_ptr<std::atomic<uint64_t>> consumed)
	: inner_(std::move(exporter)), consumed_(std::move(consumed))
{
}


std::unique_ptr<otel_sdk_logs::Recordable> otel_counting_log_exporter::MakeRecordable() noexcept
{
	return inner_->MakeRecordable();
}


otel_sdk_common::ExportResult otel_counting_log_exporter::Export(const otel_nostd::span<std::unique_ptr<otel_sdk_logs::Recordable>> &records) noexcept
{
	consumed_->fetch_add(records.size(), std::memory_order_relaxed);

	otel_sdk_common::ExportResult result = inner_->Export(records);

	if (result == otel_sdk_common::ExportResult::kSuccess) {
		export_ok_.fetch_add(1, std::memory_order_relaxed);
		records_ok_.fetch_add(records.size(), std::memory_order_relaxed);
		last_export_ms_.store(otel_steady_now_ms(), std::memory_order_relaxed);
	} else {
		export_fail_.fetch_add(1, std::memory_order_relaxed);
		records_fail_.fetch_add(records.size(), std::memory_order_relaxed);
	}

	return result;
}


/***
 * NAME
 *   otel_counting_log_exporter::last_export_age_ms - age of the last export
 *
 * DESCRIPTION
 *   Returns the number of milliseconds since the most recent successful log
 *   record export, measured on the steady clock.  The value is process-wide
 *   across all counting log exporter instances.
 *
 * RETURN VALUE
 *   Returns the age in milliseconds, or -1 when no export has yet succeeded.
 */
int64_t otel_counting_log_exporter::last_export_age_ms() noexcept
{
	int64_t stamp = last_export_ms_.load(std::memory_order_relaxed);

	if (stamp == 0)
		return -1;

	int64_t now = otel_steady_now_ms();

	return (now > stamp) ? (now - stamp) : 0;
}


bool otel_counting_log_exporter::ForceFlush(std::chrono::microseconds timeout) noexcept
{
	return inner_->ForceFlush(timeout);
}


bool otel_counting_log_exporter::Shutdown(std::chrono::microseconds timeout) noexcept
{
	return inner_->Shutdown(timeout);
}


/***
 * NAME
 *   otel_counting_log_processor - delegating processor that counts dropped log records
 *
 * DESCRIPTION
 *   Wraps a BatchLogRecordProcessor and delegates all LogRecordProcessor
 *   interface calls to it.  In OnEmit(), the processor compares a local
 *   produced counter against the shared consumed counter maintained by the
 *   counting log exporter.  When the difference reaches the configured
 *   max_queue_size, the record is considered dropped and a process-wide
 *   static drop counter is incremented; otherwise the produced counter is
 *   incremented.  A guard ensures unsigned subtraction is skipped when
 *   consumed has raced past produced, preventing wraparound and allowing
 *   the counters to self-correct.
 *
 *   The drop counter is shared across all otel_counting_log_processor instances
 *   and can be queried via the static dropped_count() method or the C-linkage
 *   function otelc_pipeline_status_get().
 */

otel_counting_log_processor::otel_counting_log_processor(std::unique_ptr<otel_sdk_logs::BatchLogRecordProcessor> &&inner, std::shared_ptr<std::atomic<uint64_t>> consumed, size_t max_queue_size)
	: consumed_(std::move(consumed)), max_queue_size_(max_queue_size), inner_(std::move(inner))
{
	instance_.store(this, std::memory_order_release);
}


otel_counting_log_processor::~otel_counting_log_processor()
{
	otel_counting_log_processor *self = this;

	instance_.compare_exchange_strong(self, nullptr, std::memory_order_acq_rel);
}


/***
 * NAME
 *   otel_counting_log_processor::queue_depth - current log queue depth
 *
 * DESCRIPTION
 *   Returns how many log records are currently buffered in the batch processor
 *   of the most recently created counting log processor, computed as produced
 *   minus consumed.  The guard yields 0 when consumed has momentarily raced
 *   past produced.
 *
 * RETURN VALUE
 *   Returns the queue depth, or 0 when no counting log processor exists.
 */
int64_t otel_counting_log_processor::queue_depth() noexcept
{
	otel_counting_log_processor *p = instance_.load(std::memory_order_acquire);
	uint64_t                     prod, cons;

	if (p == nullptr)
		return 0;

	prod = p->produced_.load(std::memory_order_relaxed);
	cons = p->consumed_->load(std::memory_order_relaxed);

	return (prod > cons) ? OTEL_CAST_STATIC(int64_t, prod - cons) : 0;
}


/***
 * NAME
 *   otel_counting_log_processor::queue_capacity - configured log queue size
 *
 * DESCRIPTION
 *   Returns the maximum queue size (max_queue_size) of the most recently
 *   created counting log processor.
 *
 * RETURN VALUE
 *   Returns the queue capacity, or 0 when no counting log processor exists.
 */
int64_t otel_counting_log_processor::queue_capacity() noexcept
{
	otel_counting_log_processor *p = instance_.load(std::memory_order_acquire);

	return (p == nullptr) ? 0 : OTEL_CAST_STATIC(int64_t, p->max_queue_size_);
}


std::unique_ptr<otel_sdk_logs::Recordable> otel_counting_log_processor::MakeRecordable() noexcept
{
	return inner_->MakeRecordable();
}


void otel_counting_log_processor::OnEmit(std::unique_ptr<otel_sdk_logs::Recordable> &&record) noexcept
{
	if (OTEL_NULL(record)) {
		inner_->OnEmit(std::move(record));

		return;
	}

	uint64_t prod = produced_.load(std::memory_order_relaxed);
	uint64_t cons = consumed_->load(std::memory_order_relaxed);

	if ((prod > cons) && ((prod - cons) >= max_queue_size_))
		dropped_count_.fetch_add(1, std::memory_order_relaxed);
	else
		produced_.fetch_add(1, std::memory_order_relaxed);

	inner_->OnEmit(std::move(record));
}


bool otel_counting_log_processor::ForceFlush(std::chrono::microseconds timeout) noexcept
{
	return inner_->ForceFlush(timeout);
}


bool otel_counting_log_processor::Shutdown(std::chrono::microseconds timeout) noexcept
{
	return inner_->Shutdown(timeout);
}


/***
 * NAME
 *   otel_counting_metric_exporter - exporter wrapper that counts metric exports
 *
 * DESCRIPTION
 *   Decorates a PushMetricExporter to record the outcome of each periodic
 *   metric export.  The periodic reader calls Export() once per export
 *   interval; this wrapper tallies the success and failure results and stamps
 *   the time of the last successful export.  The other PushMetricExporter
 *   methods are forwarded to the wrapped exporter unchanged.
 */
otel_counting_metric_exporter::otel_counting_metric_exporter(std::unique_ptr<otel_sdk_metrics::PushMetricExporter> &&exporter)
	: inner_(std::move(exporter))
{
}


otel_sdk_common::ExportResult otel_counting_metric_exporter::Export(const otel_sdk_metrics::ResourceMetrics &data) noexcept
{
	otel_sdk_common::ExportResult result = inner_->Export(data);

	if (result == otel_sdk_common::ExportResult::kSuccess) {
		export_ok_.fetch_add(1, std::memory_order_relaxed);
		last_export_ms_.store(otel_steady_now_ms(), std::memory_order_relaxed);
	} else {
		export_fail_.fetch_add(1, std::memory_order_relaxed);
	}

	return result;
}


otel_sdk_metrics::AggregationTemporality otel_counting_metric_exporter::GetAggregationTemporality(otel_sdk_metrics::InstrumentType instrument_type) const noexcept
{
	return inner_->GetAggregationTemporality(instrument_type);
}


bool otel_counting_metric_exporter::ForceFlush(std::chrono::microseconds timeout) noexcept
{
	return inner_->ForceFlush(timeout);
}


bool otel_counting_metric_exporter::Shutdown(std::chrono::microseconds timeout) noexcept
{
	return inner_->Shutdown(timeout);
}


/***
 * NAME
 *   otel_counting_metric_exporter::last_export_age_ms - age of the last export
 *
 * DESCRIPTION
 *   Returns the number of milliseconds since the most recent successful metric
 *   export, measured on the steady clock.  The value is process-wide across
 *   all counting metric exporter instances.
 *
 * RETURN VALUE
 *   Returns the age in milliseconds, or -1 when no export has yet succeeded.
 */
int64_t otel_counting_metric_exporter::last_export_age_ms() noexcept
{
	int64_t stamp = last_export_ms_.load(std::memory_order_relaxed);

	if (stamp == 0)
		return -1;

	int64_t now = otel_steady_now_ms();

	return (now > stamp) ? (now - stamp) : 0;
}


/***
 * NAME
 *   otelc_pipeline_status_get - export-pipeline status for all signals
 *
 * SYNOPSIS
 *   void otelc_pipeline_status_get(struct otelc_pipeline_status *status)
 *
 * ARGUMENTS
 *   status - pointer to a caller-allocated structure to fill
 *
 * DESCRIPTION
 *   Fills the caller-provided otelc_pipeline_status with the drop count,
 *   batch-queue depth and capacity, successful and failed export counts, the
 *   records carried by successful and failed exports, and the age of the last
 *   successful export for the trace, log, and metric signals.  Metrics are
 *   driven by a periodic reader rather than a batch processor, so their
 *   dropped, queue_depth, queue_capacity, records_ok, and records_fail fields
 *   are reported as -1.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_pipeline_status_get(struct otelc_pipeline_status *status)
{
	OTELC_FUNC("%p", status);

	if (status == nullptr)
		OTELC_RETURN();

	status->traces.dropped         = OTEL_CAST_STATIC(int64_t, otel_counting_span_processor::dropped_count());
	status->traces.queue_depth     = otel_counting_span_processor::queue_depth();
	status->traces.queue_capacity  = otel_counting_span_processor::queue_capacity();
	status->traces.export_ok       = OTEL_CAST_STATIC(int64_t, otel_counting_span_exporter::export_count(true));
	status->traces.export_fail     = OTEL_CAST_STATIC(int64_t, otel_counting_span_exporter::export_count(false));
	status->traces.records_ok      = OTEL_CAST_STATIC(int64_t, otel_counting_span_exporter::record_count(true));
	status->traces.records_fail    = OTEL_CAST_STATIC(int64_t, otel_counting_span_exporter::record_count(false));
	status->traces.last_export_ms  = otel_counting_span_exporter::last_export_age_ms();

	status->logs.dropped           = OTEL_CAST_STATIC(int64_t, otel_counting_log_processor::dropped_count());
	status->logs.queue_depth       = otel_counting_log_processor::queue_depth();
	status->logs.queue_capacity    = otel_counting_log_processor::queue_capacity();
	status->logs.export_ok         = OTEL_CAST_STATIC(int64_t, otel_counting_log_exporter::export_count(true));
	status->logs.export_fail       = OTEL_CAST_STATIC(int64_t, otel_counting_log_exporter::export_count(false));
	status->logs.records_ok        = OTEL_CAST_STATIC(int64_t, otel_counting_log_exporter::record_count(true));
	status->logs.records_fail      = OTEL_CAST_STATIC(int64_t, otel_counting_log_exporter::record_count(false));
	status->logs.last_export_ms    = otel_counting_log_exporter::last_export_age_ms();

	status->metrics.dropped        = -1;
	status->metrics.queue_depth    = -1;
	status->metrics.queue_capacity = -1;
	status->metrics.export_ok      = OTEL_CAST_STATIC(int64_t, otel_counting_metric_exporter::export_count(true));
	status->metrics.export_fail    = OTEL_CAST_STATIC(int64_t, otel_counting_metric_exporter::export_count(false));
	status->metrics.records_ok     = -1;
	status->metrics.records_fail   = -1;
	status->metrics.last_export_ms = otel_counting_metric_exporter::last_export_age_ms();

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_tracer_processor_create - creates a span processor for a tracer
 *
 * SYNOPSIS
 *   int otel_tracer_processor_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter, std::unique_ptr<otel_sdk_trace::SpanProcessor> &processor, const char *name)
 *
 * ARGUMENTS
 *   tracer    - pointer to the tracer instance for which the processor is being created
 *   exporter  - unique pointer to the span exporter to be used by the processor
 *   processor - reference to a unique pointer where the created processor is returned
 *   name      - name of the processor configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Creates a span processor configured to use the specified span exporter.
 *   The created processor is returned via the processor parameter.
 *
 *   Ownership of the exporter is transferred to the processor.  The caller
 *   retains ownership of the processor and is responsible for its lifetime.
 *
 *   This function does not attach the processor to the tracer; that must be
 *   done separately if desired.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if the processor could
 *   not be created.
 */
int otel_tracer_processor_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter, std::unique_ptr<otel_sdk_trace::SpanProcessor> &processor, const char *name)
{
	std::unique_ptr<otel_sdk_trace::SpanProcessor> processor_maybe;
	int                                            rc;
	char                                           type[OTEL_YAML_BUFSIZ] = "", thread_name[OTEL_YAML_BUFSIZ] = "";
	int64_t                                        max_queue_size = 2048, schedule_delay = 5000, export_timeout = 30000, max_export_batch_size = 512;
	int64_t                                        cpu_id = -1;
	bool                                           flag_batch = true;

	OTELC_FUNC("%p, <exporter>, <processor>, \"%s\"", tracer, OTELC_STR_ARG(name));

	if (OTEL_NULL(tracer))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	/***
	 * <opentelemetry/sdk/trace/batch_span_processor_options.h>
	 *
	 * NOTE: The export_timeout member of the BatchSpanProcessorOptions
	 * structure is defined but not yet utilized.
	 */
	rc = yaml_get_node(otelc_fyd, &(tracer->err), 0, "OpenTelemetry traces processor", OTEL_YAML_TRACER_PREFIX OTEL_YAML_PROCESSORS, name,
	                   OTEL_YAML_ARG_STR(1, PROCESSORS, type),
	                   OTEL_YAML_ARG_STR(0, PROCESSORS, thread_name),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, cpu_id, -1, OTEL_MAX_CPU_ID),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, max_queue_size, 64, 65536),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, schedule_delay, 1, 60000),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, export_timeout, 1, 60000),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, max_export_batch_size, 1, 65536),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (rc == 0) {
		if (name != nullptr)
			OTEL_TRACER_RETURN_INT("'%s': OpenTelemetry traces processor type not specified", name);
		else
			OTEL_TRACER_RETURN_INT("OpenTelemetry traces processor type not specified");
	}
	else if (strcasecmp(type, "batch") == 0)
		flag_batch = true;
	else if (strcasecmp(type, "single") == 0)
		flag_batch = false;
	else if (*type != '\0')
		OTEL_TRACER_RETURN_INT("'%s': invalid OpenTelemetry traces processor type specified", type);

	if (flag_batch) {
		otel_sdk_trace::BatchSpanProcessorOptions        options{};
		otel_sdk_trace::BatchSpanProcessorRuntimeOptions rt_options{};

		if (max_queue_size < max_export_batch_size)
			OTEL_TRACER_RETURN_INT("Maximum buffer/queue size must be greater than or equal to maximum batch size: %" PRId64 " < %" PRId64, max_queue_size, max_export_batch_size);

		/***
		 * Configuration parameters are set here.
		 */
		options.max_queue_size        = max_queue_size;
		options.schedule_delay_millis = std::chrono::milliseconds(schedule_delay);
		options.export_timeout        = std::chrono::milliseconds(export_timeout);
		options.max_export_batch_size = max_export_batch_size;

		if (*thread_name != '\0') {
			const auto thread_instrumentation = otel::make_shared_nothrow<otel_thread_instrumentation>(thread_name, OTEL_CAST_STATIC(int, cpu_id));
			if (!OTEL_NULL(thread_instrumentation))
				rt_options.thread_instrumentation = thread_instrumentation;
		}

		auto consumed = otel::make_shared_nothrow<std::atomic<uint64_t>>(0);
		std::unique_ptr<otel_counting_span_exporter>        counting_exporter{};
		std::unique_ptr<otel_sdk_trace::BatchSpanProcessor> inner{};

		if (!OTEL_NULL(consumed))
			counting_exporter = otel::make_unique_nothrow<otel_counting_span_exporter>(std::move(exporter), consumed);
		if (!OTEL_NULL(counting_exporter))
			inner = otel::make_unique_nothrow<otel_sdk_trace::BatchSpanProcessor>(std::move(counting_exporter), options, rt_options);
		if (!OTEL_NULL(inner))
			processor_maybe = otel::make_unique_nothrow<otel_counting_span_processor>(std::move(inner), consumed, options.max_queue_size);
	} else {
		processor_maybe = otel::make_unique_nothrow<otel_sdk_trace::SimpleSpanProcessor>(std::move(exporter));
	}

	if (OTEL_NULL(processor_maybe))
		OTEL_TRACER_RETURN_INT("Unable to create OpenTelemetry traces processor");

	processor = std::move(processor_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_logger_processor_create - creates a log record processor for a logger
 *
 * SYNOPSIS
 *   int otel_logger_processor_create(struct otelc_logger *logger, std::unique_ptr<otel_sdk_logs::LogRecordExporter> &exporter, std::unique_ptr<otel_sdk_logs::LogRecordProcessor> &processor, const char *name)
 *
 * ARGUMENTS
 *   logger    - pointer to the logger instance for which the processor is being created
 *   exporter  - unique pointer to the log record exporter to be used by the processor
 *   processor - reference to a unique pointer where the created processor is returned
 *   name      - name of the processor configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Creates a log record processor configured to use the specified log record
 *   exporter.  The created processor is returned via the processor parameter.
 *
 *   Ownership of the exporter is transferred to the processor.  The caller
 *   retains ownership of the processor and is responsible for its lifetime.
 *
 *   This function does not attach the processor to the logger; that must be
 *   done separately if desired.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if the processor could
 *   not be created.
 */
int otel_logger_processor_create(struct otelc_logger *logger, std::unique_ptr<otel_sdk_logs::LogRecordExporter> &exporter, std::unique_ptr<otel_sdk_logs::LogRecordProcessor> &processor, const char *name)
{
	std::unique_ptr<otel_sdk_logs::LogRecordProcessor> processor_maybe;
	int                                                rc;
	char                                               type[OTEL_YAML_BUFSIZ] = "", thread_name[OTEL_YAML_BUFSIZ] = "";
	int64_t                                            max_queue_size = 2048, schedule_delay = 1000, export_timeout = 30000, max_export_batch_size = 512;
	int64_t                                            cpu_id = -1;
	bool                                               flag_batch = true;

	OTELC_FUNC("%p, <exporter>, <processor>, \"%s\"", logger, OTELC_STR_ARG(name));

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	/***
	 * <opentelemetry/sdk/logs/batch_log_record_processor_options.h>
	 *
	 * NOTE: The export_timeout_millis member of the
	 * BatchLogRecordProcessorOptions structure is defined but not yet
	 * utilized.
	 */
	rc = yaml_get_node(otelc_fyd, &(logger->err), 0, "OpenTelemetry logs processor", OTEL_YAML_LOGGER_PREFIX OTEL_YAML_PROCESSORS, name,
	                   OTEL_YAML_ARG_STR(1, PROCESSORS, type),
	                   OTEL_YAML_ARG_STR(0, PROCESSORS, thread_name),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, cpu_id, -1, OTEL_MAX_CPU_ID),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, max_queue_size, 64, 65536),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, schedule_delay, 1, 60000),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, export_timeout, 1, 60000),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, max_export_batch_size, 1, 65536),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (rc == 0) {
		if (name != nullptr)
			OTEL_LOGGER_RETURN_INT("'%s': OpenTelemetry logs processor type not specified", name);
		else
			OTEL_LOGGER_RETURN_INT("OpenTelemetry logs processor type not specified");
	}
	else if (strcasecmp(type, "batch") == 0)
		flag_batch = true;
	else if (strcasecmp(type, "single") == 0)
		flag_batch = false;
	else if (*type != '\0')
		OTEL_LOGGER_RETURN_INT("'%s': invalid OpenTelemetry logs processor type specified", type);

	if (flag_batch) {
		otel_sdk_logs::BatchLogRecordProcessorOptions        options{};
		otel_sdk_logs::BatchLogRecordProcessorRuntimeOptions rt_options{};

		if (max_queue_size < max_export_batch_size)
			OTEL_LOGGER_RETURN_INT("Maximum buffer/queue size must be greater than or equal to maximum batch size: %" PRId64 " < %" PRId64, max_queue_size, max_export_batch_size);

		/***
		 * Configuration parameters are set here.
		 */
		options.max_queue_size        = max_queue_size;
		options.schedule_delay_millis = std::chrono::milliseconds(schedule_delay);
		options.export_timeout_millis = std::chrono::milliseconds(export_timeout);
		options.max_export_batch_size = max_export_batch_size;

		if (*thread_name != '\0') {
			const auto thread_instrumentation = otel::make_shared_nothrow<otel_thread_instrumentation>(thread_name, OTEL_CAST_STATIC(int, cpu_id));
			if (!OTEL_NULL(thread_instrumentation))
				rt_options.thread_instrumentation = thread_instrumentation;
		}

		auto consumed = otel::make_shared_nothrow<std::atomic<uint64_t>>(0);
		std::unique_ptr<otel_counting_log_exporter>              counting_exporter{};
		std::unique_ptr<otel_sdk_logs::BatchLogRecordProcessor>  inner{};

		if (!OTEL_NULL(consumed))
			counting_exporter = otel::make_unique_nothrow<otel_counting_log_exporter>(std::move(exporter), consumed);
		if (!OTEL_NULL(counting_exporter))
			inner = otel::make_unique_nothrow<otel_sdk_logs::BatchLogRecordProcessor>(std::move(counting_exporter), options, rt_options);
		if (!OTEL_NULL(inner))
			processor_maybe = otel::make_unique_nothrow<otel_counting_log_processor>(std::move(inner), consumed, options.max_queue_size);
	}
	else {
		processor_maybe = otel::make_unique_nothrow<otel_sdk_logs::SimpleLogRecordProcessor>(std::move(exporter));
	}

	if (OTEL_NULL(processor_maybe))
		OTEL_LOGGER_RETURN_INT("Unable to create OpenTelemetry logs processor");

	processor = std::move(processor_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
