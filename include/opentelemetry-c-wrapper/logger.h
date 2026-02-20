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
#ifndef OPENTELEMETRY_C_WRAPPER_LOGGER_H
#define OPENTELEMETRY_C_WRAPPER_LOGGER_H

__CPLUSPLUS_DECL_BEGIN

#define OTELC_DBG_LOGGER(l,h,p)                                              \
	OTELC_DBG_STRUCT(_##l, h, h " %p:{ %p:\"%s\" %p:\"%s\" %d }", (p),   \
	                 (p)->err, OTELC_STR_ARG((p)->err), (p)->scope_name, \
	                 OTELC_STR_ARG((p)->scope_name), (p)->min_severity)

/* <opentelemetry/logs/severity.h> */
#define OTELC_LOG_SEVERITY_DEFINES                \
	OTELC_LOG_SEVERITY_DEF(INVALID, kInvalid) \
	OTELC_LOG_SEVERITY_DEF(  TRACE,   kTrace) \
	OTELC_LOG_SEVERITY_DEF( TRACE2,  kTrace2) \
	OTELC_LOG_SEVERITY_DEF( TRACE3,  kTrace3) \
	OTELC_LOG_SEVERITY_DEF( TRACE4,  kTrace4) \
	OTELC_LOG_SEVERITY_DEF(  DEBUG,   kDebug) \
	OTELC_LOG_SEVERITY_DEF( DEBUG2,  kDebug2) \
	OTELC_LOG_SEVERITY_DEF( DEBUG3,  kDebug3) \
	OTELC_LOG_SEVERITY_DEF( DEBUG4,  kDebug4) \
	OTELC_LOG_SEVERITY_DEF(   INFO,    kInfo) \
	OTELC_LOG_SEVERITY_DEF(  INFO2,   kInfo2) \
	OTELC_LOG_SEVERITY_DEF(  INFO3,   kInfo3) \
	OTELC_LOG_SEVERITY_DEF(  INFO4,   kInfo4) \
	OTELC_LOG_SEVERITY_DEF(   WARN,    kWarn) \
	OTELC_LOG_SEVERITY_DEF(  WARN2,   kWarn2) \
	OTELC_LOG_SEVERITY_DEF(  WARN3,   kWarn3) \
	OTELC_LOG_SEVERITY_DEF(  WARN4,   kWarn4) \
	OTELC_LOG_SEVERITY_DEF(  ERROR,   kError) \
	OTELC_LOG_SEVERITY_DEF( ERROR2,  kError2) \
	OTELC_LOG_SEVERITY_DEF( ERROR3,  kError3) \
	OTELC_LOG_SEVERITY_DEF( ERROR4,  kError4) \
	OTELC_LOG_SEVERITY_DEF(  FATAL,   kFatal) \
	OTELC_LOG_SEVERITY_DEF( FATAL2,  kFatal2) \
	OTELC_LOG_SEVERITY_DEF( FATAL3,  kFatal3) \
	OTELC_LOG_SEVERITY_DEF( FATAL4,  kFatal4)

#define OTELC_LOG_SEVERITY_DEF(a,b)   OTELC_LOG_SEVERITY_##a,
typedef enum {
        OTELC_LOG_SEVERITY_DEFINES
} otelc_log_severity_t;
#undef OTELC_LOG_SEVERITY_DEF

/***
 * The logger operations vtable.
 */
struct otelc_logger;
struct otelc_logger_ops {
	/***
	 * NAME
	 *   enabled - checks whether the logger is enabled for a severity level
	 *
	 * SYNOPSIS
	 *   int (*enabled)(struct otelc_logger *logger, otelc_log_severity_t severity)
	 *
	 * ARGUMENTS
	 *   logger   - logger instance
	 *   severity - log severity level to check
	 *
	 * DESCRIPTION
	 *   Queries the underlying logger to determine whether it is enabled
	 *   for the given severity level.  Callers can use this check to skip
	 *   expensive log message construction when the logger would discard
	 *   the record anyway.
	 *
	 * RETURN VALUE
	 *   Returns true if the logger is enabled for the given severity,
	 *   false if it is not, or OTELC_RET_ERROR in case of an error.
	 */
	int (*enabled)(struct otelc_logger *logger, otelc_log_severity_t severity)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   set_min_severity - sets the minimum log severity level at runtime
	 *
	 * SYNOPSIS
	 *   int (*set_min_severity)(struct otelc_logger *logger, otelc_log_severity_t severity)
	 *
	 * ARGUMENTS
	 *   logger   - logger instance
	 *   severity - new minimum log severity level
	 *
	 * DESCRIPTION
	 *   Changes the minimum severity threshold of the underlying logger
	 *   at runtime.  After this call, only log records whose severity is
	 *   equal to or greater than the given level are emitted; lower
	 *   severity records are silently discarded.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of
	 *   an error.
	 */
	int (*set_min_severity)(struct otelc_logger *logger, otelc_log_severity_t severity)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   log - logs a formatted message with explicit trace and span identifiers
	 *
	 * SYNOPSIS
	 *   int (*log)(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, ...)
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
	 *   associating it with trace and span identifiers and enriching it
	 *   with timestamp and attributes.  When event_id is greater than
	 *   zero, the log record is tagged with the given event identifier
	 *   and name.
	 *
	 * RETURN VALUE
	 *   Returns the number of characters written to the buffer on success,
	 *   or a negative value on error (OTELC_RET_ERROR).
	 */
	int (*log)(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const uint8_t *span_id, size_t span_id_size, const uint8_t *trace_id, size_t trace_id_size, uint8_t trace_flags, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, ...)
		OTELC_NONNULL(1, 13);

	/***
	 * NAME
	 *   log_span - logs a formatted message with optional span context and attributes
	 *
	 * SYNOPSIS
	 *   int (*log_span)(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const struct otelc_span *span, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, ...)
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
	 *   Logs a formatted message with the specified severity, optionally
	 *   associated with a span and enriched with timestamp and attributes.
	 *   The log entry can be correlated with distributed traces when a span
	 *   is provided.  If span context retrieval fails, the log is still
	 *   emitted without trace correlation.  When event_id is greater than
	 *   zero, the log record is tagged with the given event identifier and
	 *   name.
	 *
	 * RETURN VALUE
	 *   Returns the number of characters written to the buffer on success,
	 *   or a negative value on error (OTELC_RET_ERROR).
	 */
	int (*log_span)(struct otelc_logger *logger, otelc_log_severity_t severity, int64_t event_id, const char *event_name, const struct otelc_span *span, const struct timespec *ts, const struct otelc_kv *attr, size_t attr_len, const char *format, ...)
		OTELC_NONNULL(1, 9);

	/***
	 * NAME
	 *   force_flush - forces the export of any buffered logs
	 *
	 * SYNOPSIS
	 *   int (*force_flush)(struct otelc_logger *logger, const struct timespec *timeout)
	 *
	 * ARGUMENTS
	 *   logger  - logger instance
	 *   timeout - maximum time to wait, or NULL for no limit
	 *
	 * DESCRIPTION
	 *   Forces the logger to export all logs that have been collected but
	 *   not yet exported.  This is a blocking call that will not return
	 *   until the export is complete.  The optional timeout argument
	 *   limits how long the operation may block.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
	 */
	int (*force_flush)(struct otelc_logger *logger, const struct timespec *timeout)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   shutdown - shuts down the underlying logger provider
	 *
	 * SYNOPSIS
	 *   int (*shutdown)(struct otelc_logger *logger, const struct timespec *timeout)
	 *
	 * ARGUMENTS
	 *   logger  - logger instance
	 *   timeout - maximum time to wait, or NULL for no limit
	 *
	 * DESCRIPTION
	 *   Shuts down the underlying logger provider, flushing any
	 *   buffered logs and releasing provider resources.  After
	 *   shutdown, no further logs will be exported.  The optional
	 *   timeout argument limits how long the operation may block.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on
	 *   failure.
	 */
	int (*shutdown)(struct otelc_logger *logger, const struct timespec *timeout)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   start - initializes and starts the logger instance
	 *
	 * SYNOPSIS
	 *   int (*start)(struct otelc_logger *logger)
	 *
	 * ARGUMENTS
	 *   logger - logger instance
	 *
	 * DESCRIPTION
	 *   The logger whose configuration is specified in the OpenTelemetry
	 *   YAML configuration file (set by the previous call to the
	 *   otelc_init() function) is started.  The function initializes the
	 *   logger in such a way that the following components are initialized
	 *   individually: exporter, processor and finally provider.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an
	 *   error.
	 */
	int (*start)(struct otelc_logger *logger)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   destroy - stops the logger and frees all associated memory
	 *
	 * SYNOPSIS
	 *   void (*destroy)(struct otelc_logger **logger)
	 *
	 * ARGUMENTS
	 *   logger - address of a logger instance pointer to be stopped and destroyed
	 *
	 * DESCRIPTION
	 *   Stops the logger and releases all resources and memory associated
	 *   with the logger instance.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	void (*destroy)(struct otelc_logger **logger)
		OTELC_NONNULL_ALL;
};

/***
 * The logger instance data.
 */
struct otelc_logger {
	char                          *err;          /* Character array containing the last library error. */
	char                          *scope_name;   /* Logger instrumentation scope name. */
	otelc_log_severity_t           min_severity; /* Minimum allowed log severity level. */
	const struct otelc_logger_ops *ops;          /* Pointer to the operations vtable. */
};


/***
 * NAME
 *   otelc_logger_create - creates and returns a new logger instance
 *
 * SYNOPSIS
 *   struct otelc_logger *otelc_logger_create(char **err)
 *
 * ARGUMENTS
 *   err - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Allocates and initializes a new logger instance by calling
 *   otel_logger_new().  The minimum log severity threshold defaults to
 *   OTELC_LOG_SEVERITY_TRACE (all severity levels allowed) and can be
 *   overridden via the YAML configuration or changed at runtime through the
 *   set_min_severity operation.  On failure, an error message may be written
 *   to *err if provided.
 *
 * RETURN VALUE
 *   Returns a pointer to a newly created logger instance on success, or nullptr
 *   on failure.
 */
struct otelc_logger *otelc_logger_create(char **err);

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
otelc_log_severity_t otelc_logger_severity_parse(const char *name);

__CPLUSPLUS_DECL_END
#endif /* OPENTELEMETRY_C_WRAPPER_LOGGER_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
