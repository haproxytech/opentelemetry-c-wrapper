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


static otel_nostd::shared_ptr<otel_logs::Logger> otel_logger_owner{};
static std::atomic<otel_logs::Logger *>          otel_logger{nullptr};


/***
 * Helper that exposes the protected SetMinimumSeverity method on the
 * OpenTelemetry C++ Logger base class.
 */
struct otel_logs_logger : public otel_logs::Logger {
	using otel_logs::Logger::SetMinimumSeverity;
};


/***
 * NAME
 *   otel_logger_severity - maps a C wrapper severity to the C++ SDK severity
 *
 * SYNOPSIS
 *   static otel_logs::Severity otel_logger_severity(struct otelc_logger *logger, otelc_log_severity_t severity)
 *
 * ARGUMENTS
 *   logger   - logger instance used for error reporting
 *   severity - C wrapper log severity level to convert
 *
 * DESCRIPTION
 *   Converts an otelc_log_severity_t value to the corresponding
 *   otel_logs::Severity enum used by the OpenTelemetry C++ SDK.  On an
 *   unrecognised severity value, an error is recorded on the logger instance
 *   and otel_logs::Severity::kInvalid is returned.
 *
 * RETURN VALUE
 *   Returns the matching otel_logs::Severity value,
 *   or otel_logs::Severity::kInvalid if the severity is not recognised.
 */
static otel_logs::Severity otel_logger_severity(struct otelc_logger *logger, otelc_log_severity_t severity)
{
#define OTELC_LOG_SEVERITY_DEF(a,b)   { OTELC_LOG_SEVERITY_##a, otel_logs::Severity::b },
	static constexpr struct {
		otelc_log_severity_t severity;
		otel_logs::Severity  level;
	} severity_levels[] = { OTELC_LOG_SEVERITY_DEFINES };
#undef OTELC_LOG_SEVERITY_DEF

	/* Severity level check. */
	for (size_t i = 0; i < OTELC_TABLESIZE(severity_levels); i++)
		if (severity_levels[i].severity == severity)
			return severity_levels[i].level;

	/***
	 * The logger parameter is assumed to be valid; no checks are performed
	 * because this function is for internal use only.
	 */
	OTEL_LOGGER_ERROR("Invalid log severity level: %d", severity);

	return otel_logs::Severity::kInvalid;
}


/***
 * NAME
 *   otel_logger_enabled - checks whether the logger is enabled for a severity level
 *
 * SYNOPSIS
 *   static int otel_logger_enabled(struct otelc_logger *logger, otelc_log_severity_t severity)
 *
 * ARGUMENTS
 *   logger   - logger instance
 *   severity - log severity level to check
 *
 * DESCRIPTION
 *   Queries the underlying logger to determine whether it is enabled for the
 *   given severity level.  Callers can use this check to skip expensive log
 *   message construction when the logger would discard the record anyway.
 *
 * RETURN VALUE
 *   Returns true if the logger is enabled for the given severity, false if it
 *   is not, or OTELC_RET_ERROR in case of an error.
 */
static int otel_logger_enabled(struct otelc_logger *logger, otelc_log_severity_t severity)
{
	OTELC_FUNC("%p, %hhu", logger, severity);

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	auto *logger_ptr = otel_logger.load();
	if (OTEL_NULL(logger_ptr))
		OTEL_LOGGER_RETURN_INT("Invalid logger");

	const auto log_severity = otel_logger_severity(logger, severity);
	if (log_severity == otel_logs::Severity::kInvalid)
		OTEL_LOGGER_RETURN_INT("Invalid log severity level: %d", severity);

	OTELC_RETURN_INT(logger_ptr->Enabled(log_severity) ? true : false);
}


/***
 * NAME
 *   otel_logger_set_min_severity - sets the minimum log severity level at runtime
 *
 * SYNOPSIS
 *   static int otel_logger_set_min_severity(struct otelc_logger *logger, otelc_log_severity_t severity)
 *
 * ARGUMENTS
 *   logger   - logger instance
 *   severity - new minimum log severity level
 *
 * DESCRIPTION
 *   Changes the minimum severity threshold of the underlying logger at
 *   runtime.  After this call, only log records whose severity is equal to
 *   or greater than the given level are emitted; lower severity records are
 *   silently discarded.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_logger_set_min_severity(struct otelc_logger *logger, otelc_log_severity_t severity)
{
	OTELC_FUNC("%p, %hhu", logger, severity);

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	auto *logger_ptr = otel_logger.load();
	if (OTEL_NULL(logger_ptr))
		OTEL_LOGGER_RETURN_INT("Invalid logger");

	const auto log_severity = otel_logger_severity(logger, severity);
	if (log_severity == otel_logs::Severity::kInvalid)
		OTEL_LOGGER_RETURN_INT("Invalid log severity level: %d", severity);

	OTEL_CAST_STATIC(otel_logs_logger *, logger_ptr)->SetMinimumSeverity(OTEL_CAST_STATIC(uint8_t, log_severity));

	logger->min_severity = severity;

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_logger_record_create - creates and populates a log record
 *
 * SYNOPSIS
 *   static int otel_logger_record_create(struct otelc_logger *logger, otel_logs::Logger *logger_ptr, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, otel_nostd::unique_ptr<otel_logs::LogRecord> &log_record)
 *
 * ARGUMENTS
 *   logger        - logger instance
 *   logger_ptr    - pointer to the underlying OTel logger
 *   severity      - log severity level
 *   event_id      - numeric event identifier, or 0 to omit
 *   event_name    - event name string, or NULL when event_id is 0
 *   span_id       - a pointer to the span identifier associated with this log entry
 *   span_id_size  - the size of the span identifier buffer
 *   trace_id      - a pointer to the trace identifier associated with this log entry
 *   trace_id_size - the size of the trace identifier buffer
 *   trace_flags   - trace flags associated with the trace
 *   ts            - the timestamp of the log event, or NULL for SDK defaults
 *   attr          - a pointer to an array of key-value attributes to attach to the log record
 *   attr_len      - the number of elements in the 'attr' array
 *   log_record    - output reference for the created log record
 *
 * DESCRIPTION
 *   Validates the logger pointer and severity, checks whether the severity
 *   level is enabled, and creates a new log record populated with trace
 *   context, timestamp, event data, and attributes.  The caller is responsible
 *   for setting the body and emitting the record.  When ts is non-NULL, both
 *   the event timestamp and observed timestamp are set to the given value; when
 *   NULL, both are left to the SDK defaults.  When event_id is greater than
 *   zero, the log record is tagged with the given event identifier and name.
 *
 * RETURN VALUE
 *   Returns 1 when a log record was created, 0 if the severity level is not
 *   enabled, or OTELC_RET_ERROR on failure.
 */
static int otel_logger_record_create(struct otelc_logger *logger, otel_logs::Logger *logger_ptr, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, otel_nostd::unique_ptr<otel_logs::LogRecord> &log_record)
{
	uint8_t span_id_buf[otel_trace::SpanId::kSize] = { 0 }, trace_id_buf[otel_trace::TraceId::kSize] = { 0 };

	OTELC_FUNC("%p, %p, %hhu, %" PRId64 ", \"%s\", %p, %zu, %p, %zu, 0x%02hhx, %p, %p, %zu", logger, logger_ptr, severity, event_id, OTELC_STR_ARG(event_name), span_id, span_id_size, trace_id, trace_id_size, trace_flags, ts, attr, attr_len);

	if (OTEL_NULL(logger_ptr))
		OTEL_LOGGER_RETURN_INT("Invalid logger");

	const auto log_severity = otel_logger_severity(logger, severity);
	if (log_severity == otel_logs::Severity::kInvalid)
		OTEL_LOGGER_RETURN_INT("Invalid log severity level: %d", severity);

	if (!logger_ptr->Enabled(log_severity))
		OTELC_RETURN_INT(0);

	if (!OTEL_NULL(span_id) && (span_id_size >= sizeof(span_id_buf)))
		(void)memcpy(span_id_buf, span_id, sizeof(span_id_buf));
	if (!OTEL_NULL(trace_id) && (trace_id_size >= sizeof(trace_id_buf)))
		(void)memcpy(trace_id_buf, trace_id, sizeof(trace_id_buf));

	/* Create the log record and populate its common fields. */
	log_record = logger_ptr->CreateLogRecord();
	if (OTEL_NULL(log_record))
		OTEL_LOGGER_RETURN_INT("Unable to create log record");

	log_record->SetSeverity(log_severity);
	log_record->SetSpanId(otel_trace::SpanId{span_id_buf});
	log_record->SetTraceId(otel_trace::TraceId{trace_id_buf});
	log_record->SetTraceFlags(otel_trace::TraceFlags{trace_flags});

	if (!OTEL_NULL(ts)) {
		const auto timestamp = otel_system_timestamp(timespec_to_duration(ts));

		log_record->SetTimestamp(timestamp);
		log_record->SetObservedTimestamp(timestamp);
	}

	if (event_id > 0)
		log_record->SetEventId(event_id, OTEL_NULL(event_name) ? "" : event_name);

	if (!OTEL_NULL(attr) && (attr_len > 0))
		for (size_t i = 0; i < attr_len; i++)
			OTEL_VALUE_ADD(_INT, (*log_record), SetAttribute, attr[i].key, &(attr[i].value), &(logger->err), "Unable to add attribute");

	OTELC_RETURN_INT(1);
}


/***
 * NAME
 *   otel_logger_log_v - logs a formatted message with explicit trace and span identifiers
 *
 * SYNOPSIS
 *   static int otel_logger_log_v(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, va_list ap)
 *
 * ARGUMENTS
 *   logger        - logger instance
 *   severity      - log severity level
 *   event_id      - numeric event identifier, or 0 to omit
 *   event_name    - event name string, or NULL when event_id is 0
 *   span_id       - a pointer to the span identifier associated with this log entry
 *   span_id_size  - the size of the span identifier buffer
 *   trace_id      - a pointer to the trace identifier associated with this log entry
 *   trace_id_size - the size of the trace identifier buffer
 *   trace_flags   - trace flags associated with the trace
 *   ts            - the timestamp of the log event, or NULL for SDK defaults
 *   attr          - a pointer to an array of key-value attributes to attach to the log record
 *   attr_len      - the number of elements in the 'attr' array
 *   format        - a printf-style format string for the log message
 *   ap            - a va_list containing the arguments for the format string
 *
 * DESCRIPTION
 *   Creates an explicit log record and populates it with the specified
 *   severity, trace context, body, and attributes.  When ts is non-NULL,
 *   both the event timestamp and observed timestamp are set to the given
 *   value; when NULL, both are left to the SDK defaults.  When event_id
 *   is greater than zero, the log record is tagged with the given event
 *   identifier and name.
 *
 * RETURN VALUE
 *   Returns the number of characters written to the buffer on success,
 *   or a negative value on error (OTELC_RET_ERROR).
 */
static int otel_logger_log_v(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, va_list ap)
{
	char *ptr = nullptr;

	OTELC_FUNC("%p, %hhu, %" PRId64 ", \"%s\", %p, %zu, %p, %zu, 0x%02hhx, %p, %p, %zu, \"%s\", %p", logger, severity, event_id, OTELC_STR_ARG(event_name), span_id, span_id_size, trace_id, trace_id_size, trace_flags, ts, attr, attr_len, OTELC_STR_ARG(format), ap);

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(format))
		OTEL_LOGGER_RETURN_INT("Invalid format string");

	otel_nostd::unique_ptr<otel_logs::LogRecord> log_record;
	auto *logger_ptr = otel_logger.load();
	auto retval = otel_logger_record_create(logger, logger_ptr, severity, event_id, event_name, span_id, span_id_size, trace_id, trace_id_size, trace_flags, ts, attr, attr_len, log_record);
	if (retval <= 0)
		OTELC_RETURN_INT(retval);

	retval = vasprintf(&ptr, format, ap);
	if (retval == -1)
		OTEL_LOGGER_RETURN_INT(OTEL_ERROR_MSG_ENOMEM("log body"));

	log_record->SetBody(ptr);
	logger_ptr->EmitLogRecord(std::move(log_record));

	free(ptr);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_logger_log - logs a formatted message with explicit trace and span identifiers
 *
 * SYNOPSIS
 *   static int otel_logger_log(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, ...)
 *
 * ARGUMENTS
 *   logger        - logger instance
 *   severity      - log severity level
 *   event_id      - numeric event identifier, or 0 to omit
 *   event_name    - event name string, or NULL when event_id is 0
 *   span_id       - a pointer to the span identifier associated with this log entry
 *   span_id_size  - the size of the span identifier buffer
 *   trace_id      - a pointer to the trace identifier associated with this log entry
 *   trace_id_size - the size of the trace identifier buffer
 *   trace_flags   - trace flags associated with the trace
 *   ts            - the timestamp of the log event, or NULL for SDK defaults
 *   attr          - a pointer to an array of key-value attributes to attach to the log record
 *   attr_len      - the number of elements in the 'attr' array
 *   format        - a printf-style format string for the log message
 *   ...           - arguments to be formatted according to the format string
 *
 * DESCRIPTION
 *   Logs a formatted message with the specified severity, explicitly
 *   associating it with trace and span identifiers and enriching it with
 *   timestamp and attributes.  When event_id is greater than zero, the
 *   log record is tagged with the given event identifier and name.
 *
 * RETURN VALUE
 *   Returns the number of characters written to the buffer on success,
 *   or a negative value on error (OTELC_RET_ERROR).
 */
static int otel_logger_log(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, ...)
{
	va_list ap;
	int     retval;

	OTELC_FUNC("%p, %hhu, %" PRId64 ", \"%s\", %p, %zu, %p, %zu, 0x%02hhx, %p, %p, %zu, \"%s\", ...", logger, severity, event_id, OTELC_STR_ARG(event_name), span_id, span_id_size, trace_id, trace_id_size, trace_flags, ts, attr, attr_len, OTELC_STR_ARG(format));

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	va_start(ap, format);
	retval = otel_logger_log_v(logger, severity, event_id, event_name, span_id, span_id_size, trace_id, trace_id_size, trace_flags, ts, attr, attr_len, format, ap);
	va_end(ap);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_logger_log_span - logs a formatted message with optional span context and attributes
 *
 * SYNOPSIS
 *   static int otel_logger_log_span(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const struct otelc_span *span, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, ...)
 *
 * ARGUMENTS
 *   logger     - logger instance
 *   severity   - log severity level
 *   event_id   - numeric event identifier, or 0 to omit
 *   event_name - event name string, or NULL when event_id is 0
 *   span       - span associated with this log entry
 *   ts         - the timestamp of the log event, or NULL for SDK defaults
 *   attr       - a pointer to an array of key-value attributes to attach to the log record
 *   attr_len   - the number of elements in the 'attr' array
 *   format     - a printf-style format string for the log message
 *   ...        - arguments to be formatted according to the format string
 *
 * DESCRIPTION
 *   Logs a formatted message with the specified severity, optionally associated
 *   with a span and enriched with timestamp and attributes.  The log entry can
 *   be correlated with distributed traces when a span is provided.  If span
 *   context retrieval fails, the log is still emitted without trace
 *   correlation.  When event_id is greater than zero, the log record is tagged
 *   with the given event identifier and name.
 *
 * RETURN VALUE
 *   Returns the number of characters written to the buffer on success,
 *   or a negative value on error (OTELC_RET_ERROR).
 */
static int otel_logger_log_span(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const struct otelc_span *span, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, ...)
{
	va_list ap;
	uint8_t span_id_buf[otel_trace::SpanId::kSize] = { 0 }, trace_id_buf[otel_trace::TraceId::kSize] = { 0 }, trace_flags = 0;
	int     retval;

	OTELC_FUNC("%p, %hhu, %" PRId64 ", \"%s\", %p, %p, %p, %zu, \"%s\", ...", logger, severity, event_id, OTELC_STR_ARG(event_name), span, ts, attr, attr_len, OTELC_STR_ARG(format));

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	/* If span context retrieval fails, emit the log without correlation. */
	if (!OTEL_NULL(span))
		if (OTELC_OPS(span, get_id, span_id_buf, sizeof(span_id_buf), trace_id_buf, sizeof(trace_id_buf), &trace_flags) == OTELC_RET_ERROR)
			OTELC_DBG(DEBUG, "%s", (OTEL_NULL(span->tracer) || OTEL_NULL(span->tracer->err)) ? "Unable to retrieve span context" : span->tracer->err);

	va_start(ap, format);
	retval = otel_logger_log_v(logger, severity, event_id, event_name, span_id_buf, sizeof(span_id_buf), trace_id_buf, sizeof(trace_id_buf), trace_flags, ts, attr, attr_len, format, ap);
	va_end(ap);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_logger_log_body - logs an otelc_value body with explicit trace and span identifiers
 *
 * SYNOPSIS
 *   static int otel_logger_log_body(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const struct otelc_value *body)
 *
 * ARGUMENTS
 *   logger        - logger instance
 *   severity      - log severity level
 *   event_id      - numeric event identifier, or 0 to omit
 *   event_name    - event name string, or NULL when event_id is 0
 *   span_id       - a pointer to the span identifier associated with this log entry
 *   span_id_size  - the size of the span identifier buffer
 *   trace_id      - a pointer to the trace identifier associated with this log entry
 *   trace_id_size - the size of the trace identifier buffer
 *   trace_flags   - trace flags associated with the trace
 *   ts            - the timestamp of the log event, or NULL for SDK defaults
 *   attr          - a pointer to an array of key-value attributes to attach to the log record
 *   attr_len      - the number of elements in the 'attr' array
 *   body          - the log body as an otelc_value (int, double, bool, or string)
 *
 * DESCRIPTION
 *   Creates an explicit log record and populates it with the specified
 *   severity, trace context, body value, and attributes.  Unlike
 *   otel_logger_log_v(), which formats a printf-style string, this function
 *   passes the otelc_value body directly to SetBody(), preserving the native
 *   type.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on error.
 */
static int otel_logger_log_body(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const struct otelc_value *body)
{
	OTELC_FUNC("%p, %hhu, %" PRId64 ", \"%s\", %p, %zu, %p, %zu, 0x%02hhx, %p, %p, %zu, %p", logger, severity, event_id, OTELC_STR_ARG(event_name), span_id, span_id_size, trace_id, trace_id_size, trace_flags, ts, attr, attr_len, body);

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (OTEL_NULL(body))
		OTEL_LOGGER_RETURN_INT("Invalid body value");

	otel_nostd::unique_ptr<otel_logs::LogRecord> log_record;
	auto *logger_ptr = otel_logger.load();
	const int retval = otel_logger_record_create(logger, logger_ptr, severity, event_id, event_name, span_id, span_id_size, trace_id, trace_id_size, trace_flags, ts, attr, attr_len, log_record);
	if (retval <= 0)
		OTELC_RETURN_INT(retval);

	if (body->u_type == OTELC_VALUE_NULL)
		log_record->SetBody("");
	else if (OTELC_IN_RANGE(body->u_type, OTELC_VALUE_BOOL, OTELC_VALUE_DATA))
		otelc_value_visit(body, [&](auto val) { log_record->SetBody(val); });
	else
		OTEL_LOGGER_RETURN_INT("Invalid body value type: %d", body->u_type);

	logger_ptr->EmitLogRecord(std::move(log_record));

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_logger_log_body_span - logs an otelc_value body with optional span context
 *
 * SYNOPSIS
 *   static int otel_logger_log_body_span(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const struct otelc_span *span, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const struct otelc_value *body)
 *
 * ARGUMENTS
 *   logger     - logger instance
 *   severity   - log severity level
 *   event_id   - numeric event identifier, or 0 to omit
 *   event_name - event name string, or NULL when event_id is 0
 *   span       - span associated with this log entry
 *   ts         - the timestamp of the log event, or NULL for SDK defaults
 *   attr       - a pointer to an array of key-value attributes to attach to the log record
 *   attr_len   - the number of elements in the 'attr' array
 *   body       - the log body as an otelc_value (int, double, bool, or string)
 *
 * DESCRIPTION
 *   Logs a non-string body value with the specified severity, optionally
 *   associated with a span.  If span context retrieval fails, the log is still
 *   emitted without trace correlation.  Unlike log_span(), which formats a
 *   printf-style string, this function passes the otelc_value directly to
 *   SetBody(), preserving the native type.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on error.
 */
static int otel_logger_log_body_span(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const struct otelc_span *span, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const struct otelc_value *body)
{
	uint8_t span_id_buf[otel_trace::SpanId::kSize] = { 0 }, trace_id_buf[otel_trace::TraceId::kSize] = { 0 }, trace_flags = 0;

	OTELC_FUNC("%p, %hhu, %" PRId64 ", \"%s\", %p, %p, %p, %zu, %p", logger, severity, event_id, OTELC_STR_ARG(event_name), span, ts, attr, attr_len, body);

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	/* If span context retrieval fails, emit the log without correlation. */
	if (!OTEL_NULL(span))
		if (OTELC_OPS(span, get_id, span_id_buf, sizeof(span_id_buf), trace_id_buf, sizeof(trace_id_buf), &trace_flags) == OTELC_RET_ERROR)
			OTELC_DBG(DEBUG, "%s", (OTEL_NULL(span->tracer) || OTEL_NULL(span->tracer->err)) ? "Unable to retrieve span context" : span->tracer->err);

	OTELC_RETURN_INT(otel_logger_log_body(logger, severity, event_id, event_name, span_id_buf, sizeof(span_id_buf), trace_id_buf, sizeof(trace_id_buf), trace_flags, ts, attr, attr_len, body));
}


/***
 * NAME
 *   otel_logger_force_flush - forces the export of any buffered logs
 *
 * SYNOPSIS
 *   static int otel_logger_force_flush(struct otelc_logger *logger, const struct timespec *timeout)
 *
 * ARGUMENTS
 *   logger  - logger instance
 *   timeout - maximum time to wait, or NULL for no limit
 *
 * DESCRIPTION
 *   Forces the logger to export all logs that have been collected but not
 *   yet exported.  This is a blocking call that will not return until the
 *   export is complete.  The optional timeout argument limits how long the
 *   operation may block.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
static int otel_logger_force_flush(struct otelc_logger *logger, const struct timespec *timeout)
{
	OTEL_PROVIDER_OP(LOGGER, logger, ForceFlush, "Unable to force flush logger provider");
}


/***
 * NAME
 *   otel_logger_shutdown - shuts down the underlying logger provider
 *
 * SYNOPSIS
 *   static int otel_logger_shutdown(struct otelc_logger *logger, const struct timespec *timeout)
 *
 * ARGUMENTS
 *   logger  - logger instance
 *   timeout - maximum time to wait, or NULL for no limit
 *
 * DESCRIPTION
 *   Shuts down the underlying logger provider, flushing any buffered logs
 *   and releasing provider resources.  After shutdown, no further logs will
 *   be exported.  The optional timeout argument limits how long the operation
 *   may block.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
static int otel_logger_shutdown(struct otelc_logger *logger, const struct timespec *timeout)
{
	OTEL_PROVIDER_OP(LOGGER, logger, Shutdown, "Unable to shut down logger provider");
}


/***
 * NAME
 *   otel_logger_start - initializes and starts the logger instance
 *
 * SYNOPSIS
 *   static int otel_logger_start(struct otelc_logger *logger)
 *
 * ARGUMENTS
 *   logger - logger instance
 *
 * DESCRIPTION
 *   The logger whose configuration is specified in the OpenTelemetry YAML
 *   configuration file (set by the previous call to the otelc_init() function)
 *   is started.  The function initializes the logger in such a way that the
 *   following components are initialized individually: exporter, processor
 *   and finally provider.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
static int otel_logger_start(struct otelc_logger *logger)
{
	std::shared_ptr<otel_logs::LoggerProvider>                      provider;
	std::vector<std::unique_ptr<otel_sdk_logs::LogRecordProcessor>> processors;
	char                                                            scope_name[OTEL_YAML_BUFSIZ], min_severity[OTEL_YAML_BUFSIZ] = "";
	char                                                            path_p[OTEL_YAML_BUFSIZ], path_e[OTEL_YAML_BUFSIZ];
	int                                                             retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p", logger);

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(retval);

	OTEL_YAML_PATH(path_p, logger->yaml_prefix, "/scope_name");
	retval = yaml_find(logger->ctx->fyd, &(logger->err), 1, "OpenTelemetry logger scope", path_p, scope_name, sizeof(scope_name));
	if (retval < 1)
		OTELC_RETURN_INT(retval);

	logger->scope_name = OTELC_STRDUP(__func__, __LINE__, scope_name);
	if (OTEL_NULL(logger->scope_name))
		OTEL_LOGGER_RETURN_INT(OTEL_ERROR_MSG_ENOMEM("scope name"));

	OTEL_YAML_PATH(path_p, logger->yaml_prefix, "/min_severity");
	if (yaml_find(logger->ctx->fyd, &(logger->err), 0, "OpenTelemetry logger min severity", path_p, min_severity, sizeof(min_severity)) > 0) {
		const auto severity = otelc_logger_severity_parse(min_severity);
		if (severity == OTELC_LOG_SEVERITY_INVALID)
			OTEL_LOGGER_RETURN_INT("'%s': invalid minimum log severity level", min_severity);

		logger->min_severity = severity;
	}

	OTEL_YAML_PATH(path_p, logger->yaml_prefix, OTEL_YAML_PROCESSORS);
	OTEL_YAML_PATH(path_e, logger->yaml_prefix, OTEL_YAML_EXPORTERS);

	/* Build processors and exporters from YAML configuration. */
	if (yaml_is_sequence(logger->ctx->fyd, path_p)) {
		const int count = yaml_get_sequence_len(logger->ctx->fyd, &(logger->err), path_p);
		if (count < 0)
			OTELC_RETURN_INT(OTELC_RET_ERROR);

		int count_exporters = yaml_get_sequence_len(logger->ctx->fyd, &(logger->err), path_e);
		if (count_exporters < 0)
			count_exporters = 0;

		for (int i = 0; i < count; i++) {
			std::unique_ptr<otel_sdk_logs::LogRecordExporter>  exporter;
			std::unique_ptr<otel_sdk_logs::LogRecordProcessor> processor;
			char                                               processor_name[OTEL_YAML_BUFSIZ], exporter_name[OTEL_YAML_BUFSIZ] = "";

			if (yaml_get_sequence_value(logger->ctx->fyd, &(logger->err), path_p, i, processor_name, sizeof(processor_name)) != 1)
				OTELC_RETURN_INT(OTELC_RET_ERROR);

			if (!yaml_is_sequence(logger->ctx->fyd, path_e)) {
				/* Do nothing. */
			}
			else if (i < count_exporters) {
				if (yaml_get_sequence_value(logger->ctx->fyd, &(logger->err), path_e, i, exporter_name, sizeof(exporter_name)) != 1)
					OTELC_RETURN_INT(OTELC_RET_ERROR);
			}
			else if (count_exporters > 0) {
				if (yaml_get_sequence_value(logger->ctx->fyd, &(logger->err), path_e, count_exporters - 1, exporter_name, sizeof(exporter_name)) != 1)
					OTELC_RETURN_INT(OTELC_RET_ERROR);
			}

			if (otel_logger_exporter_create(logger, exporter, exporter_name) != OTELC_RET_OK)
				OTELC_RETURN_INT(OTELC_RET_ERROR);
			else if (otel_logger_processor_create(logger, exporter, processor, processor_name) != OTELC_RET_OK)
				OTELC_RETURN_INT(OTELC_RET_ERROR);

			try {
				OTEL_DBG_THROW();
				processors.push_back(std::move(processor));
			}
			OTEL_CATCH_SIGNAL_RETURN( , OTEL_LOGGER_RETURN_INT, "Unable to add processor")
		}
	} else {
		std::unique_ptr<otel_sdk_logs::LogRecordExporter>  exporter;
		std::unique_ptr<otel_sdk_logs::LogRecordProcessor> processor;

		/* Use default exporter and processor when no sequence is defined. */
		if ((retval = otel_logger_exporter_create(logger, exporter)) == OTELC_RET_ERROR)
			OTELC_RETURN_INT(retval);
		else if ((retval = otel_logger_processor_create(logger, exporter, processor)) == OTELC_RET_ERROR)
			OTELC_RETURN_INT(retval);

		try {
			OTEL_DBG_THROW();
			processors.push_back(std::move(processor));
		}
		OTEL_CATCH_SIGNAL_RETURN( , OTEL_LOGGER_RETURN_INT, "Unable to add processor")
	}

	/* Create the provider and logger, then install them as globals. */
	if ((retval = otel_logger_provider_create(logger, processors, provider)) != OTELC_RET_ERROR) {
		otel_nostd::shared_ptr<otel_logs::Logger> logger_maybe{};

		logger_maybe = provider->GetLogger(logger->scope_name, "", OTELC_SCOPE_VERSION, OTELC_SCOPE_SCHEMA_URL);

		const auto severity = otel_logger_severity(logger, logger->min_severity);
		OTEL_CAST_STATIC(otel_logs_logger *, logger_maybe.get())->SetMinimumSeverity(OTEL_CAST_STATIC(uint8_t, severity));

		otel_logger_owner = std::move(logger_maybe);
		otel_logger.store(otel_logger_owner.get());
		otel_logs::Provider::SetLoggerProvider(provider);
	}

	OTELC_DBG_LOGGER(OTEL, "logger", logger);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_logger_destroy - stops the logger and frees all associated memory
 *
 * SYNOPSIS
 *   static void otel_logger_destroy(struct otelc_logger **logger)
 *
 * ARGUMENTS
 *   logger - address of a logger instance pointer to be stopped and destroyed
 *
 * DESCRIPTION
 *   Stops the logger and releases all resources and memory associated with the
 *   logger instance.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_logger_destroy(struct otelc_logger **logger)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(logger));

	if (OTEL_NULL(logger) || OTEL_NULL(*logger))
		OTELC_RETURN();

	OTELC_DBG_LOGGER(OTEL, "logger", *logger);

	/***
	 * Clear otel_logger before provider teardown to prevent concurrent
	 * callers from accessing a logger that is being destroyed.
	 */
	if (!OTEL_NULL(otel_logger)) {
		otel_logger.store(nullptr);
		otel_logger_owner = {};
	}

	otel_logger_provider_destroy();
	otel_logger_exporter_destroy();

	OTELC_SFREE((*logger)->err);
	OTELC_SFREE((*logger)->scope_name);
	OTELC_SFREE((*logger)->yaml_prefix);
	OTELC_SFREE_CLEAR(*logger);

	OTELC_RETURN();
}


const static struct otelc_logger_ops otel_logger_ops = {
	.enabled          = otel_logger_enabled,          /* Locking not required. */
	.set_min_severity = otel_logger_set_min_severity, /* Locking not required. */
	.log              = otel_logger_log,              /* Locking not required. */
	.log_span         = otel_logger_log_span,         /* Locking not required. */
	.log_body         = otel_logger_log_body,         /* Locking not required. */
	.log_body_span    = otel_logger_log_body_span,    /* Locking not required. */
	.force_flush      = otel_logger_force_flush,      /* Locking not required. */
	.shutdown         = otel_logger_shutdown,         /* Locking not required. */
	.start            = otel_logger_start,            /* Locking not required. */
	.destroy          = otel_logger_destroy,          /* Locking not required. */
};


/***
 * NAME
 *   otel_logger_new - creates a new logger instance in an unstarted state
 *
 * SYNOPSIS
 *   static struct otelc_logger *otel_logger_new(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Allocates and initializes a new logger instance in an unstarted state.
 *   The minimum log severity threshold defaults to OTELC_LOG_SEVERITY_TRACE
 *   (all severity levels allowed).  The caller is responsible for starting
 *   and eventually destroying the logger.
 *
 * RETURN VALUE
 *   Returns a pointer to a newly created logger instance on success, or nullptr
 *   on failure.
 */
static struct otelc_logger *otel_logger_new(void)
{
	struct otelc_logger *retptr = nullptr;

	OTELC_FUNC("");

	if (!OTEL_NULL(retptr = OTEL_CAST_TYPEOF(retptr, OTELC_CALLOC(__func__, __LINE__, 1, sizeof(*retptr))))) {
		retptr->err          = nullptr;
		retptr->scope_name   = nullptr;
		retptr->yaml_prefix  = nullptr;
		retptr->min_severity = OTELC_LOG_SEVERITY_TRACE;
		retptr->ops          = &otel_logger_ops;
	}

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_logger_create - creates and returns a new logger instance
 *
 * SYNOPSIS
 *   struct otelc_logger *otelc_logger_create(const struct otelc_ctx *ctx, char **err)
 *
 * ARGUMENTS
 *   ctx - context returned by otelc_init()
 *   err - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Allocates and initializes a new logger instance by calling
 *   otel_logger_new().  The minimum log severity threshold defaults to
 *   OTELC_LOG_SEVERITY_TRACE (all severity levels allowed) and can be
 *   overridden via the YAML configuration or changed at runtime through the
 *   set_min_severity operation.  On failure, an error message may be written
 *   to *err if provided.  The supplied context must be non-NULL and
 *   is retained by the logger for later configuration lookups.
 *
 * RETURN VALUE
 *   Returns a pointer to a newly created logger instance on success, or nullptr
 *   on failure.
 */
struct otelc_logger *otelc_logger_create(const struct otelc_ctx *ctx, char **err)
{
	struct otelc_logger *retptr = nullptr;

	OTELC_FUNC("%p, %p:%p", ctx, OTELC_DPTR_ARGS(err));

	if (OTEL_NULL(ctx))
		OTEL_ERR_RETURN_PTR("Invalid context");

	if (OTEL_NULL(retptr = otel_logger_new()))
		OTEL_ERR_RETURN_PTR(OTEL_ERROR_MSG_ENOMEM("logger"));

	retptr->ctx = ctx;

	{
		char prefix_buf[OTEL_YAML_BUFSIZ];

		if (yaml_resolve_prefix(ctx->fyd, err, OTEL_YAML_LOGGER_PREFIX, ctx->name, OTEL_YAML_NAME_DEFAULT, prefix_buf, sizeof(prefix_buf)) == OTELC_RET_ERROR) {
			otel_logger_destroy(&retptr);

			OTELC_RETURN_PTR(nullptr);
		}

		retptr->yaml_prefix = OTELC_STRDUP(__func__, __LINE__, prefix_buf);
		if (OTEL_NULL(retptr->yaml_prefix)) {
			otel_logger_destroy(&retptr);

			OTEL_ERR_RETURN_PTR(OTEL_ERROR_MSG_ENOMEM("logger prefix"));
		}
	}

	OTELC_DBG_LOGGER(OTEL, "logger", retptr);

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_logger_severity_parse - converts a severity name string to an enum value
 *
 * SYNOPSIS
 *   otelc_log_severity_t otelc_logger_severity_parse(const char *name)
 *
 * ARGUMENTS
 *   name - severity name string (case-insensitive)
 *
 * DESCRIPTION
 *   Converts a severity name string such as "TRACE", "DEBUG", "INFO", "WARN",
 *   "ERROR" or "FATAL" to the corresponding otelc_log_severity_t value.  The
 *   comparison is case-insensitive and accepts the numbered variants (e.g.,
 *   "TRACE2", "DEBUG4").
 *
 * RETURN VALUE
 *   Returns the matching severity value, or OTELC_LOG_SEVERITY_INVALID if the
 *   name is not recognised.
 */
otelc_log_severity_t otelc_logger_severity_parse(const char *name)
{
#define OTELC_LOG_SEVERITY_DEF(a,b)   { #a, OTELC_LOG_SEVERITY_##a },
	static constexpr struct {
		const char           *name;
		otelc_log_severity_t  severity;
	} severity_names[] = { OTELC_LOG_SEVERITY_DEFINES };
#undef OTELC_LOG_SEVERITY_DEF

	if (OTEL_NULL(name))
		return OTELC_LOG_SEVERITY_INVALID;

	for (size_t i = 0; i < OTELC_TABLESIZE(severity_names); i++)
		if (strcasecmp(severity_names[i].name, name) == 0)
			return severity_names[i].severity;

	return OTELC_LOG_SEVERITY_INVALID;
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
