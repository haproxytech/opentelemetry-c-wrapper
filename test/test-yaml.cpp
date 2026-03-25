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
#include "../include/include.h"
#include "test-util.h"


#define TEMP_YAML_FILE   "__test_yaml_tmp.yml"

static const char temp_yaml_content[] =
	"test:\n"
	"  scalar_value: hello\n"
	"  bool_value: true\n"
	"  int_value: 42\n"
	"  items:\n"
	"    - alpha\n"
	"    - beta\n"
	"    - gamma\n"
	"  mappings:\n"
	"    - name: one\n"
	"      value: first\n"
	"    - name: two\n"
	"      value: second\n"
	"  node_ref: my_node\n"
	"  group: test_data\n"
	"\n"
	"groups:\n"
	"  test_data:\n"
	"    - key_a: val_a\n"
	"    - key_b: val_b\n"
	"\n"
	"nodes:\n"
	"  my_node:\n"
	"    type: example\n"
	"    enabled: true\n"
	"    count: 99\n"
	"    ratio: 3.14\n"
	"    bad_bool: maybe\n"
	"    big_count: 999999\n"
	"    big_ratio: 999.9\n";


/***
 * NAME
 *   write_temp_yaml - writes the temporary YAML test file
 *
 * SYNOPSIS
 *   static int write_temp_yaml(const char *path)
 *
 * ARGUMENTS
 *   path - path to the temporary YAML file
 *
 * DESCRIPTION
 *   Creates a small YAML file with known content for controlled testing of the
 *   YAML parsing functions.
 *
 * RETURN VALUE
 *   Returns 0 on success, or -1 on failure.
 */
static int write_temp_yaml(const char *path)
{
	FILE *fp;

	fp = fopen(path, "w");
	if (_NULL(fp))
		return -1;

	(void)fwrite(temp_yaml_content, 1, sizeof(temp_yaml_content) - 1, fp);
	(void)fclose(fp);

	return 0;
}


/***
 * NAME
 *   test_yaml_open_valid - tests opening a valid YAML file
 *
 * SYNOPSIS
 *   static void test_yaml_open_valid(const char *yaml_file)
 *
 * ARGUMENTS
 *   yaml_file - path to a valid YAML file
 *
 * DESCRIPTION
 *   Verifies that yaml_open() returns a non-nullptr document pointer when given
 *   a valid YAML file.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_open_valid(const char *yaml_file)
{
	OTEL_YAML_DOC *doc;
	char          *err = nullptr;
	int            result = TEST_FAIL;

	doc = yaml_open(yaml_file, &err);
	if (_nNULL(doc))
		result = TEST_PASS;

	yaml_close(&doc);

	OTELC_SFREE(err);

	test_report("yaml_open valid file", result);
}


/***
 * NAME
 *   test_yaml_open_null - tests opening with nullptr file path
 *
 * SYNOPSIS
 *   static void test_yaml_open_null(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that yaml_open() returns nullptr and sets an error message when
 *   given a nullptr file path.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_open_null(void)
{
	OTEL_YAML_DOC *doc;
	char          *err = nullptr;
	int            result = TEST_FAIL;

	doc = yaml_open(nullptr, &err);
	if (_NULL(doc))
		result = TEST_PASS;

	yaml_close(&doc);

	OTELC_SFREE(err);

	test_report("yaml_open nullptr file", result);
}


/***
 * NAME
 *   test_yaml_open_nonexistent - tests opening a non-existent file
 *
 * SYNOPSIS
 *   static void test_yaml_open_nonexistent(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that yaml_open() returns nullptr when given a path to a file that
 *   does not exist.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_open_nonexistent(void)
{
	OTEL_YAML_DOC *doc;
	char          *err = nullptr;
	int            result = TEST_FAIL;

	doc = yaml_open("/nonexistent/path/to/file.yml", &err);
	if (_NULL(doc))
		result = TEST_PASS;

	yaml_close(&doc);

	OTELC_SFREE(err);

	test_report("yaml_open nonexistent file", result);
}


/***
 * NAME
 *   test_yaml_close_valid - tests closing a valid YAML document
 *
 * SYNOPSIS
 *   static void test_yaml_close_valid(const char *yaml_file)
 *
 * ARGUMENTS
 *   yaml_file - path to a valid YAML file
 *
 * DESCRIPTION
 *   Verifies that yaml_close() sets the document pointer to nullptr after
 *   closing it.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_close_valid(const char *yaml_file)
{
	OTEL_YAML_DOC *doc;
	char          *err = nullptr;
	int            result = TEST_FAIL;

	doc = yaml_open(yaml_file, &err);
	if (_nNULL(doc)) {
		yaml_close(&doc);

		if (_NULL(doc))
			result = TEST_PASS;
	}

	OTELC_SFREE(err);

	test_report("yaml_close sets pointer to nullptr", result);
}


/***
 * NAME
 *   test_yaml_close_null - tests closing a nullptr document
 *
 * SYNOPSIS
 *   static void test_yaml_close_null(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that yaml_close() does not crash when given a nullptr pointer.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_close_null(void)
{
	OTEL_YAML_DOC *doc = nullptr;

	yaml_close(&doc);
	yaml_close(nullptr);

	test_report("yaml_close nullptr (no crash)", TEST_PASS);
}


/***
 * NAME
 *   test_yaml_read_valid - tests reading a valid YAML file
 *
 * SYNOPSIS
 *   static void test_yaml_read_valid(const char *yaml_file)
 *
 * ARGUMENTS
 *   yaml_file - path to a valid YAML file
 *
 * DESCRIPTION
 *   Verifies that yaml_read() returns a non-nullptr string containing YAML
 *   content.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_read_valid(const char *yaml_file)
{
	char *content, *err = nullptr;
	int   result = TEST_FAIL;

	content = yaml_read(yaml_file, &err);
	if (_nNULL(content)) {
		if (strlen(content) > 0)
			result = TEST_PASS;

		OTELC_SFREE(content);
	}

	OTELC_SFREE(err);

	test_report("yaml_read valid file", result);
}


/***
 * NAME
 *   test_yaml_read_null - tests reading with nullptr file path
 *
 * SYNOPSIS
 *   static void test_yaml_read_null(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that yaml_read() returns nullptr when given a nullptr file path.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_read_null(void)
{
	char *content, *err = nullptr;
	int   result = TEST_FAIL;

	content = yaml_read(nullptr, &err);
	if (_NULL(content))
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_read nullptr file", result);
}


/***
 * NAME
 *   test_yaml_read_nonexistent - tests reading a non-existent file
 *
 * SYNOPSIS
 *   static void test_yaml_read_nonexistent(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that yaml_read() returns nullptr when given a path to a file that
 *   does not exist.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_read_nonexistent(void)
{
	char *content, *err = nullptr;
	int   result = TEST_FAIL;

	content = yaml_read("/nonexistent/path/to/file.yml", &err);
	if (_NULL(content))
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_read nonexistent file", result);
}


/***
 * NAME
 *   test_yaml_find_existing - tests finding an existing scalar value
 *
 * SYNOPSIS
 *   static void test_yaml_find_existing(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_find() returns 1 and populates the data pointer when
 *   given a valid path to an existing scalar node.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_find_existing(OTEL_YAML_DOC *doc)
{
	char data[OTEL_YAML_BUFSIZ], *err = nullptr;
	int  result = TEST_FAIL;

	if (yaml_find(doc, &err, true, "scalar_value", "/test/scalar_value", data, sizeof(data)) == 1)
		if (_nNULL(data) && (strcmp(data, "hello") == 0))
			result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_find existing scalar", result);
}


/***
 * NAME
 *   test_yaml_find_nonexistent_mandatory - tests finding a non-existent mandatory path
 *
 * SYNOPSIS
 *   static void test_yaml_find_nonexistent_mandatory(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_find() returns an error when a mandatory path does not
 *   exist in the document.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_find_nonexistent_mandatory(OTEL_YAML_DOC *doc)
{
	char data[OTEL_YAML_BUFSIZ], *err = nullptr;
	int  result = TEST_FAIL;

	if (yaml_find(doc, &err, true, "missing", "/nonexistent/path", data, sizeof(data)) == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_find nonexistent mandatory", result);
}


/***
 * NAME
 *   test_yaml_find_nonexistent_optional - tests finding a non-existent optional path
 *
 * SYNOPSIS
 *   static void test_yaml_find_nonexistent_optional(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_find() returns 0 when a non-mandatory path does not
 *   exist in the document.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_find_nonexistent_optional(OTEL_YAML_DOC *doc)
{
	char data[OTEL_YAML_BUFSIZ], *err = nullptr;
	int  result = TEST_FAIL;

	if (yaml_find(doc, &err, false, "missing", "/nonexistent/path", data, sizeof(data)) == 0)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_find nonexistent optional", result);
}


/***
 * NAME
 *   test_yaml_find_null_doc - tests yaml_find with nullptr document
 *
 * SYNOPSIS
 *   static void test_yaml_find_null_doc(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that yaml_find() returns an error when given a nullptr document
 *   pointer.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_find_null_doc(void)
{
	char data[OTEL_YAML_BUFSIZ], *err = nullptr;
	int  result = TEST_FAIL;

	if (yaml_find(nullptr, &err, true, "test", "/test/value", data, sizeof(data)) == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_find nullptr document", result);
}


/***
 * NAME
 *   test_yaml_find_null_path - tests yaml_find with nullptr path
 *
 * SYNOPSIS
 *   static void test_yaml_find_null_path(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_find() returns an error when given a nullptr path.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_find_null_path(OTEL_YAML_DOC *doc)
{
	char data[OTEL_YAML_BUFSIZ], *err = nullptr;
	int  result = TEST_FAIL;

	if (yaml_find(doc, &err, true, "test", nullptr, data, sizeof(data)) == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_find nullptr path", result);
}


/***
 * NAME
 *   test_yaml_is_sequence_true - tests yaml_is_sequence with a sequence node
 *
 * SYNOPSIS
 *   static void test_yaml_is_sequence_true(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_is_sequence() returns true for a node that is a YAML
 *   sequence.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_is_sequence_true(OTEL_YAML_DOC *doc)
{
	int result = TEST_FAIL;

	if (yaml_is_sequence(doc, "/test/items"))
		result = TEST_PASS;

	test_report("yaml_is_sequence on sequence node", result);
}


/***
 * NAME
 *   test_yaml_is_sequence_false - tests yaml_is_sequence with a scalar node
 *
 * SYNOPSIS
 *   static void test_yaml_is_sequence_false(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_is_sequence() returns false for a scalar node.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_is_sequence_false(OTEL_YAML_DOC *doc)
{
	int result = TEST_FAIL;

	if (!yaml_is_sequence(doc, "/test/scalar_value"))
		result = TEST_PASS;

	test_report("yaml_is_sequence on scalar node", result);
}


/***
 * NAME
 *   test_yaml_is_sequence_nonexistent - tests yaml_is_sequence with non-existent path
 *
 * SYNOPSIS
 *   static void test_yaml_is_sequence_nonexistent(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_is_sequence() returns false for a non-existent path.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_is_sequence_nonexistent(OTEL_YAML_DOC *doc)
{
	int result = TEST_FAIL;

	if (!yaml_is_sequence(doc, "/nonexistent/path"))
		result = TEST_PASS;

	test_report("yaml_is_sequence nonexistent path", result);
}


/***
 * NAME
 *   test_yaml_get_sequence_len_valid - tests getting the length of a valid sequence
 *
 * SYNOPSIS
 *   static void test_yaml_get_sequence_len_valid(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_sequence_len() returns the correct number of items
 *   in a sequence.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_sequence_len_valid(OTEL_YAML_DOC *doc)
{
	char *err = nullptr;
	int   result = TEST_FAIL;

	if (yaml_get_sequence_len(doc, &err, "/test/items") == 3)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_sequence_len valid sequence", result);
}


/***
 * NAME
 *   test_yaml_get_sequence_len_nonseq - tests getting sequence length of a non-sequence
 *
 * SYNOPSIS
 *   static void test_yaml_get_sequence_len_nonseq(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_sequence_len() returns an error for a path that is
 *   not a sequence.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_sequence_len_nonseq(OTEL_YAML_DOC *doc)
{
	char *err = nullptr;
	int   result = TEST_FAIL;

	if (yaml_get_sequence_len(doc, &err, "/test/scalar_value") == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_sequence_len non-sequence", result);
}


/***
 * NAME
 *   test_yaml_get_sequence_value_valid - tests getting a value from a scalar sequence
 *
 * SYNOPSIS
 *   static void test_yaml_get_sequence_value_valid(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_sequence_value() returns the correct scalar value
 *   at a given index in a sequence.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_sequence_value_valid(OTEL_YAML_DOC *doc)
{
	char data[OTEL_YAML_BUFSIZ], *err = nullptr;
	int  result = TEST_FAIL;

	if (yaml_get_sequence_value(doc, &err, "/test/items", 0, data, sizeof(data)) == 1)
		if (_nNULL(data) && (strcmp(data, "alpha") == 0))
			result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_sequence_value valid index", result);
}


/***
 * NAME
 *   test_yaml_get_sequence_value_oob - tests getting a value at an out-of-bounds index
 *
 * SYNOPSIS
 *   static void test_yaml_get_sequence_value_oob(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_sequence_value() returns an error when the index
 *   is out of bounds.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_sequence_value_oob(OTEL_YAML_DOC *doc)
{
	char data[OTEL_YAML_BUFSIZ], *err = nullptr;
	int  result = TEST_FAIL;

	if (yaml_get_sequence_value(doc, &err, "/test/items", 99, data, sizeof(data)) == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_sequence_value out-of-bounds", result);
}


/***
 * NAME
 *   test_yaml_get_sequence_valid - tests extracting a sequence of mappings
 *
 * SYNOPSIS
 *   static void test_yaml_get_sequence_valid(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_sequence() populates a text map with the correct
 *   key-value pairs from a sequence of mappings.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_sequence_valid(OTEL_YAML_DOC *doc)
{
	struct otelc_text_map *map = nullptr;
	char                  *err = nullptr;
	int                    result = TEST_FAIL;

	if (yaml_get_sequence(doc, &err, "/test/mappings", &map) == 2)
		if (_nNULL(map) && (map->count > 0))
			result = TEST_PASS;

	if (_nNULL(map))
		otelc_text_map_destroy(&map);

	OTELC_SFREE(err);

	test_report("yaml_get_sequence mappings", result);
}


/***
 * NAME
 *   test_yaml_find_sequence_valid - tests finding a named sequence
 *
 * SYNOPSIS
 *   static void test_yaml_find_sequence_valid(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_find_sequence() locates a named sequence and populates
 *   a text map with its key-value pairs.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_find_sequence_valid(OTEL_YAML_DOC *doc)
{
	struct otelc_text_map *map = nullptr;
	char                  *err = nullptr;
	int                    result = TEST_FAIL;

	if (yaml_find_sequence(doc, &err, true, "/test/group", "groups", &map) == 2)
		if (_nNULL(map) && (map->count > 0))
			result = TEST_PASS;

	if (_nNULL(map))
		otelc_text_map_destroy(&map);

	OTELC_SFREE(err);

	test_report("yaml_find_sequence valid", result);
}


/***
 * NAME
 *   test_yaml_get_node_from_name_str - tests retrieving a string property by node name
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_from_name_str(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() correctly retrieves a string property
 *   from a named node when called with a direct name.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_from_name_str(OTEL_YAML_DOC *doc)
{
	char type_buf[64] = "", *err = nullptr;
	int   rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_STR, 1, "/nodes/%s/type", type_buf, OTEL_CAST_STATIC(int, sizeof(type_buf)),
		OTEL_YAML_END);

	if (rc >= 1)
		if (strcmp(type_buf, "example") == 0)
			result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node STR via name", result);
}


/***
 * NAME
 *   test_yaml_get_node_from_name_bool - tests retrieving a boolean property by node name
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_from_name_bool(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() correctly retrieves a boolean property
 *   from a named node when called with a direct name.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_from_name_bool(OTEL_YAML_DOC *doc)
{
	char *err = nullptr;
	int   enabled = 0, rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_BOOL, 1, "/nodes/%s/enabled", &enabled,
		OTEL_YAML_END);

	if (rc >= 1)
		if (enabled == 1)
			result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node BOOL via name", result);
}


/***
 * NAME
 *   test_yaml_get_node_from_name_int64 - tests retrieving an integer property by node name
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_from_name_int64(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() correctly retrieves an int64 property
 *   from a named node when called with a direct name.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_from_name_int64(OTEL_YAML_DOC *doc)
{
	char    *err = nullptr;
	int64_t  count = 0;
	int      rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_INT64, 1, "/nodes/%s/count", &count, INT64_C(0), INT64_C(1000),
		OTEL_YAML_END);

	if (rc >= 1)
		if (count == 99)
			result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node INT64 via name", result);
}


/***
 * NAME
 *   test_yaml_get_node_from_name_double - tests retrieving a double property by node name
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_from_name_double(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() correctly retrieves a double property
 *   from a named node and validates it within the given range.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_from_name_double(OTEL_YAML_DOC *doc)
{
	char  *err = nullptr;
	double ratio = 0.0;
	int    rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_DOUBLE, 1, "/nodes/%s/ratio", &ratio, 0.0, 100.0,
		OTEL_YAML_END);

	if (rc >= 1)
		if ((ratio > 3.13) && (ratio < 3.15))
			result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node DOUBLE via name", result);
}


/***
 * NAME
 *   test_yaml_get_node_from_name_multi - tests retrieving multiple properties in one call
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_from_name_multi(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() correctly retrieves multiple properties
 *   of different types in a single variadic call when using a direct name.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_from_name_multi(OTEL_YAML_DOC *doc)
{
	char    type_buf[64] = "", *err = nullptr;
	int64_t count = 0;
	double  ratio = 0.0;
	int     enabled = 0, rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_STR,    1, "/nodes/%s/type",    type_buf, OTEL_CAST_STATIC(int, sizeof(type_buf)),
		OTEL_YAML_BOOL,   1, "/nodes/%s/enabled", &enabled,
		OTEL_YAML_INT64,  1, "/nodes/%s/count",   &count, INT64_C(0), INT64_C(1000),
		OTEL_YAML_DOUBLE, 1, "/nodes/%s/ratio",   &ratio, 0.0, 100.0,
		OTEL_YAML_END);

	if (rc >= 4)
		if ((strcmp(type_buf, "example") == 0) && (enabled == 1) && (count == 99) && (ratio > 3.13) && (ratio < 3.15))
			result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node multi via name", result);
}


/***
 * NAME
 *   test_yaml_get_node_str - tests retrieving a node property via path indirection
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_str(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() resolves the node name from a YAML path
 *   and then retrieves a property using the resolved name.  The path
 *   /test/node_ref contains "my_node", which is then used to look up
 *   /nodes/%s/type.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_str(OTEL_YAML_DOC *doc)
{
	char type_buf[64] = "", *err = nullptr;
	int   rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, true, "node", "/test/node_ref", nullptr,
		OTEL_YAML_STR, 1, "/nodes/%s/type", type_buf, OTEL_CAST_STATIC(int, sizeof(type_buf)),
		OTEL_YAML_END);

	if (rc >= 1)
		if (strcmp(type_buf, "example") == 0)
			result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node STR via path indirection", result);
}


/***
 * NAME
 *   test_yaml_get_node_mandatory_missing - tests missing mandatory property
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_mandatory_missing(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() returns an error when a mandatory property
 *   does not exist in the document.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_mandatory_missing(OTEL_YAML_DOC *doc)
{
	char type_buf[64] = "", *err = nullptr;
	int  rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_STR, 1, "/nodes/%s/nonexistent", type_buf, OTEL_CAST_STATIC(int, sizeof(type_buf)),
		OTEL_YAML_END);

	if (rc == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node mandatory missing", result);
}


/***
 * NAME
 *   test_yaml_get_node_optional_missing - tests missing optional property
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_optional_missing(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() skips a missing optional property without
 *   error and still returns the count of successfully parsed properties.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_optional_missing(OTEL_YAML_DOC *doc)
{
	char type_buf[64] = "", *err = nullptr;
	int  rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_STR, 0, "/nodes/%s/nonexistent", type_buf, OTEL_CAST_STATIC(int, sizeof(type_buf)),
		OTEL_YAML_STR, 1, "/nodes/%s/type", type_buf, OTEL_CAST_STATIC(int, sizeof(type_buf)),
		OTEL_YAML_END);

	if ((rc == 1) && (strcmp(type_buf, "example") == 0))
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node optional missing", result);
}


/***
 * NAME
 *   test_yaml_get_node_invalid_bool - tests rejection of an invalid boolean value
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_invalid_bool(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() returns an error when a BOOL property
 *   contains a value other than "true", "false", "1", or "0".
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_invalid_bool(OTEL_YAML_DOC *doc)
{
	char *err = nullptr;
	int   bad_bool = 0, rc, result = TEST_FAIL;

	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_BOOL, 1, "/nodes/%s/bad_bool", &bad_bool,
		OTEL_YAML_END);

	if (rc == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node invalid bool", result);
}


/***
 * NAME
 *   test_yaml_get_node_int64_out_of_range - tests rejection of out-of-range int64
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_int64_out_of_range(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() returns an error when an INT64 property
 *   value falls outside the specified range.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_int64_out_of_range(OTEL_YAML_DOC *doc)
{
	char    *err = nullptr;
	int64_t  count = 0;
	int      rc, result = TEST_FAIL;

	/* big_count is 999999; range [0, 100] rejects it. */
	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_INT64, 1, "/nodes/%s/big_count", &count, INT64_C(0), INT64_C(100),
		OTEL_YAML_END);

	if (rc == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node int64 out of range", result);
}


/***
 * NAME
 *   test_yaml_get_node_double_out_of_range - tests rejection of out-of-range double
 *
 * SYNOPSIS
 *   static void test_yaml_get_node_double_out_of_range(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_node() returns an error when a DOUBLE property
 *   value falls outside the specified range.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_node_double_out_of_range(OTEL_YAML_DOC *doc)
{
	char  *err = nullptr;
	double ratio = 0.0;
	int    rc, result = TEST_FAIL;

	/* big_ratio is 999.9; range [0.0, 10.0] rejects it. */
	rc = yaml_get_node(doc, &err, false, "node", nullptr, "my_node",
		OTEL_YAML_DOUBLE, 1, "/nodes/%s/big_ratio", &ratio, 0.0, 10.0,
		OTEL_YAML_END);

	if (rc == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_node double out of range", result);
}


/***
 * NAME
 *   test_yaml_get_sequence_value_negative - tests getting a value at a negative index
 *
 * SYNOPSIS
 *   static void test_yaml_get_sequence_value_negative(OTEL_YAML_DOC *doc)
 *
 * ARGUMENTS
 *   doc - pointer to the parsed YAML document
 *
 * DESCRIPTION
 *   Verifies that yaml_get_sequence_value() returns an error when the index
 *   is negative.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_yaml_get_sequence_value_negative(OTEL_YAML_DOC *doc)
{
	char data[OTEL_YAML_BUFSIZ], *err = nullptr;
	int  result = TEST_FAIL;

	if (yaml_get_sequence_value(doc, &err, "/test/items", -1, data, sizeof(data)) == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("yaml_get_sequence_value negative index", result);
}


/***
 * NAME
 *   test_otelc_init_valid - tests library initialization with a valid file
 *
 * SYNOPSIS
 *   static void test_otelc_init_valid(const char *cfg_file)
 *
 * ARGUMENTS
 *   cfg_file - path to the YAML configuration file
 *
 * DESCRIPTION
 *   Verifies that otelc_init() succeeds with a valid configuration file.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_otelc_init_valid(const char *cfg_file)
{
	char *err = nullptr;
	int   result = TEST_FAIL;

	if (otelc_init(cfg_file, &err) == OTELC_RET_OK)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("otelc_init valid file", result);
}


/***
 * NAME
 *   test_otelc_init_null - tests library initialization with nullptr file
 *
 * SYNOPSIS
 *   static void test_otelc_init_null(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that otelc_init() returns an error when given a nullptr
 *   configuration file path.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_otelc_init_null(void)
{
	char *err = nullptr;
	int   result = TEST_FAIL;

	if (otelc_init(nullptr, &err) == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("otelc_init nullptr file", result);
}


/***
 * NAME
 *   test_otelc_init_double - tests double initialization
 *
 * SYNOPSIS
 *   static void test_otelc_init_double(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Verifies that otelc_init() returns an error when the library is already
 *   initialized.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_otelc_init_double(void)
{
	char *err = nullptr;
	int   result = TEST_FAIL;

	/***
	 * The library should already be initialized at this point.
	 */
	if (otelc_init("dummy.yml", &err) == OTELC_RET_ERROR)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("otelc_init double init", result);
}


/***
 * NAME
 *   test_otelc_deinit_reinit - tests deinitialization and reinitialization
 *
 * SYNOPSIS
 *   static void test_otelc_deinit_reinit(const char *cfg_file)
 *
 * ARGUMENTS
 *   cfg_file - path to the YAML configuration file
 *
 * DESCRIPTION
 *   Verifies that otelc_deinit() followed by otelc_init() succeeds, confirming
 *   that the library can be reinitialized after shutdown.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_otelc_deinit_reinit(const char *cfg_file)
{
	char *err = nullptr;
	int   result = TEST_FAIL;

	otelc_deinit(nullptr, nullptr, nullptr);

	if (otelc_init(cfg_file, &err) == OTELC_RET_OK)
		result = TEST_PASS;

	OTELC_SFREE(err);

	test_report("otelc_deinit + reinit", result);
}


/***
 * NAME
 *   main - program entry point
 *
 * SYNOPSIS
 *   int main(int argc, char **argv)
 *
 * ARGUMENTS
 *   argc - number of command-line arguments
 *   argv - array of command-line argument strings
 *
 * DESCRIPTION
 *   Creates a temporary YAML file, opens the YAML documents, runs all YAML
 *   parsing tests, and reports the results.
 *
 * RETURN VALUE
 *   Returns EX_OK if all tests pass, or EX_SOFTWARE if any test fails.
 */
int main(int argc, char **argv)
{
	OTEL_YAML_DOC *doc = nullptr;
	const char    *cfg_file;
	char          *otel_err = nullptr, temp_path[PATH_MAX];
	int            retval;

	retval = test_init(argc, argv, "YAML tests", &cfg_file);
	if (retval >= 0)
		return retval;

	retval = EX_OK;

	/***
	 * Build the temporary YAML file path relative to the config file.
	 */
	{
		char *cfg_copy = strdup(cfg_file);

		if (_NULL(cfg_copy)) {
			OTELC_LOG(stderr, "ERROR: strdup() failed");
			return EX_OSERR;
		}

		(void)snprintf(temp_path, sizeof(temp_path), "%s/%s", dirname(cfg_copy), TEMP_YAML_FILE);
		OTELC_SFREE(cfg_copy);
	}

	if (write_temp_yaml(temp_path) == -1) {
		OTELC_LOG(stderr, "ERROR: Unable to create temporary YAML file '%s'", temp_path);

		return EX_CANTCREAT;
	}

	OTELC_LOG(stdout, "Temp:   %s", temp_path);
	OTELC_LOG(stdout, "");

	/***
	 * yaml_open / yaml_close tests.
	 */
	OTELC_LOG(stdout, "[yaml_open / yaml_close]");
	test_yaml_open_valid(cfg_file);
	test_yaml_open_null();
	test_yaml_open_nonexistent();
	test_yaml_close_valid(cfg_file);
	test_yaml_close_null();

	/***
	 * yaml_read tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_read]");
	test_yaml_read_valid(cfg_file);
	test_yaml_read_null();
	test_yaml_read_nonexistent();

	/***
	 * Open the temporary YAML file for structured tests.
	 */
	doc = yaml_open(temp_path, &otel_err);
	if (_NULL(doc)) {
		OTELC_LOG(stderr, "ERROR: %s", _NULL(otel_err) ? "Unable to open temp YAML" : otel_err);
		(void)unlink(temp_path);

		return test_done(EX_SOFTWARE, otel_err);
	}

	/***
	 * yaml_find tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_find]");
	test_yaml_find_existing(doc);
	test_yaml_find_nonexistent_mandatory(doc);
	test_yaml_find_nonexistent_optional(doc);
	test_yaml_find_null_doc();
	test_yaml_find_null_path(doc);

	/***
	 * yaml_is_sequence tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_is_sequence]");
	test_yaml_is_sequence_true(doc);
	test_yaml_is_sequence_false(doc);
	test_yaml_is_sequence_nonexistent(doc);

	/***
	 * yaml_get_sequence_len tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_get_sequence_len]");
	test_yaml_get_sequence_len_valid(doc);
	test_yaml_get_sequence_len_nonseq(doc);

	/***
	 * yaml_get_sequence_value tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_get_sequence_value]");
	test_yaml_get_sequence_value_valid(doc);
	test_yaml_get_sequence_value_oob(doc);
	test_yaml_get_sequence_value_negative(doc);

	/***
	 * yaml_get_sequence tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_get_sequence]");
	test_yaml_get_sequence_valid(doc);

	/***
	 * yaml_find_sequence tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_find_sequence]");
	test_yaml_find_sequence_valid(doc);

	/***
	 * yaml_get_node tests (name-based).
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_get_node (name)]");
	test_yaml_get_node_from_name_str(doc);
	test_yaml_get_node_from_name_bool(doc);
	test_yaml_get_node_from_name_int64(doc);
	test_yaml_get_node_from_name_double(doc);
	test_yaml_get_node_from_name_multi(doc);
	test_yaml_get_node_mandatory_missing(doc);
	test_yaml_get_node_optional_missing(doc);
	test_yaml_get_node_invalid_bool(doc);
	test_yaml_get_node_int64_out_of_range(doc);
	test_yaml_get_node_double_out_of_range(doc);

	/***
	 * yaml_get_node tests (path-based).
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[yaml_get_node (path)]");
	test_yaml_get_node_str(doc);

	yaml_close(&doc);

	/***
	 * otelc_init / otelc_deinit tests.
	 */
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[otelc_init / otelc_deinit]");
	test_otelc_init_valid(cfg_file);
	test_otelc_init_null();
	test_otelc_init_double();
	test_otelc_deinit_reinit(cfg_file);

	(void)unlink(temp_path);

	return test_done(retval, otel_err);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
