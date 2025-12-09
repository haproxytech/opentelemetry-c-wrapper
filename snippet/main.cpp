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
#include "main.h"


static otel_nostd::shared_ptr<otel_trace::Tracer>    otel_tracer{};
static std::ofstream                                 otel_logfile{};
static struct otel_handle<struct otel_span_handle *> otel_span;


void __attribute__((constructor)) otel_constructor(void)
{
	OTELC_FUNC("");

	OTELC_RETURN();
}


void __attribute__((destructor)) otel_destructor(void)
{
	OTELC_FUNC("");

	OTELC_RETURN();
}


static std::chrono::nanoseconds timespec_to_duration(const struct timespec *ts)
{
	auto duration = std::chrono::seconds{ts->tv_sec} + std::chrono::nanoseconds{ts->tv_nsec};

	return std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
}


static int otel_exporter_create(bool flag_use_ostream, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter)
{
	std::unique_ptr<otel_sdk_trace::SpanExporter> exporter_maybe{};

	OTELC_FUNC("%hhu, <exporter>", flag_use_ostream);

	if (flag_use_ostream) {
		exporter_maybe = std::unique_ptr<otel_sdk_trace::SpanExporter>(new otel_exporter::trace::OStreamSpanExporter(std::cout));
	} else {
		otel_exporter_otlp::OtlpHttpExporterOptions options{};

		options.url           = "http://localhost:4318/v1/traces";
		options.console_debug = 1;

		if (options.console_debug)
			otel_sdk_internal_log::GlobalLogHandler::SetLogLevel(otel_sdk_internal_log::LogLevel::Debug);

		exporter_maybe = std::unique_ptr<otel_sdk_trace::SpanExporter>(new otel_exporter_otlp::OtlpHttpExporter(options));
	}

	if (OTEL_NULL(exporter_maybe)) {
		OTELC_DBG("Unable to init %s exporter", flag_use_ostream ? "OStream" : "OpenTelemetry(OTLP) HTTP");

		OTELC_RETURN_INT(-1);
	}

	exporter = std::move(exporter_maybe);

	OTELC_RETURN_INT(0);
}


static int otel_sampler_create(double ratio, std::unique_ptr<otel_sdk_trace::Sampler> &sampler)
{
	OTELC_FUNC("%f, <sampler>", ratio);

	auto sampler_maybe = std::unique_ptr<otel_sdk_trace::Sampler>(new otel_sdk_trace::TraceIdRatioBasedSampler(ratio));
	if (OTEL_NULL(sampler_maybe)) {
		OTELC_DBG("Unable to create OpenTelemetry sampler");

		OTELC_RETURN_INT(-1);
	}

	sampler = std::move(sampler_maybe);

	OTELC_RETURN_INT(0);
}


static int otel_processor_create(bool flag_use_batch, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter, std::unique_ptr<otel_sdk_trace::SpanProcessor> &processor)
{
	std::unique_ptr<otel_sdk_trace::SpanProcessor> processor_maybe;

	OTELC_FUNC("%hhu, <exporter>, <processor>", flag_use_batch);

	if (flag_use_batch) {
		otel_sdk_trace::BatchSpanProcessorOptions options{};

		options.max_queue_size        = 2048;
		options.schedule_delay_millis = std::chrono::milliseconds(5000);
		options.max_export_batch_size = 512;

		processor_maybe = std::unique_ptr<otel_sdk_trace::SpanProcessor>(new otel_sdk_trace::BatchSpanProcessor(std::move(exporter), options));
	} else {
		processor_maybe = std::unique_ptr<otel_sdk_trace::SpanProcessor>(new otel_sdk_trace::SimpleSpanProcessor(std::move(exporter)));
	}

	if (OTEL_NULL(processor_maybe)) {
		OTELC_DBG("Unable to create OpenTelemetry processor");

		OTELC_RETURN_INT(-1);
	}

	processor = std::move(processor_maybe);

	OTELC_RETURN_INT(0);
}


static void otel_resource_detector(otel_sdk_resource::ResourceAttributes &attributes)
{
	otel_sdk_resource::OTELResourceDetector detector{};

	OTELC_FUNC("<attributes>");

	auto resource         = detector.Detect();
	auto attributes_maybe = resource.GetAttributes();

	if (attributes_maybe.size() > 0)
		attributes = std::move(attributes_maybe);

	OTELC_RETURN();
}


static int otel_tracer_provider_create(std::unique_ptr<otel_sdk_trace::SpanProcessor> &processor, std::unique_ptr<otel_sdk_trace::Sampler> &sampler, std::unique_ptr<otel_trace::TracerProvider> &provider)
{
	otel_sdk_resource::ResourceAttributes attributes{};

	OTELC_FUNC("<processor>, <sampler>, <provider>");

	(void)attributes.emplace(std::string{"service.name"}, std::string{"test service"});
	otel_resource_detector(attributes);

	auto resource = otel_sdk_resource::Resource::Create(attributes);

#if 1
	std::vector<std::unique_ptr<otel_sdk_trace::SpanProcessor>> processors;
	try {
		processors.push_back(std::move(processor));
	}
	catch (const std::exception &e) {
		OTELC_DBG("Unable to add processor: %s", e.what());

		OTELC_RETURN_INT(-1);
	}
	catch (...) {
		OTELC_DBG("Unable to add processor");

		OTELC_RETURN_INT(-1);
	}

	auto context_maybe = std::unique_ptr<otel_sdk_trace::TracerContext>(new otel_sdk_trace::TracerContext(std::move(processors), resource, std::move(sampler)));
	if (OTEL_NULL(context_maybe)) {
		OTELC_DBG("Unable to create OpenTelemetry context");

		OTELC_RETURN_INT(-1);
	}
	auto provider_maybe = otel_nostd::unique_ptr<otel_trace::TracerProvider>(new otel_sdk_trace::TracerProvider(std::move(context_maybe)));
#else
	auto provider_maybe = otel_nostd::unique_ptr<otel_trace::TracerProvider>(new otel_sdk_trace::TracerProvider(std::move(processor), resource, std::move(sampler)));
#endif
	if (OTEL_NULL(provider_maybe)) {
		OTELC_DBG("Unable to create OpenTelemetry provider");

		OTELC_RETURN_INT(-1);
	}

	provider = std::move(provider_maybe);

	OTELC_RETURN_INT(0);
}


static void otel_tracer_provider_destroy(void)
{
	const std::shared_ptr<otel_trace::TracerProvider> none;

	OTELC_FUNC("");

	otel_trace::Provider::SetTracerProvider(none);

	OTELC_RETURN();
}


static void otel_exporter_destroy(void)
{
	OTELC_FUNC("");

	if (otel_logfile.is_open())
		otel_logfile.close();

	OTELC_RETURN();
}


static void otel_tracer_close(void)
{
	OTELC_FUNC("");

#if defined(OPENTELEMETRY_ABI_VERSION_NO) && (OPENTELEMETRY_ABI_VERSION_NO < 2)
	if (!OTEL_NULL(otel_tracer))
		otel_tracer->CloseWithMicroseconds(5000000);
#endif

	OTELC_RETURN();
}


static int otel_span_start(const char *operation_name, const struct timespec *ts_steady, const struct timespec *ts_system, int64_t parent_id, int64_t span_id)
{
	otel_trace::StartSpanOptions span_options{};

	OTELC_FUNC("\"%s\", %p, %p, %" PRId64 ", %" PRId64, OTELC_STR_ARG(operation_name), ts_steady, ts_system, parent_id, span_id);

	if (OTEL_NULL(otel_tracer))
		OTELC_RETURN_INT(-1);

	if (!OTEL_NULL(ts_system))
		span_options.start_system_time = otel_system_timestamp(timespec_to_duration(ts_system));
	if (!OTEL_NULL(ts_steady))
		span_options.start_steady_time = otel_steady_timestamp(timespec_to_duration(ts_steady));
	span_options.kind = otel_trace::SpanKind::kServer;

	if (parent_id >= 0) {
		auto parent_maybe = OTEL_SPAN_HANDLE_SPAN(parent_id)->GetContext();
		if (parent_maybe.IsValid()) {
			span_options.parent = parent_maybe;
		} else {
			OTELC_DBG("Unable to get parent span context");

			OTELC_RETURN_INT(-1);
		}
	}

	auto span_maybe = otel_tracer->StartSpan(operation_name, span_options);
	if (OTEL_NULL(span_maybe)) {
		OTELC_DBG("Unable to start new span");
	} else {
		if (!otel_span.is_initialized) {
			otel_span.is_initialized = true;

			otel_span.handle.clear();
			otel_span.handle.reserve(8192);

			OTELC_DBG("otel_span initialized");
		}

		auto scope   = std::make_shared<otel_trace::Scope>(otel_trace::Scope(span_maybe));
		auto context = std::make_shared<otel_context::Context>(otel_context::RuntimeContext::GetCurrent());

		(void)otel_span.handle.emplace(span_id, new otel_span_handle{std::move(span_maybe), std::move(scope), std::move(context)});
	}

	OTELC_RETURN_INT(span_id);
}


static int otel_span_baggage_set(int64_t id, const char *key, const char *value)
{
	OTELC_FUNC("%" PRId64 ", \"%s\", \"%s\"", id, OTELC_STR_ARG(key), OTELC_STR_ARG(value));

	auto baggage = otel_baggage::GetBaggage(*OTEL_SPAN_HANDLE_CONTEXT(id));
	baggage = baggage->Set(key, value);

	auto context = otel_baggage::SetBaggage(*OTEL_SPAN_HANDLE_CONTEXT(id), baggage);
	OTEL_SPAN_HANDLE_CONTEXT(id) = std::make_shared<otel_context::Context>(context);

	OTELC_RETURN_INT(0);
}


static char *otel_span_baggage_get(int64_t id, const char *key)
{
	std::string  value;
	char        *retptr;

	OTELC_FUNC("%" PRId64 ", \"%s\"", id, OTELC_STR_ARG(key));

	auto baggage = otel_baggage::GetBaggage(*OTEL_SPAN_HANDLE_CONTEXT(id));
	baggage->GetValue(key, value);

	retptr = strndup(value.c_str(), value.length());

	OTELC_RETURN_EX(retptr, typeof(retptr), "%p");
}


static void otel_tracer_destroy(void)
{
	OTELC_FUNC("");

	if (!OTEL_NULL(otel_tracer)) {
		const std::shared_ptr<otel_context::propagation::TextMapPropagator> none;
		otel_context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(none);

		otel_tracer = nullptr;
	}

	otel_tracer_provider_destroy();
	otel_exporter_destroy();

	OTELC_RETURN();
}


static int otelc_tracer_start(const char *scope_name, double ratio, bool flag_use_ostream, bool flag_use_batch)
{
	std::unique_ptr<otel_sdk_trace::SpanExporter>  exporter;
	std::unique_ptr<otel_sdk_trace::Sampler>       sampler;
	std::unique_ptr<otel_sdk_trace::SpanProcessor> processor;
	std::unique_ptr<otel_trace::TracerProvider>    provider;
	int                                            retval = -1;

	OTELC_FUNC("\"%s\", %f, %hhu, %hhu", OTELC_STR_ARG(scope_name), ratio, flag_use_ostream, flag_use_batch);

	if ((retval = otel_exporter_create(flag_use_ostream, exporter)) == -1)
		;
	else if ((retval = otel_sampler_create(ratio, sampler)) == -1)
		;
	else if ((retval = otel_processor_create(flag_use_batch, exporter, processor)) == -1)
		;
	else if ((retval = otel_tracer_provider_create(processor, sampler, provider)) != -1) {
		otel_tracer = provider->GetTracer(scope_name, OTELC_SCOPE_VERSION, OTELC_SCOPE_SCHEMA_URL);
		otel_trace::Provider::SetTracerProvider(std::move(provider));

		otel_context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(otel_nostd::shared_ptr<otel_context::propagation::TextMapPropagator>(new otel_trace::propagation::HttpTraceContext()));
	}

	OTELC_RETURN_INT(retval);
}


static void otel_nolock_span_destroy(int64_t id)
{
	OTELC_FUNC("%" PRId64, id);

	if (OTEL_HANDLE_IS_VALID(otel_span, id)) {
		delete OTEL_SPAN_HANDLE(id);

		otel_span.handle.erase(id);
		otel_span.erase_cnt++;

		OTELC_DBG("otel_span[%" PRId64 "] erased", id);
	} else {
		OTELC_DBG("invalid otel_span[%" PRId64 "]", id);
	}

	otel_span.destroy_cnt++;

	OTELC_RETURN();
}


static void otel_span_end(int64_t id)
{
	otel_trace::EndSpanOptions end_options{};

	OTELC_FUNC("%" PRId64, id);

	OTEL_SPAN_HANDLE_SPAN(id)->End(end_options);

	otel_nolock_span_destroy(id);

	OTELC_RETURN();
}


int main(int argc, char **argv)
{
	const char *scope_name       = (argc > 1) ? argv[1] : "test";
	const char *operation_name_1 = (argc > 2) ? argv[2] : "root span";
	const char *operation_name_2 = (argc > 3) ? argv[3] : "child span";
	const char *baggage_key      = "baggage_key";
	char       *baggage_value;

	OTELC_FUNC("%d, %p:%p", argc, argv, OTEL_NULL(argv) ? nullptr : *argv);

	if (otelc_tracer_start(scope_name, 1.0, 0, 1) == -1)
		OTELC_RETURN_INT(1);
	else if (otel_span_start(operation_name_1, nullptr, nullptr, -1, 0) == -1)
		OTELC_RETURN_INT(2);
	else if (otel_span_baggage_set(0, baggage_key, "baggage_value") == -1)
		OTELC_RETURN_INT(3);
	else if (otel_span_start(operation_name_2, nullptr, nullptr, 0, 1) == -1)
		OTELC_RETURN_INT(4);
	else {
		if ((baggage_value = otel_span_baggage_get(0, baggage_key)) != nullptr)
			OTELC_DBG("span[%d] baggage: '%s' -> '%s'", 0, baggage_key, baggage_value);
		if ((baggage_value = otel_span_baggage_get(1, baggage_key)) != nullptr)
			OTELC_DBG("span[%d] baggage: '%s' -> '%s'", 1, baggage_key, baggage_value);
	}

	otel_span_end(1);
	otel_span_end(0);
	otel_tracer_close();
	otel_tracer_destroy();

	OTELC_RETURN_INT(0);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
