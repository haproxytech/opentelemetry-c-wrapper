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
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, max_queue_size, 64, 65536),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, schedule_delay, 1, 60000),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, export_timeout, 1, 60000),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, max_export_batch_size, 1, 65536),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (rc == 0) {
		if (name != nullptr)
			OTEL_TRACER_ERETURN_INT("'%s': OpenTelemetry traces processor type not specified", name);
		else
			OTEL_TRACER_ERETURN_INT("OpenTelemetry traces processor type not specified");
	}
	else if (strcasecmp(type, "batch") == 0)
		flag_batch = true;
	else if (strcasecmp(type, "single") == 0)
		flag_batch = false;
	else if (*type != '\0')
		OTEL_TRACER_ERETURN_INT("'%s': invalid OpenTelemetry traces processor type specified", type);

	if (flag_batch) {
		otel_sdk_trace::BatchSpanProcessorOptions        options{};
		otel_sdk_trace::BatchSpanProcessorRuntimeOptions rt_options{};

		if (max_queue_size < max_export_batch_size)
			OTEL_TRACER_ERETURN_INT("Maximum buffer/queue size must be greater than or equal to maximum batch size: %" PRId64 " < %" PRId64, max_queue_size, max_export_batch_size);

		/***
		 * Configuration parameters are set here.
		 */
		options.max_queue_size        = max_queue_size;
		options.schedule_delay_millis = std::chrono::milliseconds(schedule_delay);
		options.export_timeout        = std::chrono::milliseconds(export_timeout);
		options.max_export_batch_size = max_export_batch_size;

		if (*thread_name != '\0') {
			const auto thread_instrumentation = otel::make_shared_nothrow<otel_thread_instrumentation>(thread_name);
			if (!OTEL_NULL(thread_instrumentation))
				rt_options.thread_instrumentation = thread_instrumentation;
		}

		processor_maybe = otel::make_unique_nothrow<otel_sdk_trace::BatchSpanProcessor>(std::move(exporter), options, rt_options);
	} else {
		processor_maybe = otel::make_unique_nothrow<otel_sdk_trace::SimpleSpanProcessor>(std::move(exporter));
	}

	if (OTEL_NULL(processor_maybe))
		OTEL_TRACER_ERETURN_INT("Unable to create OpenTelemetry traces processor");

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
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, max_queue_size, 64, 65536),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, schedule_delay, 1, 60000),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, export_timeout, 1, 60000),
	                   OTEL_YAML_ARG_INT64(0, PROCESSORS, max_export_batch_size, 1, 65536),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (rc == 0) {
		if (name)
			OTEL_LOGGER_ERETURN_INT("'%s': OpenTelemetry logs processor type not specified", name);
		else
			OTEL_LOGGER_ERETURN_INT("OpenTelemetry logs processor type not specified");
	}
	else if (strcasecmp(type, "batch") == 0)
		flag_batch = 1;
	else if (strcasecmp(type, "single") == 0)
		flag_batch = 0;
	else if (*type != '\0')
		OTEL_LOGGER_ERETURN_INT("'%s': invalid OpenTelemetry logs processor type specified", type);

	if (flag_batch) {
		otel_sdk_logs::BatchLogRecordProcessorOptions        options{};
		otel_sdk_logs::BatchLogRecordProcessorRuntimeOptions rt_options{};

		if (max_queue_size < max_export_batch_size)
			OTEL_LOGGER_ERETURN_INT("Maximum buffer/queue size must be greater than or equal to maximum batch size: %" PRId64 " < %" PRId64, max_queue_size, max_export_batch_size);

		/***
		 * Configuration parameters are set here.
		 */
		options.max_queue_size        = max_queue_size;
		options.schedule_delay_millis = std::chrono::milliseconds(schedule_delay);
		options.export_timeout_millis = std::chrono::milliseconds(export_timeout);
		options.max_export_batch_size = max_export_batch_size;

		if (*thread_name != '\0') {
			const auto thread_instrumentation = otel::make_shared_nothrow<otel_thread_instrumentation>(thread_name);
			if (!OTEL_NULL(thread_instrumentation))
				rt_options.thread_instrumentation = thread_instrumentation;
		}

		processor_maybe = otel::make_unique_nothrow<otel_sdk_logs::BatchLogRecordProcessor>(std::move(exporter), options, rt_options);
	}
	else {
		processor_maybe = otel::make_unique_nothrow<otel_sdk_logs::SimpleLogRecordProcessor>(std::move(exporter));
	}

	if (OTEL_NULL(processor_maybe))
		OTEL_LOGGER_ERETURN_INT("Unable to create OpenTelemetry logs processor");

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
