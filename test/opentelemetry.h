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
#ifndef TEST_OPENTELEMETRY_H
#define TEST_OPENTELEMETRY_H

/***
 * NAME
 *   otelc_span_inject_text_map - injects span context into a text map carrier
 *
 * ARGUMENTS
 *   span    - pointer to the span instance
 *   carrier - pointer to the text map writer carrier
 *
 * DESCRIPTION
 *   Injects the context of the given span into the provided text map carrier.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if an error occurs.
 */
int                        otelc_span_inject_text_map(const struct otelc_span *span, struct otelc_text_map_writer *carrier);

/***
 * NAME
 *   otelc_span_inject_http_headers - injects span context into HTTP headers
 *
 * ARGUMENTS
 *   span    - pointer to the span instance
 *   carrier - pointer to the HTTP headers writer carrier
 *
 * DESCRIPTION
 *   Injects the context of the given span into the provided HTTP headers
 *   carrier.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR if an error occurs.
 */
int                        otelc_span_inject_http_headers(const struct otelc_span *span, struct otelc_http_headers_writer *carrier);

/***
 * NAME
 *   otelc_tracer_extract_text_map - extracts span context from a text map carrier
 *
 * ARGUMENTS
 *   tracer   - pointer to the tracer instance
 *   carrier  - pointer to the text map reader carrier
 *   text_map - pointer to the text map structure containing trace data
 *
 * DESCRIPTION
 *   Extracts the span context from a provided text map carrier.
 *
 * RETURN VALUE
 *   Returns a pointer to the extracted otelc_span_context on success, or NULL
 *   if extraction fails.
 */
struct otelc_span_context *otelc_tracer_extract_text_map(struct otelc_tracer *tracer, struct otelc_text_map_reader *carrier, const struct otelc_text_map *text_map);

/***
 * NAME
 *   otelc_tracer_extract_http_headers - extracts span context from HTTP headers
 *
 * ARGUMENTS
 *   tracer   - pointer to the tracer instance
 *   carrier  - pointer to the HTTP headers reader carrier
 *   text_map - pointer to the text map structure containing header data
 *
 * DESCRIPTION
 *   Extracts the span context from the provided HTTP headers carrier.
 *
 * RETURN VALUE
 *   Returns a pointer to the extracted otelc_span_context on success, or NULL
 *   if extraction fails.
 */
struct otelc_span_context *otelc_tracer_extract_http_headers(struct otelc_tracer *tracer, struct otelc_http_headers_reader *carrier, const struct otelc_text_map *text_map);

#endif /* TEST_OPENTELEMETRY_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
