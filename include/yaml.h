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
#ifndef _OPENTELEMETRY_C_WRAPPER_YAML_H_
#define _OPENTELEMETRY_C_WRAPPER_YAML_H_

#define OTEL_YAML_BUFSIZ_1                4095
#define OTEL_YAML_BUFSIZ                  (OTEL_YAML_BUFSIZ_1 + 1)
#define OTEL_YAML_BUFLEN                  OTELC_STRINGIFY(OTEL_YAML_BUFSIZ_1)

#define OTEL_YAML_NAME_DEFAULT            "default"

/***
 * Builds a YAML lookup path of the form "<prefix><suffix>" into the supplied
 * buffer and evaluates to a pointer to that buffer.  Intended for inline use
 * inside yaml_*() call expressions; the buffer must be an array (so sizeof
 * yields its size).
 */
#define OTEL_YAML_PATH(b,p,s)             (void)snprintf((b), sizeof(b), "%s%s", (p), (s));

#define OTEL_YAML_ARG_STR(m,p,n)          OTEL_YAML_STR,    (m), OTEL_YAML_##p "/%s/" #n, (n), OTEL_CAST_STATIC(int, sizeof(n))
#define OTEL_YAML_ARG_STR_PTR(m,p,n,s)    OTEL_YAML_STR,    (m), OTEL_YAML_##p "/%s/" #n, (n), (s)
#define OTEL_YAML_ARG_BOOL(m,p,n)         OTEL_YAML_BOOL,   (m), OTEL_YAML_##p "/%s/" #n, &(n)
#define OTEL_YAML_ARG_INT64(m,p,n,a,b)    OTEL_YAML_INT64,  (m), OTEL_YAML_##p "/%s/" #n, &(n), INT64_C(a), INT64_C(b)
#define OTEL_YAML_ARG_DOUBLE(m,p,n,a,b)   OTEL_YAML_DOUBLE, (m), OTEL_YAML_##p "/%s/" #n, &(n), (a), (b)
#define OTEL_YAML_ARG_MAP(m,p,n)          OTEL_YAML_MAP,    (m), OTEL_YAML_##p "/%s/" #n, &(n)

typedef enum {
	OTEL_YAML_STR,      /* { char *value, size_t value_size } */
	OTEL_YAML_BOOL,     /* { int *value } */
	OTEL_YAML_INT64,    /* { int64_t *value, int64_t min, int64_t max } */
	OTEL_YAML_DOUBLE,   /* { double *value, double min, double max } */
	OTEL_YAML_MAP,      /* { struct otelc_text_map **map } */
	OTEL_YAML_END = -1, /* Marks the end of a variable-length argument list. */
} otel_yaml_data_t;

#ifdef HAVE_LIBFYAML_H
#  define OTEL_YAML_DOC                   struct fy_document
#else
#  define OTEL_YAML_DOC                   ryml::Tree
#endif


OTEL_YAML_DOC *yaml_open(const char *file, char **err);
void           yaml_close(OTEL_YAML_DOC **fyd);
char          *yaml_read(const char *file, char **err);
int            yaml_resolve_prefix(OTEL_YAML_DOC *fyd, char **err, const char *base, const char *name, const char *fallback, char *buf, size_t buf_size);
int            yaml_find(OTEL_YAML_DOC *fyd, char **err, bool is_mandatory, const char *desc, const char *path, char *data, size_t data_size);
int            yaml_get_sequence(OTEL_YAML_DOC *fyd, char **err, const char *path, struct otelc_text_map **map);
int            yaml_get_sequence_len(OTEL_YAML_DOC *fyd, char **err, const char *path);
bool           yaml_is_sequence(OTEL_YAML_DOC *fyd, const char *path);
int            yaml_get_sequence_value(OTEL_YAML_DOC *fyd, char **err, const char *path, int index, char *data, size_t data_size);
int            yaml_find_sequence(OTEL_YAML_DOC *fyd, char **err, bool is_mandatory, const char *path, const char *sequence, struct otelc_text_map **map);
int            yaml_get_node(OTEL_YAML_DOC *fyd, char **err, bool is_mandatory, const char *desc, const char *path, const char *name, int type, ...);

#endif /* _OPENTELEMETRY_C_WRAPPER_YAML_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
