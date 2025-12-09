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


struct fy_document *otelc_fyd = nullptr;


/***
 * NAME
 *   yaml_open - opens and parses a YAML file
 *
 * SYNOPSIS
 *   struct fy_document *yaml_open(const char *file, char **err)
 *
 * ARGUMENTS
 *   file - path to the YAML file to be opened
 *   err  - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Opens and parses the specified YAML file into a document object.  If an
 *   error occurs during file access or parsing, a descriptive error message
 *   is returned through the err parameter.  The caller is responsible for
 *   closing the returned document using yaml_close() to release resources.
 *
 * RETURN VALUE
 *   Returns a pointer to the parsed fy_document on success, or nullptr on
 *   failure.
 */
struct fy_document *yaml_open(const char *file, char **err)
{
	struct fy_document *retptr = nullptr;

	OTELC_FUNC("\"%s\", %p:%p", OTELC_STR_ARG(file), OTELC_DPTR_ARGS(err));

	if (OTEL_NULL(file))
		OTEL_ERETURN_PTR("YAML file name not specified");

	retptr = fy_document_build_from_file(nullptr, file);
	if (OTEL_NULL(retptr))
		OTEL_ERROR("'%s': unable to parse OpenTelemetry configuration", file);

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   yaml_close - closes a YAML document and releases resources
 *
 * SYNOPSIS
 *   void yaml_close(struct fy_document **fyd)
 *
 * ARGUMENTS
 *   fyd - address of a pointer to the YAML document to be closed
 *
 * DESCRIPTION
 *   Releases all resources associated with the specified YAML document,
 *   including the parsed document structure.  After this function returns,
 *   the document pointer is set to nullptr.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void yaml_close(struct fy_document **fyd)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(fyd));

	if (OTEL_NULL(fyd) || OTEL_NULL(*fyd))
		OTELC_RETURN();

	fy_document_destroy(*fyd);

	*fyd = nullptr;

	OTELC_RETURN();
}


/***
 * NAME
 *   yaml_read - reads a YAML file and returns its contents as a string
 *
 * SYNOPSIS
 *   char *yaml_read(const char *file, char **err)
 *
 * ARGUMENTS
 *   file - path to the YAML file
 *   err  - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Opens, parses, and converts the specified YAML file into a string
 *   representation.  If an error occurs, a descriptive message is returned
 *   through the err parameter.  The caller is responsible for freeing the
 *   returned string.
 *
 * RETURN VALUE
 *   Returns a pointer to a newly allocated string containing the YAML content,
 *   or nullptr on failure.
 */
char *yaml_read(const char *file, char **err)
{
	OTELC_FUNC("\"%s\", %p:%p", OTELC_STR_ARG(file), OTELC_DPTR_ARGS(err));

	if (OTEL_NULL(file))
		OTEL_ERETURN_PTR("YAML file name not specified");

	auto fyd = yaml_open(file, err);
	if (OTEL_NULL(fyd))
		OTEL_ERETURN_PTR("'%s': unable to open YAML file", file);

	OTEL_DEFER(yaml_close(&fyd));

	auto retptr = fy_emit_document_to_string(fyd, OTEL_CAST_STATIC(enum fy_emitter_cfg_flags, 0));
	if (OTEL_NULL(retptr))
		OTEL_ERROR("'%s': unable to emit YAML document", file);

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   yaml_find - finds a string value in a YAML document by path
 *
 * SYNOPSIS
 *   int yaml_find(struct fy_document *fyd, char **err, bool is_mandatory, const char *desc, const char *path, char *data, size_t data_size)
 *
 * ARGUMENTS
 *   fyd          - pointer to the YAML document
 *   err          - address of a pointer to store an error message on failure
 *   is_mandatory - if true, an error is returned if the path is not found
 *   desc         - description of the value being searched for, used in error messages
 *   path         - the path expression to the desired node
 *   data         - buffer to store the found string value
 *   data_size    - size of the data buffer
 *
 * DESCRIPTION
 *   Searches for a string value in the YAML document using the specified path.
 *   If the node is found, its value is returned through the data parameter.
 *   If the node is not found and is_mandatory is true, an error is reported.
 *
 * RETURN VALUE
 *   Returns 1 on success, OTELC_RET_ERROR on failure, or 0 if the node is not
 *   found and is not mandatory.
 */
int yaml_find(struct fy_document *fyd, char **err, bool is_mandatory, const char *desc, const char *path, char *data, size_t data_size)
{
	char buffer[OTEL_YAML_BUFSIZ], fmt[OTEL_YAML_BUFSIZ << 1];

	OTELC_FUNC("%p, %p:%p, %hhu, \"%s\", \"%s\", %p, %zu", fyd, OTELC_DPTR_ARGS(err), is_mandatory, OTELC_STR_ARG(desc), OTELC_STR_ARG(path), data, data_size);

	if (OTEL_NULL(fyd))
		OTEL_ERETURN_INT("YAML document name not specified");
	else if (OTEL_NULL(desc))
		OTEL_ERETURN_INT("YAML node description not specified");
	else if (OTEL_NULL(path) || (*path == '\0'))
		OTEL_ERETURN_INT("YAML node path not specified");
	else if (OTEL_NULL(data) || (data_size == 0))
		OTEL_ERETURN_INT("Data buffer not specified or has zero size");

	*data = '\0';

	/***
	 * NOTE: The use of the fy_node_compare_text()/fy_node_compare_string()
	 * functions is not satisfactory because instead of checking the entire
	 * string, they return a positive result for a substring.
	 */
	(void)snprintf(fmt, sizeof(fmt), "%s %%" OTEL_YAML_BUFLEN "[^\n]s", path);
PRAGMA_DIAG_IGNORE("-Wformat-nonliteral")
	auto retval = fy_document_scanf(fyd, fmt, buffer);
PRAGMA_DIAG_RESTORE
	if (retval == 1) {
		(void)otelc_strlcpy(data, data_size, buffer, 0);

		OTELC_DBG(DEBUG, "\"%s\"", data);
	}
	else if ((retval == 0) && is_mandatory) {
		OTEL_ERETURN_INT("'%s' is not specified in the YAML document", desc);
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   yaml_get_sequence - extracts a sequence of key-value pairs from a YAML document
 *
 * SYNOPSIS
 *   int yaml_get_sequence(struct fy_document *fyd, char **err, const char *path, struct otelc_text_map **map)
 *
 * ARGUMENTS
 *   fyd  - pointer to the YAML document
 *   err  - address of a pointer to store an error message on failure
 *   path - the path to the YAML sequence node
 *   map  - address of a pointer to a text map where the sequence data will be stored
 *
 * DESCRIPTION
 *   Retrieves a YAML sequence from the document at the specified path and
 *   populates a text map with its key-value pairs.  If the text map does not
 *   exist, it is created.
 *
 * RETURN VALUE
 *   Returns the number of items in the sequence on success, or OTELC_RET_ERROR
 *   on failure.
 */
int yaml_get_sequence(struct fy_document *fyd, char **err, const char *path, struct otelc_text_map **map)
{
	void *iter_seq = nullptr;
	int   retval = OTELC_RET_OK;

	OTELC_FUNC("%p, %p:%p, \"%s\", %p:%p", fyd, OTELC_DPTR_ARGS(err), OTELC_STR_ARG(path), OTELC_DPTR_ARGS(map));

	if (OTEL_NULL(fyd))
		OTEL_ERETURN_INT("YAML document name not specified");
	else if (OTEL_NULL(path) || (*path == '\0'))
		OTEL_ERETURN_INT("YAML node path not specified");
	else if (OTEL_NULL(map))
		OTEL_ERETURN_INT("Text map pointer not specified");

	auto node_seq = fy_node_by_path(fy_document_root(fyd), path, -1, FYNWF_DONT_FOLLOW);
	if (OTEL_NULL(node_seq))
		OTEL_ERETURN_INT("'%s': path does not exist", path);
	else if (!fy_node_is_sequence(node_seq))
		OTEL_ERETURN_INT("'%s': not a YAML sequence", path);

	for (int count = fy_node_sequence_item_count(node_seq); count > 0; count--, retval++) {
		struct fy_node      *node_iter;
		struct fy_node_pair *node_pair;

		OTELC_DBG(DEBUG, "YAML sequence iteration: %d", retval);

		if (OTEL_NULL(node_iter = fy_node_sequence_iterate(node_seq, &iter_seq)))
			OTEL_ERETURN_INT("'%s[%d]': error while iterating YAML sequence", path, retval);

		for (void *iter_map = nullptr; !OTEL_NULL(node_pair = fy_node_mapping_iterate(node_iter, &iter_map)); ) {
			size_t      key_len, value_len;
			const char *key   = fy_node_get_scalar(fy_node_pair_key(node_pair), &key_len);
			const char *value = fy_node_get_scalar(fy_node_pair_value(node_pair), &value_len);

			if (OTEL_NULL(*map))
				if (OTEL_NULL(*map = OTELC_TEXT_MAP_NEW(nullptr, count)))
					OTEL_ERETURN_INT(OTEL_ERROR_MSG_ENOMEM("text map"));

			if (OTELC_TEXT_MAP_ADD(*map, key, key_len, value, value_len, OTELC_TEXT_MAP_AUTO) == OTELC_RET_ERROR)
				OTEL_ERETURN_INT("Unable to add a key-value pair to a text map");
		}
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   yaml_find_sequence - finds and extracts a named sequence from a YAML document
 *
 * SYNOPSIS
 *   int yaml_find_sequence(struct fy_document *fyd, char **err, bool is_mandatory, const char *path, const char *sequence, struct otelc_text_map **map)
 *
 * ARGUMENTS
 *   fyd          - pointer to the YAML document
 *   err          - address of a pointer to store an error message on failure
 *   is_mandatory - if true, an error is returned if the sequence is not found
 *   path         - the base path to search for the sequence
 *   sequence     - the name of the sequence to find
 *   map          - address of a pointer to a text map to store the sequence data
 *
 * DESCRIPTION
 *   Finds a sequence by name within a YAML document and extracts its key-value
 *   pairs into a text map.  This function first locates the sequence name at
 *   the given path and then retrieves the sequence data itself.
 *
 *   NOTE: Unused for now; kept for potential future use.
 *
 * RETURN VALUE
 *   Returns the number of items in the sequence on success, OTELC_RET_ERROR on
 *   failure, or 0 if the sequence is not found and is not mandatory.
 */
int yaml_find_sequence(struct fy_document *fyd, char **err, bool is_mandatory, const char *path, const char *sequence, struct otelc_text_map **map)
{
	char path_seq[OTEL_YAML_BUFSIZ << 1], arg[OTEL_YAML_BUFSIZ];

	OTELC_FUNC("%p, %p:%p, %hhu, \"%s\", \"%s\", %p:%p", fyd, OTELC_DPTR_ARGS(err), is_mandatory, OTELC_STR_ARG(path), OTELC_STR_ARG(sequence), OTELC_DPTR_ARGS(map));

	if (OTEL_NULL(sequence) || (*sequence == '\0'))
		OTEL_ERETURN_INT("YAML sequence name not specified");

	const auto retval = yaml_find(fyd, err, is_mandatory, sequence, path, arg, sizeof(arg));
	if (retval < 1)
		OTELC_RETURN_INT(retval);

	(void)snprintf(path_seq, sizeof(path_seq), "/%s/%s", sequence, arg);
	OTELC_DBG(DEBUG, "sequence path: '%s'", path_seq);

	OTELC_RETURN_INT(yaml_get_sequence(fyd, err, path_seq, map));
}


/***
 * NAME
 *   yaml_get_node - retrieves a YAML node and its properties
 *
 * SYNOPSIS
 *   int yaml_get_node(struct fy_document *fyd, char **err, bool is_mandatory, const char *desc, const char *path, int type, ...)
 *
 * ARGUMENTS
 *   fyd          - pointer to the YAML document
 *   err          - address of a pointer to store an error message on failure
 *   is_mandatory - if true, an error is returned if the node is not found
 *   desc         - description of the node, used in error messages
 *   path         - the path to the node in the YAML document
 *   type         - the expected type of the node value
 *   ...          - repeating groups of type, mandatory flag, path and storage pointer, terminated by OTEL_YAML_END
 *
 * DESCRIPTION
 *   Retrieves a node from a YAML document and parses its value according to
 *   the specified type.  This function supports various data types, such as
 *   strings, booleans, integers, and doubles, and performs validation and
 *   range checking.
 *
 * RETURN VALUE
 *   Returns the number of successfully parsed properties on success,
 *   or OTELC_RET_ERROR on failure.
 */
int yaml_get_node(struct fy_document *fyd, char **err, bool is_mandatory, const char *desc, const char *path, int type, ...)
{
	va_list ap;
	char    fmt[OTEL_YAML_BUFSIZ << 1], subfmt[OTEL_YAML_BUFSIZ << 1], subarg[OTEL_YAML_BUFSIZ], arg[OTEL_YAML_BUFSIZ];
	int     rc;

	OTELC_FUNC("%p, %p:%p, %hhu, \"%s\", \"%s\", %d, ...", fyd, OTELC_DPTR_ARGS(err), is_mandatory, OTELC_STR_ARG(desc), OTELC_STR_ARG(path), type);

	if (OTEL_NULL(fyd))
		OTEL_ERETURN_INT("YAML document not specified");
	else if (OTEL_NULL(desc))
		OTEL_ERETURN_INT("YAML node description not specified");
	else if (OTEL_NULL(path) || (*path == '\0'))
		OTEL_ERETURN_INT("YAML node path not specified");

	auto retval = yaml_find(fyd, err, is_mandatory, desc, path, arg, sizeof(arg));
	if (retval < 1)
		OTELC_RETURN_INT(retval);

	OTEL_VA_AUTO(ap, type);
	for ( ; type != OTEL_YAML_END; type = va_arg(ap, typeof(type))) {
		int         arg_is_mandatory = va_arg(ap, typeof(arg_is_mandatory));
		const char *arg_path         = va_arg(ap, typeof(arg_path));
		const char *path_desc        = strrchr(arg_path, '/');

		*subarg = '\0';

		OTELC_DBG(DEBUG, "args: %d %d '%s' '%s'", type, arg_is_mandatory, OTELC_STR_ARG(arg_path), OTELC_STR_ARG(path_desc));

		if (OTEL_NULL(path_desc))
			OTEL_ERETURN_INT("'%s': invalid %s path", arg_path, desc);
		else
			path_desc++;

		if (type == OTEL_YAML_MAP) {
			struct otelc_text_map **arg_map = va_arg(ap, typeof(arg_map));

PRAGMA_DIAG_IGNORE("-Wformat-nonliteral")
			(void)snprintf(fmt, sizeof(fmt), arg_path, arg);
PRAGMA_DIAG_RESTORE
			rc = yaml_get_sequence(fyd, err, fmt, arg_map);
			if (rc == OTELC_RET_ERROR)
				OTELC_RETURN_INT(OTELC_RET_ERROR);
			else if (rc > 0)
				retval++;
			else if (arg_is_mandatory != 0)
				OTEL_ERETURN_INT("%s %s not specified", desc, path_desc);

			continue;
		}

		(void)snprintf(subfmt, sizeof(subfmt), "%s %%" OTEL_YAML_BUFLEN "[^\n]s", arg_path);
PRAGMA_DIAG_IGNORE("-Wformat-nonliteral")
		(void)snprintf(fmt, sizeof(fmt), subfmt, arg);
		rc = fy_document_scanf(fyd, fmt, subarg);
PRAGMA_DIAG_RESTORE
		if (rc == -1)
			OTEL_ERETURN_INT("Unable to read %s %s", desc, path_desc);
		else if (rc == 1)
			retval++;
		else if (arg_is_mandatory != 0)
			OTEL_ERETURN_INT("%s %s not specified", desc, path_desc);

		OTELC_DBG(DEBUG, "found: '%s'", subarg);

		if (type == OTEL_YAML_STR) {
			char *arg_str_ptr  = va_arg(ap, typeof(arg_str_ptr));
			int   arg_str_size = va_arg(ap, typeof(arg_str_size));

			if (rc != 1)
				continue;

			(void)otelc_strlcpy(arg_str_ptr, arg_str_size, subarg, 0);
		}
		else if (type == OTEL_YAML_BOOL) {
			int *arg_value_ptr = va_arg(ap, typeof(arg_value_ptr));

			if (rc != 1)
				continue;

			if ((strcasecmp(subarg, "true") == 0) || (strcasecmp(subarg, "1") == 0))
				*arg_value_ptr = 1;
			else if ((strcasecmp(subarg, "false") == 0) || (strcasecmp(subarg, "0") == 0))
				*arg_value_ptr = 0;
			else
				OTEL_ERETURN_INT("'%s': invalid %s %s", subarg, desc, path_desc);
		}
		else if (type == OTEL_YAML_INT64) {
			char    *endptr = nullptr;
			int64_t  value;
			int64_t *arg_value_ptr = va_arg(ap, typeof(arg_value_ptr));
			int64_t  arg_value_min = va_arg(ap, typeof(arg_value_min));
			int64_t  arg_value_max = va_arg(ap, typeof(arg_value_max));

			if (rc != 1)
				continue;

			errno = 0;
			value = strtoll(subarg, &endptr, 0);
			if ((*endptr != '\0') || (errno != 0) || !OTELC_IN_RANGE(value, arg_value_min, arg_value_max))
				OTEL_ERETURN_INT("'%s': invalid %s %s", subarg, desc, path_desc);

			*arg_value_ptr = value;
		}
		else if (type == OTEL_YAML_DOUBLE) {
			char   *endptr = nullptr;
			double  value;
			double *arg_value_ptr = va_arg(ap, typeof(arg_value_ptr));
			double  arg_value_min = va_arg(ap, typeof(arg_value_min));
			double  arg_value_max = va_arg(ap, typeof(arg_value_max));

			if (rc != 1)
				continue;

			errno = 0;
			value = strtod(subarg, &endptr);
			if ((*endptr != '\0') || (errno != 0) || !OTELC_IN_RANGE(value, arg_value_min, arg_value_max))
				OTEL_ERETURN_INT("'%s': invalid %s %s", subarg, desc, path_desc);

			*arg_value_ptr = value;
		}
		else {
			OTEL_ERETURN_INT("Invalid data type: %d", type);
		}
	}

	OTELC_RETURN_INT(retval);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
