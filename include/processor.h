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
#ifndef _OPENTELEMETRY_C_WRAPPER_PROCESSOR_H_
#define _OPENTELEMETRY_C_WRAPPER_PROCESSOR_H_

/***
 * Exporter wrapper that tracks how many span records the batch processor has
 * exported, enabling the delegating processor to compute queue depth.
 */
class otel_counting_span_exporter : public otel_sdk_trace::SpanExporter
{
public:
	otel_counting_span_exporter(std::unique_ptr<otel_sdk_trace::SpanExporter> &&exporter, std::shared_ptr<std::atomic<uint64_t>> consumed);

	std::unique_ptr<otel_sdk_trace::Recordable> MakeRecordable() noexcept override;
	otel_sdk_common::ExportResult Export(const otel_nostd::span<std::unique_ptr<otel_sdk_trace::Recordable>> &spans) noexcept override;
	bool ForceFlush(std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;
	bool Shutdown(std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

private:
	std::unique_ptr<otel_sdk_trace::SpanExporter> inner_;
	std::shared_ptr<std::atomic<uint64_t>>        consumed_;
};


/***
 * Delegating span processor that counts dropped spans.
 *
 * Wraps a real BatchSpanProcessor and delegates all calls to it.  Maintains a
 * pair of monotonic counters (produced/consumed) shared with a counting exporter
 * wrapper to approximate queue depth and detect drops without accessing the batch
 * processor's internal buffer.  A guard on the unsigned subtraction prevents
 * wraparound when consumed momentarily races past produced.
 */
class otel_counting_span_processor : public otel_sdk_trace::SpanProcessor
{
public:
	otel_counting_span_processor(std::unique_ptr<otel_sdk_trace::BatchSpanProcessor> &&inner, std::shared_ptr<std::atomic<uint64_t>> consumed, size_t max_queue_size);

	std::unique_ptr<otel_sdk_trace::Recordable> MakeRecordable() noexcept override;
	void OnStart(otel_sdk_trace::Recordable &span, const otel_trace::SpanContext &parent_context) noexcept override;
	void OnEnd(std::unique_ptr<otel_sdk_trace::Recordable> &&span) noexcept override;
	bool ForceFlush(std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;
	bool Shutdown(std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

	static uint64_t dropped_count() noexcept
	{
		return dropped_count_.load(std::memory_order_relaxed);
	}
	static void reset_dropped_count() noexcept
	{
		dropped_count_.store(0, std::memory_order_relaxed);
	}

private:
	std::shared_ptr<std::atomic<uint64_t>>              consumed_;
	std::atomic<uint64_t>                               produced_{0};
	size_t                                              max_queue_size_;
	std::unique_ptr<otel_sdk_trace::BatchSpanProcessor> inner_;
	static std::atomic<uint64_t>                        dropped_count_;
};


/***
 * Exporter wrapper that tracks how many log records the batch processor has
 * exported, enabling the delegating processor to compute queue depth.
 */
class otel_counting_log_exporter : public otel_sdk_logs::LogRecordExporter
{
public:
	otel_counting_log_exporter(std::unique_ptr<otel_sdk_logs::LogRecordExporter> &&exporter, std::shared_ptr<std::atomic<uint64_t>> consumed);

	std::unique_ptr<otel_sdk_logs::Recordable> MakeRecordable() noexcept override;
	otel_sdk_common::ExportResult Export(const otel_nostd::span<std::unique_ptr<otel_sdk_logs::Recordable>> &records) noexcept override;
	bool ForceFlush(std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;
	bool Shutdown(std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

private:
	std::unique_ptr<otel_sdk_logs::LogRecordExporter> inner_;
	std::shared_ptr<std::atomic<uint64_t>>            consumed_;
};


/***
 * Delegating log record processor that counts dropped log records.
 *
 * Wraps a real BatchLogRecordProcessor and delegates all calls to it.  Uses the
 * same guarded produced/consumed counter technique as the span variant to detect
 * drops without accessing internal buffer state.
 */
class otel_counting_log_processor : public otel_sdk_logs::LogRecordProcessor
{
public:
	otel_counting_log_processor(std::unique_ptr<otel_sdk_logs::BatchLogRecordProcessor> &&inner, std::shared_ptr<std::atomic<uint64_t>> consumed, size_t max_queue_size);

	std::unique_ptr<otel_sdk_logs::Recordable> MakeRecordable() noexcept override;
	void OnEmit(std::unique_ptr<otel_sdk_logs::Recordable> &&record) noexcept override;
	bool ForceFlush(std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;
	bool Shutdown(std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

	static uint64_t dropped_count() noexcept
	{
		return dropped_count_.load(std::memory_order_relaxed);
	}
	static void reset_dropped_count() noexcept
	{
		dropped_count_.store(0, std::memory_order_relaxed);
	}

private:
	std::shared_ptr<std::atomic<uint64_t>>                  consumed_;
	std::atomic<uint64_t>                                   produced_{0};
	size_t                                                  max_queue_size_;
	std::unique_ptr<otel_sdk_logs::BatchLogRecordProcessor> inner_;
	static std::atomic<uint64_t>                            dropped_count_;
};


int otel_tracer_processor_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter, std::unique_ptr<otel_sdk_trace::SpanProcessor> &processor, const char *name = nullptr);
int otel_logger_processor_create(struct otelc_logger *logger, std::unique_ptr<otel_sdk_logs::LogRecordExporter> &exporter, std::unique_ptr<otel_sdk_logs::LogRecordProcessor> &processor, const char *name = nullptr);

#endif /* _OPENTELEMETRY_C_WRAPPER_PROCESSOR_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
