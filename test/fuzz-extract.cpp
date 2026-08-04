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
 *
 * libFuzzer harness for the carrier extraction paths, which consume the
 * traceparent, tracestate and baggage headers a remote peer sends.  Every
 * input is split into carrier entries and presented to both extraction
 * operations of a tracer, and its first line is also passed to
 * otelc_span_context_create() as a trace state header.
 *
 * Build and run through test/fuzz-extract.sh, which prepares a clang build
 * of the library instrumented with -fsanitize=address,fuzzer-no-link so
 * the fuzzer receives coverage feedback from the library code.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <opentelemetry-c-wrapper/include.h>


#define FUZZ_CFG_FILE_ENV    "OTELC_FUZZ_CFG"
#define FUZZ_CTX_NAME        "speed_test"
#define FUZZ_MAX_ENTRIES     16
#define FUZZ_MAX_LINE        512

static struct otelc_ctx    *fuzz_ctx = nullptr;
static struct otelc_tracer *fuzz_tracer = nullptr;


/***
 * NAME
 *   fuzz_log_handler - discards SDK diagnostic messages
 *
 * SYNOPSIS
 *   static void fuzz_log_handler(otelc_log_level_t level, const char *file, int line, const char *msg, const struct otelc_kv *attr, size_t attr_len, void *ctx)
 *
 * ARGUMENTS
 *   level    - severity of the SDK diagnostic message
 *   file     - source file that emitted the message
 *   line     - source line number
 *   msg      - formatted diagnostic message text
 *   attr     - array of key-value attributes associated with the message
 *   attr_len - number of entries in the attr array
 *   ctx      - opaque context pointer (unused)
 *
 * DESCRIPTION
 *   Keeps the fuzzing output readable by dropping the diagnostic messages
 *   that malformed carriers produce on every iteration.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void fuzz_log_handler(otelc_log_level_t level, const char *file, int line, const char *msg, const struct otelc_kv *attr, size_t attr_len, void *ctx)
{
	(void)level;
	(void)file;
	(void)line;
	(void)msg;
	(void)attr;
	(void)attr_len;
	(void)ctx;
}


/***
 * NAME
 *   fuzz_foreach_key - iterates over the entries of a fuzzed carrier
 *
 * SYNOPSIS
 *   static int fuzz_foreach_key(const struct otelc_text_map *text_map, int (*handler)(void *arg, const char *key, const char *value), void *arg)
 *
 * ARGUMENTS
 *   text_map - text map holding the carrier entries
 *   handler  - callback invoked for each key-value pair
 *   arg      - opaque argument passed back to the callback
 *
 * DESCRIPTION
 *   Walks the carrier text map and hands every entry to the propagator
 *   callback, stopping as soon as the callback reports an error.  Shared by
 *   the text map and the HTTP headers reader of this harness.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK when all entries were passed on, or the error the
 *   callback returned.
 */
static int fuzz_foreach_key(const struct otelc_text_map *text_map, int (*handler)(void *arg, const char *key, const char *value), void *arg)
{
	size_t i;
	int    retval = OTELC_RET_OK;

	if ((text_map == NULL) || (handler == NULL))
		return OTELC_RET_ERROR;

	for (i = 0; i < text_map->count; i++) {
		retval = handler(arg, text_map->key[i], text_map->value[i]);
		if (retval != OTELC_RET_OK)
			break;
	}

	return retval;
}


/***
 * NAME
 *   fuzz_text_map_foreach - text map reader iteration callback
 *
 * SYNOPSIS
 *   static int fuzz_text_map_foreach(const struct otelc_text_map_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
 *
 * ARGUMENTS
 *   reader  - text map reader carrier
 *   handler - callback invoked for each key-value pair
 *   arg     - opaque argument passed back to the callback
 *
 * DESCRIPTION
 *   Forwards the iteration to fuzz_foreach_key() over the reader's own text
 *   map.
 *
 * RETURN VALUE
 *   Returns the result of fuzz_foreach_key().
 */
static int fuzz_text_map_foreach(const struct otelc_text_map_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
{
	if (reader == NULL)
		return OTELC_RET_ERROR;

	return fuzz_foreach_key(&(reader->text_map), handler, arg);
}


/***
 * NAME
 *   fuzz_http_headers_foreach - HTTP headers reader iteration callback
 *
 * SYNOPSIS
 *   static int fuzz_http_headers_foreach(const struct otelc_http_headers_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
 *
 * ARGUMENTS
 *   reader  - HTTP headers reader carrier
 *   handler - callback invoked for each key-value pair
 *   arg     - opaque argument passed back to the callback
 *
 * DESCRIPTION
 *   Forwards the iteration to fuzz_foreach_key() over the reader's own text
 *   map.
 *
 * RETURN VALUE
 *   Returns the result of fuzz_foreach_key().
 */
static int fuzz_http_headers_foreach(const struct otelc_http_headers_reader *reader, int (*handler)(void *arg, const char *key, const char *value), void *arg)
{
	if (reader == NULL)
		return OTELC_RET_ERROR;

	return fuzz_foreach_key(&(reader->text_map), handler, arg);
}


/***
 * NAME
 *   fuzz_split_entries - turns the fuzzer input into carrier entries
 *
 * SYNOPSIS
 *   static void fuzz_split_entries(const uint8_t *data, size_t size, struct otelc_text_map *text_map)
 *
 * ARGUMENTS
 *   data     - fuzzer-generated input bytes
 *   size     - number of bytes in the input
 *   text_map - text map that receives the parsed entries
 *
 * DESCRIPTION
 *   Reads the input line by line and splits each line at the first colon
 *   into a header name and value, so the mutation engine can steer both
 *   halves.  A line without a colon becomes a traceparent value, which keeps
 *   the W3C parser reachable from a corpus of bare header values.  Entries
 *   are copied into the text map, which owns and releases them.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void fuzz_split_entries(const uint8_t *data, size_t size, struct otelc_text_map *text_map)
{
	char        line[FUZZ_MAX_LINE], *sep;
	const char *key, *value;
	size_t      offset = 0, len;

	while ((offset < size) && (text_map->count < FUZZ_MAX_ENTRIES)) {
		for (len = 0; ((offset + len) < size) && (data[offset + len] != '\n'); len++)
			;

		if (len >= sizeof(line))
			len = sizeof(line) - 1;

		(void)memcpy(line, data + offset, len);
		line[len] = '\0';

		offset += len + 1;

		if (len == 0)
			continue;

		sep = strchr(line, ':');
		if (sep == NULL) {
			key   = "traceparent";
			value = line;
		}
		else {
			*sep  = '\0';
			key   = line;
			value = sep + 1;
		}

		(void)OTELC_TEXT_MAP_ADD(text_map, key, 0, value, 0, OTELC_TEXT_MAP_AUTO);
	}
}


/***
 * NAME
 *   fuzz_extract - runs both extraction operations over one carrier
 *
 * SYNOPSIS
 *   static void fuzz_extract(const uint8_t *data, size_t size)
 *
 * ARGUMENTS
 *   data - fuzzer-generated input bytes
 *   size - number of bytes in the input
 *
 * DESCRIPTION
 *   Builds a text map reader and an HTTP headers reader from the same input,
 *   extracts a span context through each of them, and destroys whatever was
 *   returned together with the carrier content.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void fuzz_extract(const uint8_t *data, size_t size)
{
	struct otelc_text_map_reader     reader_map;
	struct otelc_http_headers_reader reader_hdr;
	struct otelc_span_context       *context;

	(void)memset(&reader_map, 0, sizeof(reader_map));
	(void)memset(&reader_hdr, 0, sizeof(reader_hdr));

	reader_map.foreach_key = fuzz_text_map_foreach;
	reader_hdr.foreach_key = fuzz_http_headers_foreach;

	if (OTELC_TEXT_MAP_NEW(&(reader_map.text_map), FUZZ_MAX_ENTRIES) == NULL)
		return;
	if (OTELC_TEXT_MAP_NEW(&(reader_hdr.text_map), FUZZ_MAX_ENTRIES) == NULL) {
		otelc_text_map_free(&(reader_map.text_map));

		return;
	}

	fuzz_split_entries(data, size, &(reader_map.text_map));
	fuzz_split_entries(data, size, &(reader_hdr.text_map));

	context = fuzz_tracer->ops->extract_text_map(fuzz_tracer, &reader_map);
	if (context != NULL)
		context->ops->destroy(&context);

	context = fuzz_tracer->ops->extract_http_headers(fuzz_tracer, &reader_hdr);
	if (context != NULL)
		context->ops->destroy(&context);

	otelc_text_map_free(&(reader_map.text_map));
	otelc_text_map_free(&(reader_hdr.text_map));
}


/***
 * NAME
 *   fuzz_span_context - builds a span context from the fuzzed input
 *
 * SYNOPSIS
 *   static void fuzz_span_context(const uint8_t *data, size_t size)
 *
 * ARGUMENTS
 *   data - fuzzer-generated input bytes
 *   size - number of bytes in the input
 *
 * DESCRIPTION
 *   Passes the first input line to otelc_span_context_create() as the trace
 *   state header, with the identifier bytes taken from the input as well, so
 *   the tracestate parser and the identifier validation both run.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void fuzz_span_context(const uint8_t *data, size_t size)
{
	char                       header[FUZZ_MAX_LINE];
	uint8_t                    trace_id[OTELC_TRACE_ID_SIZE], span_id[OTELC_SPAN_ID_SIZE];
	struct otelc_span_context *context;
	char                      *err = NULL;
	size_t                     len;

	for (len = 0; (len < size) && (len < (sizeof(header) - 1)) && (data[len] != '\n'); len++)
		header[len] = (char)data[len];
	header[len] = '\0';

	(void)memset(trace_id, 0, sizeof(trace_id));
	(void)memset(span_id, 0, sizeof(span_id));
	(void)memcpy(trace_id, data, (size < sizeof(trace_id)) ? size : sizeof(trace_id));
	(void)memcpy(span_id, data, (size < sizeof(span_id)) ? size : sizeof(span_id));

	context = otelc_span_context_create(trace_id, sizeof(trace_id), span_id, sizeof(span_id), data[0], 1, header, &err);
	if (context != NULL)
		context->ops->destroy(&context);

	OTELC_SFREE(err);
}


/***
 * NAME
 *   LLVMFuzzerInitialize - one-time fuzzing process setup
 *
 * SYNOPSIS
 *   extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
 *
 * ARGUMENTS
 *   argc - address of the argument counter (unused)
 *   argv - address of the argument array (unused)
 *
 * DESCRIPTION
 *   Initializes the library runtime, silences the diagnostic log handler and
 *   creates the single tracer that every input reuses; the configuration file
 *   comes from the OTELC_FUZZ_CFG environment variable.  A failure here is
 *   fatal, since the harness cannot run without a started tracer.
 *
 * RETURN VALUE
 *   Always returns 0.
 */
extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	const char *cfg_file;
	char       *err = NULL;

	(void)argc;
	(void)argv;

	(void)otelc_runtime();
	otelc_ext_init(NULL, NULL, NULL);
	otelc_log_set_handler(fuzz_log_handler, NULL, false);

	cfg_file = getenv(FUZZ_CFG_FILE_ENV);
	if (cfg_file == NULL) {
		(void)fprintf(stderr, "ERROR: %s not set\n", FUZZ_CFG_FILE_ENV);
		exit(EXIT_FAILURE);
	}

	fuzz_ctx = otelc_init(cfg_file, FUZZ_CTX_NAME, &err);
	if (fuzz_ctx == NULL) {
		(void)fprintf(stderr, "ERROR: unable to initialize: %s\n", (err == NULL) ? "" : err);
		exit(EXIT_FAILURE);
	}

	fuzz_tracer = otelc_tracer_create(fuzz_ctx, &err);
	if (fuzz_tracer == NULL) {
		(void)fprintf(stderr, "ERROR: unable to create the tracer: %s\n", (err == NULL) ? "" : err);
		exit(EXIT_FAILURE);
	}

	if (fuzz_tracer->ops->start(fuzz_tracer) != OTELC_RET_OK) {
		(void)fprintf(stderr, "ERROR: unable to start the tracer: %s\n", (fuzz_tracer->err == NULL) ? "" : fuzz_tracer->err);
		exit(EXIT_FAILURE);
	}

	OTELC_SFREE(err);

	return 0;
}


/***
 * NAME
 *   LLVMFuzzerTestOneInput - fuzzing entry point
 *
 * SYNOPSIS
 *   extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
 *
 * ARGUMENTS
 *   data - fuzzer-generated input bytes
 *   size - number of bytes in the input
 *
 * DESCRIPTION
 *   Presents the input to both carrier extraction operations and to the span
 *   context constructor.  An empty input is skipped, since every consumer
 *   here needs at least one byte.
 *
 * RETURN VALUE
 *   Always returns 0.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size == 0)
		return 0;

	fuzz_extract(data, size);
	fuzz_span_context(data, size);

	return 0;
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
