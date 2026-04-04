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


/***
 * NAME
 *   otel_tracer_provider_create - creates a new tracer provider
 *
 * SYNOPSIS
 *   int otel_tracer_provider_create(struct otelc_tracer *tracer, std::vector<std::unique_ptr<otel_sdk_trace::SpanProcessor>> &processors, std::unique_ptr<otel_sdk_trace::Sampler> &sampler, std::unique_ptr<otel_trace::TracerProvider> &provider)
 *
 * ARGUMENTS
 *   tracer     - tracer instance
 *   processors - the span processors to be used by the provider
 *   sampler    - the sampler to be used by the provider
 *   provider   - unique pointer to store the created tracer provider
 *
 * DESCRIPTION
 *   Creates a new tracer provider with the specified span processors and sampler.
 *   The provider is responsible for creating and managing tracer instances, and
 *   it is configured with resource attributes detected from the environment and
 *   the YAML configuration file.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_tracer_provider_create(struct otelc_tracer *tracer, std::vector<std::unique_ptr<otel_sdk_trace::SpanProcessor>> &processors, std::unique_ptr<otel_sdk_trace::Sampler> &sampler, std::unique_ptr<otel_trace::TracerProvider> &provider)
{
	otel_sdk_resource::Resource resource{};

	OTELC_FUNC("%p, <processors>, <sampler>, <provider>", tracer);

	if (OTEL_NULL(tracer))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (otel_resource_create("OpenTelemetry tracer provider", OTEL_YAML_TRACER_PREFIX OTEL_YAML_PROVIDERS, resource, &(tracer->err)) == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

#ifdef OTELC_USE_MULTIPLE_PROCESSORS
	auto context_maybe = otel::make_unique_nothrow<otel_sdk_trace::TracerContext>(std::move(processors), std::move(resource), std::move(sampler));
	if (OTEL_NULL(context_maybe))
		OTEL_TRACER_RETURN_INT("Unable to create OpenTelemetry tracer context");
	auto provider_maybe = otel_nostd::unique_ptr<otel_trace::TracerProvider>(otel::make_unique_nothrow<otel_sdk_trace::TracerProvider>(std::move(context_maybe)).release());
#else
	auto provider_maybe = otel_nostd::unique_ptr<otel_trace::TracerProvider>(otel::make_unique_nothrow<otel_sdk_trace::TracerProvider>(std::move(processors[0]), std::move(resource), std::move(sampler)).release());
#endif /* OTELC_USE_MULTIPLE_PROCESSORS */

	if (OTEL_NULL(provider_maybe))
		OTEL_TRACER_RETURN_INT("Unable to create OpenTelemetry tracer provider");

	provider = std::move(provider_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_tracer_provider_get - gets the global tracer provider
 *
 * SYNOPSIS
 *   int otel_tracer_provider_get(struct otelc_tracer *tracer, otel_nostd::shared_ptr<otel_trace::TracerProvider> &provider)
 *
 * ARGUMENTS
 *   tracer   - tracer instance
 *   provider - shared pointer to store the global tracer provider
 *
 * DESCRIPTION
 *   Retrieves the currently configured global tracer provider.  This allows
 *   different parts of an application to access the same tracer provider
 *   instance without having to pass it around explicitly.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_tracer_provider_get(struct otelc_tracer *tracer, otel_nostd::shared_ptr<otel_trace::TracerProvider> &provider)
{
	OTELC_FUNC("%p, <provider>", tracer);

	if (OTEL_NULL(tracer))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	auto provider_maybe = otel_trace::Provider::GetTracerProvider();
	if (OTEL_NULL(provider_maybe))
		OTEL_TRACER_RETURN_INT("Unable to get OpenTelemetry tracer provider");

	provider = std::move(provider_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_tracer_provider_destroy - destroys the global tracer provider
 *
 * SYNOPSIS
 *   void otel_tracer_provider_destroy(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Resets the global tracer provider, effectively disabling tracing.  This
 *   should be called during application shutdown to ensure that all tracing
 *   resources are properly released.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_tracer_provider_destroy(void)
{
	const std::shared_ptr<otel_trace::TracerProvider> none;

	OTELC_FUNC("");

	const auto provider_sdk = OTEL_TRACER_PROVIDER();
	if (!OTEL_NULL(provider_sdk))
		(void)provider_sdk->ForceFlush(std::chrono::microseconds{5000000});

	otel_trace::Provider::SetTracerProvider(none);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_meter_reader_create - creates a new periodic exporting metric reader
 *
 * SYNOPSIS
 *   static int otel_meter_reader_create(struct otelc_meter *meter, std::unique_ptr<otel_sdk_metrics::PushMetricExporter> &exporter, std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader> &reader, const char *name)
 *
 * ARGUMENTS
 *   meter    - meter instance
 *   exporter - the metric exporter to be used by the reader
 *   reader   - unique pointer to store the created metric reader
 *   name     - name of the reader configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Creates a new periodic exporting metric reader with the specified exporter.
 *   The reader is responsible for collecting and exporting metrics at regular
 *   intervals, which are configured via the YAML file.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_meter_reader_create(struct otelc_meter *meter, std::unique_ptr<otel_sdk_metrics::PushMetricExporter> &exporter, std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader> &reader, const char *name)
{
	/***
	 * The default values of export_interval and export_timeout are defined
	 * in the include file
	 * <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
	 */
	otel_sdk_metrics::PeriodicExportingMetricReaderOptions        options{};
	otel_sdk_metrics::PeriodicExportingMetricReaderRuntimeOptions rt_options{};
	char                                                          thread_name[OTEL_YAML_BUFSIZ] = "";
	int64_t                                                       export_interval = 60000, export_timeout = 30000;
	int                                                           rc;

	OTELC_FUNC("%p, <exporter>, <reader>", meter);

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	rc = yaml_get_node(otelc_fyd, &(meter->err), 0, "OpenTelemetry meter reader", OTEL_YAML_METER_PREFIX OTEL_YAML_READERS, name,
	                   OTEL_YAML_ARG_STR(0, READERS, thread_name),
	                   OTEL_YAML_ARG_INT64(0, READERS, export_interval, 100, 3600000),
	                   OTEL_YAML_ARG_INT64(0, READERS, export_timeout, 100, 3600000),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (export_interval < export_timeout)
		OTEL_METER_RETURN_INT("Periodic Exporting Metric Reader: invalid configuration: export_timeout should not exceed export_interval");

	options.export_interval_millis = std::chrono::milliseconds(export_interval);
	options.export_timeout_millis  = std::chrono::milliseconds(export_timeout);

	if (*thread_name != '\0') {
		const auto thread_instrumentation = otel::make_shared_nothrow<otel_thread_instrumentation>(thread_name);
		if (!OTEL_NULL(thread_instrumentation))
			rt_options.periodic_thread_instrumentation = thread_instrumentation;
	}

	auto reader_maybe = otel::make_unique_nothrow<otel_sdk_metrics::PeriodicExportingMetricReader>(std::move(exporter), options, rt_options);
	if (OTEL_NULL(reader_maybe))
		OTEL_METER_RETURN_INT("Unable to create Periodic Exporting Metric Reader");

	reader = std::move(reader_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_meter_provider_create - creates a new meter provider
 *
 * SYNOPSIS
 *   int otel_meter_provider_create(struct otelc_meter *meter, std::vector<std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader>> &readers, std::shared_ptr<otel_metrics::MeterProvider> &provider)
 *
 * ARGUMENTS
 *   meter    - meter instance
 *   readers  - vector of metric readers to attach to the provider
 *   provider - shared pointer to store the created meter provider
 *
 * DESCRIPTION
 *   Creates a new meter provider and attaches one or more metric readers.
 *   The provider is responsible for creating and managing meter instances, and
 *   it is configured with resource attributes and views from the YAML file.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_meter_provider_create(struct otelc_meter *meter, std::vector<std::unique_ptr<otel_sdk_metrics::PeriodicExportingMetricReader>> &readers, std::shared_ptr<otel_metrics::MeterProvider> &provider)
{
	otel_sdk_resource::Resource resource{};

	OTELC_FUNC("%p, <readers:%zu>, <provider>", meter, readers.size());

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (otel_resource_create("OpenTelemetry meter provider", OTEL_YAML_METER_PREFIX OTEL_YAML_PROVIDERS, resource, &(meter->err)) == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	auto views_maybe = otel::make_unique_nothrow<otel_sdk_metrics::ViewRegistry>();
	if (OTEL_NULL(views_maybe))
		OTEL_METER_RETURN_INT("Unable to create OpenTelemetry meter views");
	auto provider_maybe = otel::make_shared_nothrow<otel_sdk_metrics::MeterProvider>(std::move(views_maybe), std::move(resource));
	if (OTEL_NULL(provider_maybe))
		OTEL_METER_RETURN_INT("Unable to create OpenTelemetry meter provider");

	const auto provider_sdk = OTEL_CAST_STATIC_PTR(otel_sdk_metrics::MeterProvider, provider_maybe);

	for (auto &reader : readers)
		provider_sdk->AddMetricReader(std::move(reader));

	provider = std::move(provider_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_meter_provider_destroy - destroys the global meter provider
 *
 * SYNOPSIS
 *   void otel_meter_provider_destroy(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Shuts down the global meter provider, flushing any pending metrics and
 *   releasing all associated resources.  This should be called during
 *   application shutdown to ensure that all metric data is exported.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_meter_provider_destroy(void)
{
	const std::shared_ptr<otel_metrics::MeterProvider> none;

	OTELC_FUNC("");

	/***
	 * Shutdown() is implicitly called so it doesn't need to be used here.
	 */
	const auto provider_sdk = OTEL_METER_PROVIDER();
	if (!OTEL_NULL(provider_sdk))
		(void)provider_sdk->ForceFlush(std::chrono::microseconds{5000000});

	otel_metrics::Provider::SetMeterProvider(none);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_logger_provider_create - creates a new logger provider
 *
 * SYNOPSIS
 *   int otel_logger_provider_create(struct otelc_logger *logger, std::vector<std::unique_ptr<otel_sdk_logs::LogRecordProcessor>> &processors, std::shared_ptr<otel_logs::LoggerProvider> &provider)
 *
 * ARGUMENTS
 *   logger     - logger instance
 *   processors - the log record processors to be used by the provider
 *   provider   - shared pointer to store the created logger provider
 *
 * DESCRIPTION
 *   Creates a new logger provider with the specified log record processors.
 *   The provider is responsible for creating and managing logger instances,
 *   and it is configured with resource attributes from the YAML file.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_logger_provider_create(struct otelc_logger *logger, std::vector<std::unique_ptr<otel_sdk_logs::LogRecordProcessor>> &processors, std::shared_ptr<otel_logs::LoggerProvider> &provider)
{
	otel_sdk_resource::Resource resource{};

	OTELC_FUNC("%p, <processors>, <provider>", logger);

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (otel_resource_create("OpenTelemetry logger provider", OTEL_YAML_LOGGER_PREFIX OTEL_YAML_PROVIDERS, resource, &(logger->err)) == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (processors.empty())
		OTEL_LOGGER_RETURN_INT("No OpenTelemetry logs processors configured");

#ifdef OTELC_USE_MULTIPLE_PROCESSORS
#  if 0
	auto context_maybe = otel::make_unique_nothrow<otel_sdk_logs::LoggerContext>(std::move(processors), std::move(resource));
	if (OTEL_NULL(context_maybe))
		OTEL_LOGGER_RETURN_INT("Unable to create OpenTelemetry logger context");
	auto provider_maybe = otel::make_shared_nothrow<otel_sdk_logs::LoggerProvider>(std::move(context_maybe));
#  else
	auto provider_maybe = otel::make_shared_nothrow<otel_sdk_logs::LoggerProvider>(std::move(processors), std::move(resource));
#  endif
#else
	auto provider_maybe = otel::make_shared_nothrow<otel_sdk_logs::LoggerProvider>(std::move(processors[0]), std::move(resource));
#endif /* OTELC_USE_MULTIPLE_PROCESSORS */

	if (OTEL_NULL(provider_maybe))
		OTEL_LOGGER_RETURN_INT("Unable to create OpenTelemetry logger provider");

	provider = std::move(provider_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_logger_provider_destroy - destroys the global logger provider
 *
 * SYNOPSIS
 *   void otel_logger_provider_destroy(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Resets the global logger provider, effectively disabling logging.  This
 *   should be called during application shutdown to ensure that all logging
 *   resources are properly released.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_logger_provider_destroy(void)
{
	const std::shared_ptr<otel_logs::LoggerProvider> none;

	OTELC_FUNC("");

	const auto provider_sdk = OTEL_LOGGER_PROVIDER();
	if (!OTEL_NULL(provider_sdk))
		(void)provider_sdk->ForceFlush(std::chrono::microseconds{5000000});

	otel_logs::Provider::SetLoggerProvider(none);

	OTELC_RETURN();
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
