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
#ifndef OPENTELEMETRY_C_WRAPPER_PROPAGATION_H
#define OPENTELEMETRY_C_WRAPPER_PROPAGATION_H

__CPLUSPLUS_DECL_BEGIN

#define OTELC_DBG_CARRIER_WRITER(l,h,p)                                               \
	OTELC_DBG_STRUCT(_##l, h, h " %p:{ { %p %p %zu/%zu %hhu } %p }", (p),         \
	                 (p)->text_map.key, (p)->text_map.value, (p)->text_map.count, \
	                 (p)->text_map.size, (p)->text_map.is_dynamic, (p)->set)
#define OTELC_DBG_CARRIER_READER(l,h,p)                                               \
	OTELC_DBG_STRUCT(_##l, h, h " %p:{ { %p %p %zu/%zu %hhu } %p }", (p),         \
	                 (p)->text_map.key, (p)->text_map.value, (p)->text_map.count, \
	                 (p)->text_map.size, (p)->text_map.is_dynamic, (p)->foreach_key)


/***
 * Used to set information into a span context for propagation, entries are
 * strings in a map of string pointers.
 *
 * Set and get implementations should use the same convention for naming the
 * keys they manipulate.
 */
struct otelc_text_map_writer {
	struct otelc_text_map text_map;

	/***
	 * NAME
	 *   set - sets a key-value pair
	 *
	 * SYNOPSIS
	 *   int (*set)(struct otelc_text_map_writer *writer, const char *key, const char *value)
	 *
	 * ARGUMENTS
	 *   writer - writer instance
	 *   key    - string key
	 *   value  - string value
	 *
	 * DESCRIPTION
	 *   Sets a key-value pair in the writer's underlying text map.  This
	 *   function is responsible for storing the provided key and value,
	 *   which can later be retrieved by a corresponding reader.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
	 */
	int (*set)(struct otelc_text_map_writer *writer, const char *key, const char *value)
		OTELC_NONNULL(1);
};

/***
 * Used to get information from a propagated span context.
 *
 * Set and get implementations should use the same convention for naming the
 * keys they manipulate.
 */
struct otelc_text_map_reader {
	struct otelc_text_map text_map;

	/***
	 * NAME
	 *   foreach_key - iterates over all key-value pairs
	 *
	 * SYNOPSIS
	 *   int (*foreach_key)(const struct otelc_text_map_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
	 *
	 * ARGUMENTS
	 *   reader  - reader instance
	 *   handler - handler function for each key-value pair
	 *   arg     - user-defined content
	 *
	 * DESCRIPTION
	 *   Iterates over all key-value pairs in the reader's underlying text
	 *   map and invokes the provided handler function for each pair.  If
	 *   the handler returns an error, iteration stops immediately.  This
	 *   function is used to extract propagated context from a carrier.
	 *
	 * RETURN VALUE
	 *   Returns an error code indicating success or failure.
	 */
	int (*foreach_key)(const struct otelc_text_map_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
		OTELC_NONNULL(1, 2);
};

/***
 * Used to set HTTP headers.
 */
struct otelc_http_headers_writer {
	struct otelc_text_map text_map;

	/***
	 * NAME
	 *   set - sets a key-value pair
	 *
	 * SYNOPSIS
	 *   int (*set)(struct otelc_http_headers_writer *writer, const char *key, const char *value)
	 *
	 * ARGUMENTS
	 *   writer - writer instance
	 *   key    - string key
	 *   value  - string value
	 *
	 * DESCRIPTION
	 *   Sets a key-value pair in the writer's underlying text map.  This
	 *   function is responsible for storing the provided key and value,
	 *   which can later be retrieved by a corresponding reader.
	 *
	 * RETURN VALUE
	 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
	 */
	int (*set)(struct otelc_http_headers_writer *writer, const char *key, const char *value)
		OTELC_NONNULL(1);
};

/***
 * Used to get HTTP headers.
 */
struct otelc_http_headers_reader {
	struct otelc_text_map text_map;

	/***
	 * NAME
	 *   foreach_key - iterates over all key-value pairs
	 *
	 * SYNOPSIS
	 *   int (*foreach_key)(const struct otelc_http_headers_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
	 *
	 * ARGUMENTS
	 *   reader  - reader instance
	 *   handler - handler function for each key-value pair
	 *   arg     - user-defined content
	 *
	 * DESCRIPTION
	 *   Iterates over all key-value pairs in the reader's underlying text
	 *   map and invokes the provided handler function for each pair.  If
	 *   the handler returns an error, iteration stops immediately.  This
	 *   function is used to extract propagated context from a carrier.
	 *
	 * RETURN VALUE
	 *   Returns an error code indicating success or failure.
	 */
	int (*foreach_key)(const struct otelc_http_headers_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
		OTELC_NONNULL(1, 2);
};

__CPLUSPLUS_DECL_END
#endif /* OPENTELEMETRY_C_WRAPPER_PROPAGATION_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
