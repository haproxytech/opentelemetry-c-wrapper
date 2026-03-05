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
#ifndef OPENTELEMETRY_C_WRAPPER_SPAN_H
#define OPENTELEMETRY_C_WRAPPER_SPAN_H

__CPLUSPLUS_DECL_BEGIN

#define OTELC_DBG_SPAN(l,h,p) \
	OTELC_DBG_STRUCT(_##l, h, h " %p:{ %" PRId64 " %p }", (p), (p)->idx, (p)->tracer)
#define OTELC_DBG_SPAN_CONTEXT(l,h,p) \
	OTELC_DBG_STRUCT(_##l, h, h " %p:{ %" PRId64 " }", (p), (p)->idx)

/***
 * otel_trace::StatusCode - Represents the canonical set of status codes of a
 * finished span and is used as an argument to the otelc_span->end_with_options()
 * function.  Defined in <opentelemetry/trace/span_metadata.h>.
 */
typedef enum {
	OTELC_SPAN_STATUS_IGNORE = -1, /* Do not set span status. */
	OTELC_SPAN_STATUS_UNSET,       /* Default status, the operation completed successfully without error. */
	OTELC_SPAN_STATUS_OK,          /* The operation completed successfully, explicitly marked as error-free. */
	OTELC_SPAN_STATUS_ERROR        /* The operation contains an error. */
} otelc_span_status_t;

/***
 * Describes a single span link for use at span creation time.
 */
struct otelc_span_link {
	const struct otelc_span         *span;    /* Linked span, or NULL. */
	const struct otelc_span_context *context; /* Linked span context, or NULL. */
	const struct otelc_kv           *kv;      /* Link attributes, or NULL. */
	size_t                           kv_len;  /* Size of key-value pair array. */
};

/***
 * The span context operations vtable.
 */
struct otelc_span_context;
struct otelc_span_context_ops {
	/***
	 * NAME
	 *   is_valid - checks whether the span context is valid
	 *
	 * SYNOPSIS
	 *   int (*is_valid)(const struct otelc_span_context *context)
	 *
	 * ARGUMENTS
	 *   context - instance of span context
	 *
	 * DESCRIPTION
	 *   Queries the span context to determine whether it contains valid
	 *   trace and span identifiers.  A span context is valid when both
	 *   its trace ID and span ID are non-zero.
	 *
	 * RETURN VALUE
	 *   Returns true if the span context is valid, false if it is not,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*is_valid)(const struct otelc_span_context *context)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   is_sampled - checks whether the span context is sampled
	 *
	 * SYNOPSIS
	 *   int (*is_sampled)(const struct otelc_span_context *context)
	 *
	 * ARGUMENTS
	 *   context - instance of span context
	 *
	 * DESCRIPTION
	 *   Queries the span context to determine whether the sampled flag is
	 *   set in the trace flags.  A sampled span context indicates that the
	 *   trace data should be exported.
	 *
	 * RETURN VALUE
	 *   Returns true if the span context is sampled, false if it is not,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*is_sampled)(const struct otelc_span_context *context)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   is_remote - checks whether the span context was propagated from a remote parent
	 *
	 * SYNOPSIS
	 *   int (*is_remote)(const struct otelc_span_context *context)
	 *
	 * ARGUMENTS
	 *   context - instance of span context
	 *
	 * DESCRIPTION
	 *   Queries the span context to determine whether it was propagated
	 *   from a remote parent service.  A remote span context is one that
	 *   was extracted from a carrier (e.g. HTTP headers or text map) rather
	 *   than created locally.
	 *
	 * RETURN VALUE
	 *   Returns true if the span context is remote, false if it is not,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*is_remote)(const struct otelc_span_context *context)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   get_id - gets span context identifiers
	 *
	 * SYNOPSIS
	 *   int (*get_id)(const struct otelc_span_context *context, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
	 *
	 * ARGUMENTS
	 *   context       - instance of span context
	 *   span_id       - buffer to store the span ID
	 *   span_id_size  - size of the span ID buffer
	 *   trace_id      - buffer to store the trace ID
	 *   trace_id_size - size of the trace ID buffer
	 *   trace_flags   - buffer to store the trace flags
	 *
	 * DESCRIPTION
	 *   Retrieves the span identifier, trace identifier, and trace flags
	 *   from the span context.  Any of the output pointers can be null if
	 *   that specific identifier is not needed.  The provided buffers for
	 *   span_id and trace_id must be large enough to hold the respective
	 *   identifiers.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
	 */
	int (*get_id)(const struct otelc_span_context *context, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   trace_state_get - gets a trace state value by key
	 *
	 * SYNOPSIS
	 *   int (*trace_state_get)(const struct otelc_span_context *context, const char *key, char *value, size_t value_size)
	 *
	 * ARGUMENTS
	 *   context    - instance of span context
	 *   key        - trace state key to look up
	 *   value      - buffer to store the value
	 *   value_size - the size of the value buffer
	 *
	 * DESCRIPTION
	 *   Retrieves the value associated with a key in the W3C trace state
	 *   embedded in the span context.  Follows the snprintf convention:
	 *   writes up to value_size-1 characters plus a NUL terminator when
	 *   value_size is greater than zero, and always returns the total
	 *   length of the value regardless of the buffer size.  The value
	 *   and value_size arguments can be zero/null when only the length
	 *   is needed.
	 *
	 * RETURN VALUE
	 *   Returns the length of the value (excluding NUL) if the key is
	 *   found, 0 if the key is not present, or OTELC_RET_ERROR in case
	 *   of an error.
	 */
	int (*trace_state_get)(const struct otelc_span_context *context, const char *key, char *value, size_t value_size)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   trace_state_entries - gets all trace state key-value pairs
	 *
	 * SYNOPSIS
	 *   int (*trace_state_entries)(const struct otelc_span_context *context, struct otelc_text_map *text_map)
	 *
	 * ARGUMENTS
	 *   context  - instance of span context
	 *   text_map - text map to fill with trace state entries
	 *
	 * DESCRIPTION
	 *   Iterates over all key-value pairs in the W3C trace state and
	 *   appends them to the provided text map.  The caller is responsible
	 *   for freeing the text map contents with otelc_text_map_destroy()
	 *   when done.
	 *
	 * RETURN VALUE
	 *   Returns the number of entries added to the text map,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*trace_state_entries)(const struct otelc_span_context *context, struct otelc_text_map *text_map)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   trace_state_header - serializes the trace state to a W3C header string
	 *
	 * SYNOPSIS
	 *   int (*trace_state_header)(const struct otelc_span_context *context, char *header, size_t header_size)
	 *
	 * ARGUMENTS
	 *   context     - instance of span context
	 *   header      - buffer to store the serialized header
	 *   header_size - size of the header buffer
	 *
	 * DESCRIPTION
	 *   Serializes the W3C trace state to its header string representation.
	 *   Follows the snprintf convention: writes up to header_size-1
	 *   characters plus a NUL terminator when header_size is greater than
	 *   zero, and always returns the total length of the header regardless
	 *   of the buffer size.  The header and header_size arguments can be
	 *   zero/null when only the length is needed.
	 *
	 * RETURN VALUE
	 *   Returns the length of the header string (excluding NUL), 0 if the
	 *   trace state is empty, or OTELC_RET_ERROR in case of an error.
	 */
	int (*trace_state_header)(const struct otelc_span_context *context, char *header, size_t header_size)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   trace_state_empty - checks whether the trace state is empty
	 *
	 * SYNOPSIS
	 *   int (*trace_state_empty)(const struct otelc_span_context *context)
	 *
	 * ARGUMENTS
	 *   context - instance of span context
	 *
	 * DESCRIPTION
	 *   Queries the span context to determine whether its W3C trace state
	 *   contains any key-value pairs.
	 *
	 * RETURN VALUE
	 *   Returns true if the trace state is empty, false if it is not,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*trace_state_empty)(const struct otelc_span_context *context)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   trace_state_set - sets a key-value pair in the trace state
	 *
	 * SYNOPSIS
	 *   int (*trace_state_set)(const struct otelc_span_context *context, const char *key, const char *value, char *header, size_t header_size)
	 *
	 * ARGUMENTS
	 *   context     - instance of span context
	 *   key         - trace state key to set
	 *   value       - trace state value to set
	 *   header      - buffer to store the resulting W3C header string
	 *   header_size - size of the header buffer
	 *
	 * DESCRIPTION
	 *   Creates a modified copy of the W3C trace state with the given
	 *   key-value pair added or updated, then serializes it to a W3C
	 *   header string.  The original trace state in the span context is
	 *   not modified.  Follows the snprintf convention: writes up to
	 *   header_size-1 characters plus a NUL terminator when header_size
	 *   is greater than zero, and always returns the total length of the
	 *   header regardless of the buffer size.  The header and header_size
	 *   arguments can be zero/null when only the length is needed.
	 *
	 * RETURN VALUE
	 *   Returns the length of the resulting header string (excluding NUL),
	 *   0 if the trace state is empty, or OTELC_RET_ERROR in case of an
	 *   error.
	 */
	int (*trace_state_set)(const struct otelc_span_context *context, const char *key, const char *value, char *header, size_t header_size)
		OTELC_NONNULL(1, 2, 3);

	/***
	 * NAME
	 *   trace_state_delete - removes a key from the trace state
	 *
	 * SYNOPSIS
	 *   int (*trace_state_delete)(const struct otelc_span_context *context, const char *key, char *header, size_t header_size)
	 *
	 * ARGUMENTS
	 *   context     - instance of span context
	 *   key         - trace state key to remove
	 *   header      - buffer to store the resulting W3C header string
	 *   header_size - size of the header buffer
	 *
	 * DESCRIPTION
	 *   Creates a modified copy of the W3C trace state with the given key
	 *   removed, then serializes it to a W3C header string.  The original
	 *   trace state in the span context is not modified.  Follows the
	 *   snprintf convention: writes up to header_size-1 characters plus a
	 *   NUL terminator when header_size is greater than zero, and always
	 *   returns the total length of the header regardless of the buffer
	 *   size.  The header and header_size arguments can be zero/null when
	 *   only the length is needed.
	 *
	 * RETURN VALUE
	 *   Returns the length of the resulting header string (excluding NUL),
	 *   0 if the trace state is empty, or OTELC_RET_ERROR in case of an
	 *   error.
	 */
	int (*trace_state_delete)(const struct otelc_span_context *context, const char *key, char *header, size_t header_size)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   destroy - destroys a span context handle
	 *
	 * SYNOPSIS
	 *   void (*destroy)(struct otelc_span_context **context)
	 *
	 * ARGUMENTS
	 *   context - address of a pointer to the span context instance to be destroyed
	 *
	 * DESCRIPTION
	 *   Destroys the specified span context handle.  This involves removing
	 *   the handle from the internal map and freeing the memory associated
	 *   with the C-style otelc_span_context structure.  The underlying C++
	 *   context object is managed by a shared pointer and will be released
	 *   when its reference count drops to zero.  After this function, the
	 *   *context pointer is set to null.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	void (*destroy)(struct otelc_span_context **context)
		OTELC_NONNULL_ALL;
};

/***
 * The span context instance data.
 */
struct otelc_span_context {
	int64_t                              idx; /* The index (key) within the internal otel_span_context structure. */
	const struct otelc_span_context_ops *ops; /* Pointer to the operations vtable. */
};

/***
 * The span operations vtable.
 */
struct otelc_span;
struct otelc_span_ops {
	/***
	 * NAME
	 *   get_id - retrieves the identifiers associated with a span
	 *
	 * SYNOPSIS
	 *   int (*get_id)(const struct otelc_span *span, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
	 *
	 * ARGUMENTS
	 *   span          - span instance
	 *   span_id       - buffer to store the span ID
	 *   span_id_size  - size of the span ID buffer
	 *   trace_id      - buffer to store the trace ID
	 *   trace_id_size - size of the trace ID buffer
	 *   trace_flags   - buffer to store the trace flags
	 *
	 * DESCRIPTION
	 *   Retrieves the unique identifiers associated with a span's context.
	 *   These identifiers are essential for linking spans together into a
	 *   single trace.  Any of the output pointers can be null if that
	 *   specific identifier is not needed.  The provided buffers for
	 *   span_id and trace_id must be large enough to hold the respective
	 *   identifiers.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an
	 *   error.
	 */
	int (*get_id)(const struct otelc_span *span, uint8_t *span_id, size_t span_id_size, uint8_t *trace_id, size_t trace_id_size, uint8_t *trace_flags)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   is_recording - checks whether the span is recording
	 *
	 * SYNOPSIS
	 *   int (*is_recording)(const struct otelc_span *span)
	 *
	 * ARGUMENTS
	 *   span - span instance
	 *
	 * DESCRIPTION
	 *   Queries the underlying span to determine whether it is currently
	 *   recording events and attributes.  A span that is not recording
	 *   (e.g. not sampled) will silently discard any data added to it, so
	 *   callers can use this check to skip expensive attribute or event
	 *   construction.
	 *
	 * RETURN VALUE
	 *   Returns true if the span is recording, false if it is not,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*is_recording)(const struct otelc_span *span)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   end - marks the end of the span
	 *
	 * SYNOPSIS
	 *   void (*end)(struct otelc_span **span)
	 *
	 * ARGUMENTS
	 *   span - span instance
	 *
	 * DESCRIPTION
	 *   Finalizes the operations with span.  It must be the last call
	 *   for the span instance.  This function calls the function
	 *   end_with_options(), which offers additional control over span
	 *   finalization.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	void (*end)(struct otelc_span **span)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   end_with_options - marks the end of the span
	 *
	 * SYNOPSIS
	 *   void (*end_with_options)(struct otelc_span **span, const struct timespec *ts_steady, otelc_span_status_t status, const char *desc)
	 *
	 * ARGUMENTS
	 *   span      - span instance
	 *   ts_steady - time when the span finished (monotonic clock)
	 *   status    - status code of a finished span
	 *   desc      - description of the status
	 *
	 * DESCRIPTION
	 *   Finalizes the operations with span.  It must be the last call for
	 *   the span instance.  The optional <ts_steady> argument sets the end
	 *   time of the span.  <status> is used to set the status of the span
	 *   and its setting can be avoided if OTELC_SPAN_STATUS_IGNORE is used
	 *   as the argument.  <desc> is used as a text description that can be
	 *   set for the span status.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	void (*end_with_options)(struct otelc_span **span, const struct timespec *ts_steady, otelc_span_status_t status, const char *desc)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   set_operation_name - sets the span name
	 *
	 * SYNOPSIS
	 *   void (*set_operation_name)(const struct otelc_span *span, const char *operation_name)
	 *
	 * ARGUMENTS
	 *   span           - span instance
	 *   operation_name - span name
	 *
	 * DESCRIPTION
	 *   Sets the span name.  If used, this will override the name set
	 *   during creation.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	void (*set_operation_name)(const struct otelc_span *span, const char *operation_name)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   set_baggage_var - sets baggage key-value pairs
	 *
	 * SYNOPSIS
	 *   int (*set_baggage_var)(const struct otelc_span *span, const char *key, const char *value, ...)
	 *
	 * ARGUMENTS
	 *   span  - span instance
	 *   key   - baggage name
	 *   value - baggage value
	 *   ...   - additional key-value string pairs, terminated by a NULL key
	 *
	 * DESCRIPTION
	 *   Stores data in a baggage key-value store that can be propagated
	 *   alongside context.  The baggage name and value can be any valid
	 *   UTF-8 character string, with the restriction that the name cannot
	 *   be an empty character string.
	 *
	 * RETURN VALUE
	 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in
	 *   case of an error.
	 */
	int (*set_baggage_var)(const struct otelc_span *span, const char *key, const char *value, ...)
		OTELC_NONNULL(1, 2, 3);

	/***
	 * NAME
	 *   set_baggage_kv_var - sets baggage key-value pairs
	 *
	 * SYNOPSIS
	 *   int (*set_baggage_kv_var)(const struct otelc_span *span, const struct otelc_kv *kv, ...)
	 *
	 * ARGUMENTS
	 *   span - span instance
	 *   kv   - baggage key-value pair
	 *   ...  - additional key-value pairs, terminated by NULL
	 *
	 * DESCRIPTION
	 *   Stores data in a baggage key-value store that can be propagated
	 *   alongside context.  The baggage name and value can be any valid
	 *   UTF-8 character string, with the restriction that the name cannot
	 *   be an empty character string.
	 *
	 * RETURN VALUE
	 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in
	 *   case of an error.
	 */
	int (*set_baggage_kv_var)(const struct otelc_span *span, const struct otelc_kv *kv, ...)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   set_baggage_kv_n - sets baggage key-value pairs
	 *
	 * SYNOPSIS
	 *   int (*set_baggage_kv_n)(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
	 *
	 * ARGUMENTS
	 *   span   - span instance
	 *   kv     - an array of key-value pairs of baggage to be set
	 *   kv_len - size of key-value pair array
	 *
	 * DESCRIPTION
	 *   Stores data in a baggage key-value store that can be propagated
	 *   alongside context.  The baggage name and value can be any valid
	 *   UTF-8 character string, with the restriction that the name cannot
	 *   be an empty character string.
	 *
	 * RETURN VALUE
	 *   Returns the number of saved key-value pairs, or OTELC_RET_ERROR in
	 *   case of an error.
	 */
	int (*set_baggage_kv_n)(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   set_baggage - sets a single baggage entry
	 *
	 * SYNOPSIS
	 *   int (*set_baggage)(const struct otelc_span *span, const char *key, const char *value)
	 *
	 * ARGUMENTS
	 *   span  - span instance
	 *   key   - baggage name
	 *   value - baggage value
	 *
	 * DESCRIPTION
	 *   Stores a single entry in the baggage key-value store.  Unlike
	 *   set_baggage_var which accepts a NULL-terminated variadic list,
	 *   this function sets exactly one key-value pair per call.
	 *
	 * RETURN VALUE
	 *   Returns the number of saved key-value pairs (1),
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*set_baggage)(const struct otelc_span *span, const char *key, const char *value)
		OTELC_NONNULL(1, 2, 3);

	/***
	 * NAME
	 *   get_baggage - gets the value associated with the requested baggage name
	 *
	 * SYNOPSIS
	 *   char *(*get_baggage)(const struct otelc_span *span, const char *key)
	 *
	 * ARGUMENTS
	 *   span - span instance
	 *   key  - baggage name
	 *
	 * DESCRIPTION
	 *   Used to access the value of a baggage key-value pair set by a
	 *   previous event.
	 *
	 * RETURN VALUE
	 *   Returns a newly allocated string containing the value associated
	 *   with the specified name, or NULL if the specified name is not
	 *   present and also in case of error.  The caller is responsible
	 *   for freeing the returned string.
	 */
	char *(*get_baggage)(const struct otelc_span *span, const char *key)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   get_baggage_var - gets the value associated with the requested baggage name
	 *
	 * SYNOPSIS
	 *   struct otelc_text_map *(*get_baggage_var)(const struct otelc_span *span, const char *key, ...)
	 *
	 * ARGUMENTS
	 *   span - span instance
	 *   key  - baggage name
	 *   ...  - additional baggage names, terminated by NULL
	 *
	 * DESCRIPTION
	 *   Used to access the values of baggage key-value pairs set by a
	 *   previous event.  Returns a text map containing the baggage
	 *   key-value pairs that correspond to the baggage names given via the
	 *   function arguments.  The size of the text map (the maximum number
	 *   of contained key-value pairs) is always equal to the number of
	 *   input arguments, regardless of how many values are actually found
	 *   for the given names.  This means that the number of found key-value
	 *   pairs can be less than this amount.
	 *
	 * RETURN VALUE
	 *   Returns a text map containing the baggage key-value pairs, or NULL
	 *   if there was an error.
	 */
	struct otelc_text_map *(*get_baggage_var)(const struct otelc_span *span, const char *key, ...)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   set_attribute_var - sets an attribute on the span
	 *
	 * SYNOPSIS
	 *   int (*set_attribute_var)(const struct otelc_span *span, const char *key, const struct otelc_value *value, ...)
	 *
	 * ARGUMENTS
	 *   span  - span instance
	 *   key   - key of the attribute being set
	 *   value - value of the attribute being set
	 *   ...   - additional key-value pairs, terminated by a NULL key
	 *
	 * DESCRIPTION
	 *   Sets attributes on the span.  An attribute is a key-value pair with
	 *   a unique key that provides additional information about the span.
	 *   If an attribute with the same key has already been set, its value
	 *   will be updated with the new one.
	 *
	 * RETURN VALUE
	 *   Returns the number of attributes set, or OTELC_RET_ERROR in case of
	 *   an error.
	 */
	int (*set_attribute_var)(const struct otelc_span *span, const char *key, const struct otelc_value *value, ...)
		OTELC_NONNULL(1, 2, 3);

	/***
	 * NAME
	 *   set_attribute_kv_var - sets an attribute on the span
	 *
	 * SYNOPSIS
	 *   int (*set_attribute_kv_var)(const struct otelc_span *span, const struct otelc_kv *kv, ...)
	 *
	 * ARGUMENTS
	 *   span - span instance
	 *   kv   - key-value pair of the attribute being set
	 *   ...  - additional key-value pairs, terminated by NULL
	 *
	 * DESCRIPTION
	 *   Sets attributes on the span.  An attribute is a key-value pair with
	 *   a unique key that provides additional information about the span.
	 *   If an attribute with the same key has already been set, its value
	 *   will be updated with the new one.
	 *
	 * RETURN VALUE
	 *   Returns the number of attributes set, or OTELC_RET_ERROR in case of
	 *   an error.
	 */
	int (*set_attribute_kv_var)(const struct otelc_span *span, const struct otelc_kv *kv, ...)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   set_attribute_kv_n - sets an attribute on the span
	 *
	 * SYNOPSIS
	 *   int (*set_attribute_kv_n)(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
	 *
	 * ARGUMENTS
	 *   span   - span instance
	 *   kv     - an array of key-value pairs of attributes to be set
	 *   kv_len - size of key-value pair array
	 *
	 * DESCRIPTION
	 *   Sets attributes on the span.  An attribute is a key-value pair with
	 *   a unique key that provides additional information about the span.
	 *   If an attribute with the same key has already been set, its value
	 *   will be updated with the new one.
	 *
	 * RETURN VALUE
	 *   Returns the number of attributes set, or OTELC_RET_ERROR in case of
	 *   an error.
	 */
	int (*set_attribute_kv_n)(const struct otelc_span *span, const struct otelc_kv *kv, size_t kv_len)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   add_event_var - adds an event to the span
	 *
	 * SYNOPSIS
	 *   int (*add_event_var)(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const char *key, const struct otelc_value *value, ...)
	 *
	 * ARGUMENTS
	 *   span      - span instance
	 *   name      - name of the event being added
	 *   ts_system - time of the event being added
	 *   key       - attribute key for the event being added
	 *   value     - attribute value for the event being added
	 *   ...       - additional attribute key-value pairs, terminated by a NULL key
	 *
	 * DESCRIPTION
	 *   Adds an event to the span.  An event can be customized with a
	 *   timestamp and a set of attributes, which are key-value pairs
	 *   providing additional information.
	 *
	 * RETURN VALUE
	 *   Returns the number of attributes that the added event contains,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*add_event_var)(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const char *key, const struct otelc_value *value, ...)
		OTELC_NONNULL(1, 2, 4, 5);

	/***
	 * NAME
	 *   add_event_kv_var - adds an event to the span
	 *
	 * SYNOPSIS
	 *   int (*add_event_kv_var)(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const struct otelc_kv *kv, ...)
	 *
	 * ARGUMENTS
	 *   span      - span instance
	 *   name      - name of the event being added
	 *   ts_system - time of the event being added
	 *   kv        - key-value pair of the attribute being set
	 *   ...       - additional key-value pairs, terminated by NULL
	 *
	 * DESCRIPTION
	 *   Adds an event to the span.  An event can be customized with a
	 *   timestamp and a set of attributes, which are key-value pairs
	 *   providing additional information.
	 *
	 * RETURN VALUE
	 *   Returns the number of attributes that the added event contains,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*add_event_kv_var)(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const struct otelc_kv *kv, ...)
		OTELC_NONNULL(1, 2, 4);

	/***
	 * NAME
	 *   add_event_kv_n - adds an event to the span
	 *
	 * SYNOPSIS
	 *   int (*add_event_kv_n)(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const struct otelc_kv *kv, size_t kv_len)
	 *
	 * ARGUMENTS
	 *   span      - span instance
	 *   name      - name of the event being added
	 *   ts_system - time of the event being added
	 *   kv        - an array of key-value pairs of attributes to be set
	 *   kv_len    - size of key-value pair array
	 *
	 * DESCRIPTION
	 *   Adds an event to the span.  An event can be customized with a
	 *   timestamp and a set of attributes, which are key-value pairs
	 *   providing additional information.
	 *
	 * RETURN VALUE
	 *   Returns the number of attributes that the added event contains,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*add_event_kv_n)(const struct otelc_span *span, const char *name, const struct timespec *ts_system, const struct otelc_kv *kv, size_t kv_len)
		OTELC_NONNULL(1, 2, 4);

	/***
	 * NAME
	 *   add_link - adds a link to the span
	 *
	 * SYNOPSIS
	 *   int (*add_link)(const struct otelc_span *span, const struct otelc_span *link_span, const struct otelc_span_context *link_context, const struct otelc_kv *kv, size_t kv_len)
	 *
	 * ARGUMENTS
	 *   span         - span instance
	 *   link_span    - span to link to
	 *   link_context - span context to link to
	 *   kv           - an array of key-value pairs of attributes for the link
	 *   kv_len       - size of key-value pair array
	 *
	 * DESCRIPTION
	 *   Adds a link to another span, identified by its span context.  The
	 *   link can be specified either by a span instance (link_span) or by
	 *   a span context (link_context), but not both.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an
	 *   error.
	 */
	int (*add_link)(const struct otelc_span *span, const struct otelc_span *link_span, const struct otelc_span_context *link_context, const struct otelc_kv *kv, size_t kv_len)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   set_status - sets the status of the span
	 *
	 * SYNOPSIS
	 *   int (*set_status)(const struct otelc_span *span, otelc_span_status_t status, const char *desc)
	 *
	 * ARGUMENTS
	 *   span   - span instance
	 *   status - status set on a span
	 *   desc   - description of the status
	 *
	 * DESCRIPTION
	 *   Used to set the status of the span and usually to determine that
	 *   the span has not been successfully completed - that is, that some
	 *   error has occurred (using code OTELC_SPAN_STATUS_ERROR).  By
	 *   default, all spans have a status of OTELC_SPAN_STATUS_UNSET, which
	 *   means that the span operation completed without error.  The
	 *   OTELC_SPAN_STATUS_OK status is reserved for situations where a
	 *   span needs to be explicitly marked as successful.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an
	 *   error.
	 */
	int (*set_status)(const struct otelc_span *span, otelc_span_status_t status, const char *desc)
		OTELC_NONNULL(1);

	/***
	 * NAME
	 *   inject_text_map - injects span context into text map carrier
	 *
	 * SYNOPSIS
	 *   int (*inject_text_map)(const struct otelc_span *span, struct otelc_text_map_writer *carrier)
	 *
	 * ARGUMENTS
	 *   span    - span instance
	 *   carrier - medium used by propagators to write values to
	 *
	 * DESCRIPTION
	 *   Used to store context information in a text map carrier used to
	 *   propagate traces between services.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK if the context is injected into the carrier,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*inject_text_map)(const struct otelc_span *span, struct otelc_text_map_writer *carrier)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   inject_http_headers - injects span context into HTTP headers carrier
	 *
	 * SYNOPSIS
	 *   int (*inject_http_headers)(const struct otelc_span *span, struct otelc_http_headers_writer *carrier)
	 *
	 * ARGUMENTS
	 *   span    - span instance
	 *   carrier - medium used by propagators to write values to
	 *
	 * DESCRIPTION
	 *   Used to store context information in an HTTP headers carrier used
	 *   to propagate traces between services.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK if the context is injected into the carrier,
	 *   or OTELC_RET_ERROR in case of an error.
	 */
	int (*inject_http_headers)(const struct otelc_span *span, struct otelc_http_headers_writer *carrier)
		OTELC_NONNULL_ALL;

	/***
	 * NAME
	 *   record_exception - records an exception event on the span
	 *
	 * SYNOPSIS
	 *   int (*record_exception)(const struct otelc_span *span, const char *type, const char *message, const char *stacktrace, const struct timespec *ts_system, const struct otelc_kv *kv, size_t kv_len)
	 *
	 * ARGUMENTS
	 *   span       - span instance
	 *   type       - exception type or class name
	 *   message    - exception message, or NULL to omit
	 *   stacktrace - stack trace string, or NULL to omit
	 *   ts_system  - time of the exception, or NULL for now
	 *   kv         - an array of additional key-value attributes
	 *   kv_len     - size of key-value pair array
	 *
	 * DESCRIPTION
	 *   Records an exception as an event on the span, following the
	 *   OpenTelemetry semantic conventions.  The event is named "exception"
	 *   and carries standard attributes: exception.type, exception.message,
	 *   and exception.stacktrace.  Additional attributes can be provided
	 *   via the kv array.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an
	 *   error.
	 */
	int (*record_exception)(const struct otelc_span *span, const char *type, const char *message, const char *stacktrace, const struct timespec *ts_system, const struct otelc_kv *kv, size_t kv_len)
		OTELC_NONNULL(1, 2);

	/***
	 * NAME
	 *   destroy - destroys all references related to the specified span
	 *
	 * SYNOPSIS
	 *   void (*destroy)(struct otelc_span **span)
	 *
	 * ARGUMENTS
	 *   span - span instance
	 *
	 * DESCRIPTION
	 *   Destroys all references associated with a specific span, as well
	 *   as deleting the span from the otel_span handle map.  After this
	 *   function is executed, all data related to the span is deleted.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	void (*destroy)(struct otelc_span **span)
		OTELC_NONNULL_ALL;
};

/***
 * The span instance data.
 */
struct otelc_span {
	int64_t                      idx;    /* The index (key) within the internal otel_span structure. */
	struct otelc_tracer         *tracer; /* Pointer to the tracer to which this span belongs. */
	const struct otelc_span_ops *ops;    /* Pointer to the operations vtable. */
};

/***
 * NAME
 *   otelc_span_context_create - creates a span context from raw identifiers
 *
 * SYNOPSIS
 *   struct otelc_span_context *otelc_span_context_create(const uint8_t *trace_id, size_t trace_id_size, const uint8_t *span_id, size_t span_id_size, uint8_t trace_flags, int is_remote, const char *trace_state_header, char **err)
 *
 * ARGUMENTS
 *   trace_id           - 16-byte trace identifier
 *   trace_id_size      - the size of the trace identifier buffer
 *   span_id            - 8-byte span identifier
 *   span_id_size       - the size of the span identifier buffer
 *   trace_flags        - W3C trace flags byte (e.g. 0x01 for sampled)
 *   is_remote          - set to true if propagated from a remote parent
 *   trace_state_header - optional W3C tracestate header string, or NULL
 *   err                - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Constructs a new span context from raw trace_id, span_id, and trace_flags
 *   bytes.  This allows callers to link to external spans or restore context
 *   from storage without round-tripping through text map propagation.  The
 *   optional trace_state_header argument is parsed as a W3C tracestate header
 *   string.
 *
 * RETURN VALUE
 *   Returns a pointer to a newly created span context on success, or nullptr
 *   on failure.
 */
struct otelc_span_context *otelc_span_context_create(const uint8_t *trace_id, size_t trace_id_size, const uint8_t *span_id, size_t span_id_size, uint8_t trace_flags, int is_remote, const char *trace_state_header, char **err);

__CPLUSPLUS_DECL_END
#endif /* OPENTELEMETRY_C_WRAPPER_SPAN_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
