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
 *   otelc_text_map_writer_set_cb - callback to set a value in a text map
 *
 * SYNOPSIS
 *   static int otelc_text_map_writer_set_cb(struct otelc_text_map_writer *writer, const char *key, const char *value)
 *
 * ARGUMENTS
 *   writer - pointer to the text map writer structure
 *   key    - key of the text map entry
 *   value  - value of the text map entry
 *
 * DESCRIPTION
 *   This function is a callback used during span injection to add a key-value
 *   pair to the text map stored within the writer carrier.  It simplifies the
 *   process of propagating trace context by providing a standardized way to
 *   populate different types of carriers.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if an error occurs.
 */
static int otelc_text_map_writer_set_cb(struct otelc_text_map_writer *writer, const char *key, const char *value)
{
	OTELC_FUNC("%p, \"%s\", \"%s\"", writer, OTELC_STR_ARG(key), OTELC_STR_ARG(value));

	OTELC_RETURN_INT(OTELC_TEXT_MAP_ADD(&(writer->text_map), key, 0, value, 0, OTELC_TEXT_MAP_AUTO));
}


/***
 * NAME
 *   otelc_http_headers_writer_set_cb - callback to set a value in HTTP headers
 *
 * SYNOPSIS
 *   static int otelc_http_headers_writer_set_cb(struct otelc_http_headers_writer *writer, const char *key, const char *value)
 *
 * ARGUMENTS
 *   writer - pointer to the HTTP headers writer structure
 *   key    - key of the HTTP header
 *   value  - value of the HTTP header
 *
 * DESCRIPTION
 *   This function is a callback used during span injection to add a key-value
 *   pair to the HTTP headers stored within the writer carrier.  It ensures
 *   that trace context information is correctly formatted for transmission
 *   over HTTP.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if an error occurs.
 */
static int otelc_http_headers_writer_set_cb(struct otelc_http_headers_writer *writer, const char *key, const char *value)
{
	OTELC_FUNC("%p, \"%s\", \"%s\"", writer, OTELC_STR_ARG(key), OTELC_STR_ARG(value));

	OTELC_RETURN_INT(OTELC_TEXT_MAP_ADD(&(writer->text_map), key, 0, value, 0, OTELC_TEXT_MAP_AUTO));
}


/***
 * NAME
 *   otelc_span_inject_text_map - injects span context into a text map carrier
 *
 * SYNOPSIS
 *   int otelc_span_inject_text_map(const struct otelc_span *span, struct otelc_text_map_writer *carrier)
 *
 * ARGUMENTS
 *   span    - pointer to the span instance
 *   carrier - pointer to the text map writer carrier
 *
 * DESCRIPTION
 *   Injects the context of the given span into the provided text map carrier.
 *   This allows the trace information to be propagated across different
 *   services or components using a text-based format.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if an error occurs.
 */
int otelc_span_inject_text_map(const struct otelc_span *span, struct otelc_text_map_writer *carrier)
{
	OTELC_FUNC("%p, %p", span, carrier);

	if (_NULL(span) || _NULL(carrier))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	(void)memset(carrier, 0, sizeof(*carrier));
	carrier->set = otelc_text_map_writer_set_cb;

	OTELC_RETURN_INT(OTELC_OPS(span, inject_text_map, carrier));
}


/***
 * NAME
 *   otelc_span_inject_http_headers - injects span context into HTTP headers
 *
 * SYNOPSIS
 *   int otelc_span_inject_http_headers(const struct otelc_span *span, struct otelc_http_headers_writer *carrier)
 *
 * ARGUMENTS
 *   span    - pointer to the span instance
 *   carrier - pointer to the HTTP headers writer carrier
 *
 * DESCRIPTION
 *   Injects the context of the given span into the provided HTTP headers
 *   carrier.  This is typically used when making outbound HTTP requests to
 *   ensure that the trace context is passed to the downstream service.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if an error occurs.
 */
int otelc_span_inject_http_headers(const struct otelc_span *span, struct otelc_http_headers_writer *carrier)
{
	OTELC_FUNC("%p, %p", span, carrier);

	if (_NULL(span) || _NULL(carrier))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	(void)memset(carrier, 0, sizeof(*carrier));
	carrier->set = otelc_http_headers_writer_set_cb;

	OTELC_RETURN_INT(OTELC_OPS(span, inject_http_headers, carrier));
}


/***
 * NAME
 *   otelc_text_map_reader_foreach_key_cb - iterates over all keys in a text map
 *
 * SYNOPSIS
 *   static int otelc_text_map_reader_foreach_key_cb(const struct otelc_text_map_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
 *
 * ARGUMENTS
 *   reader  - pointer to the text map reader structure
 *   handler - callback function to handle each key-value pair
 *   arg     - user-defined argument passed to the handler
 *
 * DESCRIPTION
 *   Iterates through all key-value pairs stored in the text map associated
 *   with the reader and calls the provided handler function for each entry.
 *   This is used during trace context extraction to retrieve information
 *   from a carrier.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if the handler returns
 *   an error.
 */
static int otelc_text_map_reader_foreach_key_cb(const struct otelc_text_map_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
{
	size_t i;
	int    retval = OTELC_RET_OK;

	OTELC_FUNC("%p, %p, %p", reader, handler, arg);

	for (i = 0; (retval != OTELC_RET_ERROR) && (i < reader->text_map.count); i++) {
		OTELC_DBG(OTELC, "\"%s\" -> \"%s\"", reader->text_map.key[i], reader->text_map.value[i]);

		retval = handler(arg, reader->text_map.key[i], reader->text_map.value[i]);
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_http_headers_reader_foreach_key_cb - iterates over all HTTP headers
 *
 * SYNOPSIS
 *   static int otelc_http_headers_reader_foreach_key_cb(const struct otelc_http_headers_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
 *
 * ARGUMENTS
 *   reader  - pointer to the HTTP headers reader structure
 *   handler - callback function to handle each key-value pair
 *   arg     - user-defined argument passed to the handler
 *
 * DESCRIPTION
 *   Iterates through all key-value pairs representing HTTP headers in the
 *   reader and calls the provided handler function for each entry.  It
 *   facilitates the extraction of trace context from incoming HTTP requests.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if the handler returns
 *   an error.
 */
static int otelc_http_headers_reader_foreach_key_cb(const struct otelc_http_headers_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
{
	size_t i;
	int    retval = OTELC_RET_OK;

	OTELC_FUNC("%p, %p, %p", reader, handler, arg);

	for (i = 0; (retval != OTELC_RET_ERROR) && (i < reader->text_map.count); i++) {
		OTELC_DBG(OTELC, "\"%s\" -> \"%s\"", reader->text_map.key[i], reader->text_map.value[i]);

		retval = handler(arg, reader->text_map.key[i], reader->text_map.value[i]);
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_tracer_extract_text_map - extracts span context from a text map carrier
 *
 * SYNOPSIS
 *   struct otelc_span_context *otelc_tracer_extract_text_map(struct otelc_tracer *tracer, struct otelc_text_map_reader *carrier, const struct otelc_text_map *text_map)
 *
 * ARGUMENTS
 *   tracer   - pointer to the tracer instance
 *   carrier  - pointer to the text map reader carrier
 *   text_map - pointer to the text map structure containing trace data
 *
 * DESCRIPTION
 *   Extracts the span context from a provided text map carrier.  The extracted
 *   context can then be used to start a new span as a child of the span
 *   represented by the context, enabling trace continuity across process
 *   boundaries.
 *
 * RETURN VALUE
 *   Returns a pointer to the extracted otelc_span_context on success, or NULL
 *   if extraction fails.
 */
struct otelc_span_context *otelc_tracer_extract_text_map(struct otelc_tracer *tracer, struct otelc_text_map_reader *carrier, const struct otelc_text_map *text_map)
{
	OTELC_FUNC("%p, %p, %p", tracer, carrier, text_map);

	if (_NULL(tracer) || _NULL(carrier))
		OTELC_RETURN_PTR(NULL);

	(void)memset(carrier, 0, sizeof(*carrier));
	carrier->foreach_key = otelc_text_map_reader_foreach_key_cb;

	if (_nNULL(text_map))
		(void)memcpy(&(carrier->text_map), text_map, sizeof(carrier->text_map));

	OTELC_RETURN_PTR(OTELC_OPS(tracer, extract_text_map, carrier));
}


/***
 * NAME
 *   otelc_tracer_extract_http_headers - extracts span context from HTTP headers
 *
 * SYNOPSIS
 *   struct otelc_span_context *otelc_tracer_extract_http_headers(struct otelc_tracer *tracer, struct otelc_http_headers_reader *carrier, const struct otelc_text_map *text_map)
 *
 * ARGUMENTS
 *   tracer   - pointer to the tracer instance
 *   carrier  - pointer to the HTTP headers reader carrier
 *   text_map - pointer to the text map structure containing header data
 *
 * DESCRIPTION
 *   Extracts the span context from the provided HTTP headers carrier.  This
 *   is typically used at the start of an incoming HTTP request handler to
 *   continue a trace started by a calling service.
 *
 * RETURN VALUE
 *   Returns a pointer to the extracted otelc_span_context on success, or NULL
 *   if extraction fails.
 */
struct otelc_span_context *otelc_tracer_extract_http_headers(struct otelc_tracer *tracer, struct otelc_http_headers_reader *carrier, const struct otelc_text_map *text_map)
{
	OTELC_FUNC("%p, %p, %p", tracer, carrier, text_map);

	if (_NULL(tracer) || _NULL(carrier))
		OTELC_RETURN_PTR(NULL);

	(void)memset(carrier, 0, sizeof(*carrier));
	carrier->foreach_key = otelc_http_headers_reader_foreach_key_cb;

	if (_nNULL(text_map))
		(void)memcpy(&(carrier->text_map), text_map, sizeof(carrier->text_map));

	OTELC_RETURN_PTR(OTELC_OPS(tracer, extract_http_headers, carrier));
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
