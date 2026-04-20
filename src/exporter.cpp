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


static std::ofstream otel_tracer_logfile{}, otel_meter_logfile{}, otel_logger_logfile{};


/***
 * NAME
 *   otel_exporter_logfile_close - closes an exporter log file stream
 *
 * SYNOPSIS
 *   static void otel_exporter_logfile_close(std::ofstream &logfile)
 *
 * ARGUMENTS
 *   logfile - reference to the output file stream to close
 *
 * DESCRIPTION
 *   Closes the given output file stream if it is currently open.  This is a
 *   shared helper used by the per-signal exporter destroy functions to avoid
 *   repeating the same open-check-and-close pattern.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otel_exporter_logfile_close(std::ofstream &logfile)
{
	if (logfile.is_open())
		logfile.close();
}


/***
 * NAME
 *   otel_exporter_set_otlp_file_options - populates OTLP File exporter options from YAML configuration
 *
 * SYNOPSIS
 *   template <typename T, typename R>
 *   static int otel_exporter_set_otlp_file_options(const char *desc, const char *path, T &options, R &rt_options, char **err, const char *name)
 *
 * ARGUMENTS
 *   desc       - description of the exporter
 *   path       - the YAML configuration path
 *   options    - exporter options structure to populate
 *   rt_options - runtime options structure to populate
 *   err        - address of a pointer to store an error message on failure
 *   name       - name of the exporter configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Reads and applies configuration values common to all OTLP File exporters
 *   from a YAML document and stores them in the supplied exporter options
 *   structure.  If a thread_name is specified in the YAML configuration, the
 *   function creates a thread instrumentation object and stores it in the
 *   runtime options.
 *
 *   The function is type-agnostic and relies on the options object exposing
 *   the expected OTLP File fields rather than on inheritance or runtime type
 *   checks.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
template <typename T, typename R>
static int otel_exporter_set_otlp_file_options(const char *desc, const char *path, T &options, R &rt_options, char **err, const char *name = nullptr)
{
	/***
	 * The default values are defined in the include file
	 * <opentelemetry/exporters/otlp/otlp_file_client_options.h>.
	 */
	otel_exporter_otlp::OtlpFileClientFileSystemOptions fs_options{};
	char                                                thread_name[OTEL_YAML_BUFSIZ] = "";
	char                                                file_pattern[OTEL_YAML_BUFSIZ] = OTEL_EXPORTER_OTLP_FILE_PATTERN, alias_pattern[OTEL_YAML_BUFSIZ] = "";
	int64_t                                             flush_interval = 30000000, flush_count = 256, file_size = 1024 * 1024 * 20, rotate_size = 3;
	int64_t                                             cpu_id = -1;
	int                                                 rc;

	OTELC_FUNC("\"%s\", \"%s\", <options>, %p:%p, \"%s\"", OTELC_STR_ARG(desc), OTELC_STR_ARG(path), OTELC_DPTR_ARGS(err), OTELC_STR_ARG(name));

	if (OTEL_NULL(desc))
		OTEL_ERR_RETURN_INT("Exporter description not specified");
	else if (OTEL_NULL(path))
		OTEL_ERR_RETURN_INT("Exporter path not specified");

	rc = yaml_get_node(otelc_fyd, err, 1, desc, path, name,
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, thread_name),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, cpu_id, -1, OTEL_MAX_CPU_ID),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, file_pattern),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, alias_pattern),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, flush_interval, 100000, 60000000),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, flush_count, 16, 8192),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, file_size, 1024 * 64, INT64_C(1024) * 1024 * 1024 * 2),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, rotate_size, 1, 256),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);
	else if (rc == 0) {
		if (OTEL_NULL(name))
			OTEL_ERR_RETURN_INT("OpenTelemetry exporter type not specified");
		else
			OTEL_ERR_RETURN_INT("'%s': OpenTelemetry exporter type not specified", name);
	}

	fs_options.file_pattern   = file_pattern;
	fs_options.alias_pattern  = alias_pattern;
	fs_options.flush_interval = std::chrono::microseconds(flush_interval);
	fs_options.flush_count    = flush_count;
	fs_options.file_size      = file_size;
	fs_options.rotate_size    = rotate_size;
	options.backend_options   = fs_options;

	if (*thread_name != '\0') {
		const auto thread_instrumentation = otel::make_shared_nothrow<otel_thread_instrumentation>(thread_name, OTEL_CAST_STATIC(int, cpu_id));
		if (!OTEL_NULL(thread_instrumentation))
			rt_options.thread_instrumentation = thread_instrumentation;
	}

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_exporter_set_otlp_grpc_options - populates OTLP gRPC exporter options from YAML configuration
 *
 * SYNOPSIS
 *   template <typename T>
 *   static int otel_exporter_set_otlp_grpc_options(const char *desc, const char *path, char *endpoint, T &options, char **err, const char *name)
 *
 * ARGUMENTS
 *   desc     - description of the exporter
 *   path     - the YAML configuration path
 *   endpoint - the endpoint to connect to
 *   options  - exporter options structure to populate
 *   err      - address of a pointer to store an error message on failure
 *   name     - name of the exporter configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Reads and applies configuration values common to all OTLP gRPC exporters
 *   from a YAML document and stores them in the supplied exporter options
 *   structure.  This includes the exporter endpoint and other gRPC-related
 *   settings shared by trace, metric and logs exporters.
 *
 *   The thread_name setting is read from YAML for configuration consistency
 *   but is not used because the OpenTelemetry SDK does not provide runtime
 *   options for gRPC exporters.
 *
 *   The function is type-agnostic and relies on the options object exposing
 *   the expected OTLP gRPC fields rather than on inheritance or runtime type
 *   checks.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
template <typename T>
static int otel_exporter_set_otlp_grpc_options(const char *desc, const char *path, char *endpoint, T &options, char **err, const char *name = nullptr)
{
	char    thread_name[OTEL_YAML_BUFSIZ] = "";
	char    ssl_credentials_cacert_path[OTEL_YAML_BUFSIZ] = "", ssl_credentials_cacert_as_string[OTEL_YAML_BUFSIZ] = "", user_agent[OTEL_YAML_BUFSIZ] = "", compression[OTEL_YAML_BUFSIZ] = "";
#ifdef ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW
	char    ssl_client_key_path[OTEL_YAML_BUFSIZ] = "", ssl_client_key_string[OTEL_YAML_BUFSIZ] = "";
	char    ssl_client_cert_path[OTEL_YAML_BUFSIZ] = "", ssl_client_cert_string[OTEL_YAML_BUFSIZ] = "";
#endif
	int     rc, use_ssl_credentials = false;
	int64_t timeout = 10, max_threads = 0, max_concurrent_requests = 0;
	int64_t cpu_id = -1;

	OTELC_FUNC("\"%s\", \"%s\", \"%s\", <options>, %p:%p, \"%s\"", OTELC_STR_ARG(desc), OTELC_STR_ARG(path), OTELC_STR_ARG(endpoint), OTELC_DPTR_ARGS(err), OTELC_STR_ARG(name));

	if (OTEL_NULL(desc))
		OTEL_ERR_RETURN_INT("Exporter description not specified");
	else if (OTEL_NULL(path))
		OTEL_ERR_RETURN_INT("Exporter path not specified");
	else if (OTEL_NULL(endpoint))
		OTEL_ERR_RETURN_INT("Exporter endpoint not specified");

	rc = yaml_get_node(otelc_fyd, err, 1, desc, path, name,
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, thread_name),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, cpu_id, -1, OTEL_MAX_CPU_ID),
	                   OTEL_YAML_ARG_STR_PTR(0, EXPORTERS, endpoint, OTEL_YAML_BUFSIZ),
	                   OTEL_YAML_ARG_BOOL(0, EXPORTERS, use_ssl_credentials),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_credentials_cacert_path),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_credentials_cacert_as_string),
#ifdef ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_client_key_path),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_client_key_string),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_client_cert_path),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_client_cert_string),
#endif
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, timeout, 1, 60),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, user_agent),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, max_threads, 1, 1024),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, compression),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, max_concurrent_requests, 1, 1024),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	options.endpoint                         = endpoint;
	options.use_ssl_credentials              = use_ssl_credentials;
	options.ssl_credentials_cacert_path      = ssl_credentials_cacert_path;
	options.ssl_credentials_cacert_as_string = ssl_credentials_cacert_as_string;
#ifdef ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW
	options.ssl_client_key_path              = ssl_client_key_path;
	options.ssl_client_key_string            = ssl_client_key_string;
	options.ssl_client_cert_path             = ssl_client_cert_path;
	options.ssl_client_cert_string           = ssl_client_cert_string;
#endif
	options.timeout                          = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds{timeout});
	options.user_agent                       = user_agent;
	options.max_threads                      = max_threads;
	options.compression                      = compression;
	options.max_concurrent_requests          = max_concurrent_requests;

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_exporter_set_otlp_http_options - populates OTLP HTTP exporter options from YAML configuration
 *
 * SYNOPSIS
 *   template <typename T, typename R>
 *   static int otel_exporter_set_otlp_http_options(const char *desc, const char *path, char *endpoint, T &options, R &rt_options, char **err, const char *name)
 *
 * ARGUMENTS
 *   desc       - description of the exporter
 *   path       - the YAML configuration path
 *   endpoint   - the endpoint to connect to
 *   options    - exporter options structure to populate
 *   rt_options - runtime options structure to populate
 *   err        - address of a pointer to store an error message on failure
 *   name       - name of the exporter configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Reads and applies configuration values common to all OTLP HTTP exporters
 *   from a YAML document and stores them in the supplied exporter options
 *   structure.  This includes the exporter endpoint and other HTTP-related
 *   settings shared by trace, metric and logs exporters.  If a thread_name
 *   is specified in the YAML configuration, the function creates a thread
 *   instrumentation object and stores it in the runtime options.
 *
 *   The function is type-agnostic and relies on the options object exposing
 *   the expected OTLP HTTP fields rather than on inheritance or runtime type
 *   checks.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
template <typename T, typename R>
static int otel_exporter_set_otlp_http_options(const char *desc, const char *path, char *endpoint, T &options, R &rt_options, int64_t *thread_wait_time, char **err, const char *name = nullptr)
{
	/***
	 * The default values are defined in the include file
	 * <opentelemetry/exporters/otlp/otlp_http_client.h>.
	 */
	otel_exporter_otlp::OtlpHeaders  otlp_http_headers{};
	struct otelc_text_map           *http_headers = nullptr;
	char                             thread_name[OTEL_YAML_BUFSIZ] = "";
	char                             content_type[OTEL_YAML_BUFSIZ] = "json", json_bytes_mapping[OTEL_YAML_BUFSIZ] = "hexid";
	char                             ssl_ca_cert_path[OTEL_YAML_BUFSIZ] = "", ssl_ca_cert_string[OTEL_YAML_BUFSIZ] = "";
	char                             ssl_client_key_path[OTEL_YAML_BUFSIZ] = "", ssl_client_key_string[OTEL_YAML_BUFSIZ] = "";
	char                             ssl_client_cert_path[OTEL_YAML_BUFSIZ] = "", ssl_client_cert_string[OTEL_YAML_BUFSIZ] = "";
	char                             ssl_min_tls[OTEL_YAML_BUFSIZ] = "", ssl_max_tls[OTEL_YAML_BUFSIZ] = "";
	char                             ssl_cipher[OTEL_YAML_BUFSIZ] = "", ssl_cipher_suite[OTEL_YAML_BUFSIZ] = "", compression[OTEL_YAML_BUFSIZ] = "";
	int                              rc, use_json_name = false, debug = false, ssl_insecure_skip_verify = false;
	int64_t                          timeout = 10, max_concurrent_requests = 64, max_requests_per_connection = 8, background_thread_wait_for = 0, cpu_id = -1;

	OTELC_FUNC("\"%s\", \"%s\", \"%s\", <options>, %p, %p:%p, \"%s\"", OTELC_STR_ARG(desc), OTELC_STR_ARG(path), OTELC_STR_ARG(endpoint), thread_wait_time, OTELC_DPTR_ARGS(err), OTELC_STR_ARG(name));

	if (OTEL_NULL(desc))
		OTEL_ERR_RETURN_INT("Exporter description not specified");
	else if (OTEL_NULL(path))
		OTEL_ERR_RETURN_INT("Exporter path not specified");
	else if (OTEL_NULL(endpoint))
		OTEL_ERR_RETURN_INT("Exporter endpoint not specified");

	rc = yaml_get_node(otelc_fyd, err, 1, desc, path, name,
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, thread_name),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, cpu_id, -1, OTEL_MAX_CPU_ID),
	                   OTEL_YAML_ARG_STR_PTR(0, EXPORTERS, endpoint, OTEL_YAML_BUFSIZ),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, content_type),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, json_bytes_mapping),
	                   OTEL_YAML_ARG_BOOL(0, EXPORTERS, use_json_name),
	                   OTEL_YAML_ARG_BOOL(0, EXPORTERS, debug),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, timeout, 1, 60),
	                   OTEL_YAML_ARG_MAP(0, EXPORTERS, http_headers),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, max_concurrent_requests, 1, 1024),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, max_requests_per_connection, 1, 1024),
	                   OTEL_YAML_ARG_INT64(0, EXPORTERS, background_thread_wait_for, 0, 3600000),
	                   OTEL_YAML_ARG_BOOL(0, EXPORTERS, ssl_insecure_skip_verify),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_ca_cert_path),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_ca_cert_string),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_client_key_path),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_client_key_string),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_client_cert_path),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_client_cert_string),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_min_tls),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_max_tls),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_cipher),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, ssl_cipher_suite),
	                   OTEL_YAML_ARG_STR(0, EXPORTERS, compression),
	                   OTEL_YAML_END);
	OTEL_DEFER_DPTR_FREE(struct otelc_text_map, http_headers, otelc_text_map_destroy);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	/* <opentelemetry/exporters/otlp/otlp_http.h> */
	if (strcasecmp(content_type, "json") == 0)
		options.content_type = otel_exporter_otlp::HttpRequestContentType::kJson;
	else if (strcasecmp(content_type, "binary") == 0)
		options.content_type = otel_exporter_otlp::HttpRequestContentType::kBinary;
	else if (*content_type != '\0')
		OTEL_ERR_RETURN_INT("Invalid content_type: '%s'", content_type);

	/* <opentelemetry/exporters/otlp/otlp_http.h> */
	if (strcasecmp(json_bytes_mapping, "hexid") == 0)
		options.json_bytes_mapping = otel_exporter_otlp::JsonBytesMappingKind::kHexId;
	else if (strcasecmp(json_bytes_mapping, "hex") == 0)
		options.json_bytes_mapping = otel_exporter_otlp::JsonBytesMappingKind::kHex;
	else if (strcasecmp(json_bytes_mapping, "base64") == 0)
		options.json_bytes_mapping = otel_exporter_otlp::JsonBytesMappingKind::kBase64;
	else if (*json_bytes_mapping != '\0')
		OTEL_ERR_RETURN_INT("Invalid json_bytes_mapping: '%s'", json_bytes_mapping);

	if (!OTEL_NULL(http_headers))
		for (size_t i = 0; i < http_headers->count; i++) {
			try {
				OTEL_DBG_THROW();
				otlp_http_headers.emplace(std::string{http_headers->key[i]}, std::string{http_headers->value[i]});
			}
			OTEL_CATCH_SIGNAL_RETURN( , OTEL_ERR_RETURN_INT, "Unable to add HTTP header")
		}

	options.url                         = endpoint;
	options.use_json_name               = use_json_name;
	options.console_debug               = debug;
	options.timeout                     = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds{timeout});
	options.http_headers                = std::move(otlp_http_headers);
	options.max_concurrent_requests     = max_concurrent_requests;
	options.max_requests_per_connection = max_requests_per_connection;
	options.ssl_insecure_skip_verify    = ssl_insecure_skip_verify;
	options.ssl_ca_cert_path            = ssl_ca_cert_path;
	options.ssl_ca_cert_string          = ssl_ca_cert_string;
	options.ssl_client_key_path         = ssl_client_key_path;
	options.ssl_client_key_string       = ssl_client_key_string;
	options.ssl_client_cert_path        = ssl_client_cert_path;
	options.ssl_client_cert_string      = ssl_client_cert_string;
	options.ssl_min_tls                 = ssl_min_tls;
	options.ssl_max_tls                 = ssl_max_tls;
	options.ssl_cipher                  = ssl_cipher;
	options.ssl_cipher_suite            = ssl_cipher_suite;
	options.compression                 = compression;

	if (options.console_debug)
		otel_sdk_internal_log::GlobalLogHandler::SetLogLevel(otel_sdk_internal_log::LogLevel::Debug);

	if (*thread_name != '\0') {
		const auto thread_instrumentation = otel::make_shared_nothrow<otel_thread_instrumentation>(thread_name, OTEL_CAST_STATIC(int, cpu_id));
		if (!OTEL_NULL(thread_instrumentation))
			rt_options.thread_instrumentation = thread_instrumentation;
	}

	if (!OTEL_NULL(thread_wait_time))
		*thread_wait_time = background_thread_wait_for;

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_exporter_set_ostream_options - populates ostream exporter options from YAML configuration
 *
 * SYNOPSIS
 *   template <typename C, typename T>
 *   static int otel_exporter_set_ostream_options(const char *desc, const char *path, std::ofstream &stream, T &exporter, char **err, const char *name)
 *
 * ARGUMENTS
 *   desc     - description of the exporter
 *   path     - the YAML configuration path
 *   stream   - output file stream
 *   exporter - reference to a unique pointer where the created exporter is stored
 *   err      - address of a pointer to store an error message on failure
 *   name     - name of the exporter configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Reads and applies configuration values for an ostream exporter from a YAML
 *   document, opens the specified output stream, and creates the exporter.
 *   The created exporter is returned via the exporter parameter.
 *
 *   The function is type-agnostic and relies on the template parameter to
 *   determine the concrete exporter type to instantiate.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
template <typename C, typename T>
static int otel_exporter_set_ostream_options(const char *desc, const char *path, std::ofstream &stream, T &exporter, char **err, const char *name = nullptr)
{
	char filename[PATH_MAX] = OTEL_EXPORTER_OSTREAM_STDOUT;
	int  rc;

	OTELC_FUNC("\"%s\", \"%s\", <stream>, <exporter>, %p:%p, \"%s\"", OTELC_STR_ARG(desc), OTELC_STR_ARG(path), OTELC_DPTR_ARGS(err), OTELC_STR_ARG(name));

	if (OTEL_NULL(desc))
		OTEL_ERR_RETURN_INT("Exporter description not specified");
	else if (OTEL_NULL(path))
		OTEL_ERR_RETURN_INT("Exporter path not specified");

	rc = yaml_get_node(otelc_fyd, err, 1, desc, path, name, OTEL_YAML_ARG_STR(0, EXPORTERS, filename), OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (strcasecmp(filename, OTEL_EXPORTER_OSTREAM_STDOUT) == 0) {
		exporter = otel::make_unique_nothrow<C>(std::cout);
	}
	else if (strcasecmp(filename, OTEL_EXPORTER_OSTREAM_STDERR) == 0) {
		exporter = otel::make_unique_nothrow<C>(std::cerr);
	}
	else {
		stream.open(filename, std::ios::out);
		if (stream.fail())
			OTEL_ERR_RETURN_INT("'%s': %s", filename, otel_strerror(errno));
		else
			exporter = otel::make_unique_nothrow<C>(stream);
	}

	if (OTEL_NULL(exporter)) {
		if (stream.is_open())
			stream.close();

		OTEL_ERR_RETURN_INT("Unable to create ostream exporter");
	}

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_tracer_exporter_create - creates a new span exporter
 *
 * SYNOPSIS
 *   int otel_tracer_exporter_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter, const char *name)
 *
 * ARGUMENTS
 *   tracer   - tracer instance
 *   exporter - unique pointer to store the created span exporter
 *   name     - name of the exporter configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Creates a new span exporter based on the configuration provided in the YAML
 *   file.  The exporter is responsible for sending trace data to a backend,
 *   such as Zipkin or an OTLP-compatible collector.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_tracer_exporter_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanExporter> &exporter, const char *name)
{
	std::unique_ptr<otel_sdk_trace::SpanExporter> exporter_maybe{};
	char                                          type[OTEL_YAML_BUFSIZ] = "";
	int                                           rc;

	OTELC_FUNC("%p, <exporter>, \"%s\"", tracer, OTELC_STR_ARG(name));

	if (OTEL_NULL(tracer))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	rc = yaml_get_node(otelc_fyd, &(tracer->err), 1, OTEL_TRACER_EXPORTER_DESC, OTEL_YAML_TRACER_PREFIX OTEL_YAML_EXPORTERS, name,
	                   OTEL_YAML_ARG_STR(1, EXPORTERS, type),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (strcasecmp(type, OTEL_EXPORTER_ELASTICSEARCH) == 0) {
		OTEL_TRACER_ERROR(OTEL_TRACER_EXPORTER_NOT_SUPPORTED("Elasticsearch"));
	}
	else if (strcasecmp(type, OTEL_EXPORTER_IN_MEMORY) == 0) {
#ifdef HAVE_OTEL_EXPORTER_IN_MEMORY
		/* <opentelemetry/exporters/memory/in_memory_span_exporter.h> */
		int64_t buffer_size = otel_exporter_memory::MAX_BUFFER_SIZE;

		rc = yaml_get_node(otelc_fyd, &(tracer->err), 1, OTEL_TRACER_EXPORTER_DESC, OTEL_YAML_TRACER_PREFIX OTEL_YAML_EXPORTERS, name,
		                   OTEL_YAML_ARG_INT64(0, EXPORTERS, buffer_size, 16, 65536),
		                   OTEL_YAML_END);
		if (rc == OTELC_RET_ERROR)
			OTELC_RETURN_INT(OTELC_RET_ERROR);

		exporter_maybe = otel::make_unique_nothrow<otel_exporter_memory::InMemorySpanExporter>(buffer_size);
		if (OTEL_NULL(exporter_maybe))
			OTEL_TRACER_ERROR(OTEL_TRACER_EXPORTER_FAILED("In-Memory"));
#else
		OTEL_TRACER_ERROR(OTEL_TRACER_EXPORTER_NOT_SUPPORTED("In-Memory"));
#endif /* HAVE_OTEL_EXPORTER_IN_MEMORY */
	}
	OTEL_EXPORTER_CASE_OSTREAM(TRACER, otel_exporter_trace::OStreamSpanExporter, otel_tracer_logfile, &(tracer->err))
	OTEL_EXPORTER_CASE_OTLP_FILE(TRACER, OtlpFileExporter, &(tracer->err))
	OTEL_EXPORTER_CASE_OTLP_GRPC(TRACER, OtlpGrpcExporter, &(tracer->err))
	OTEL_EXPORTER_CASE_OTLP_HTTP(TRACER, OtlpHttpExporter, &(tracer->err))
	else if (strcasecmp(type, OTEL_EXPORTER_ZIPKIN) == 0) {
#ifdef HAVE_OTEL_EXPORTER_ZIPKIN
		otel_exporter_zipkin::ZipkinExporterOptions options{};
		char                                        endpoint[OTEL_YAML_BUFSIZ] = OTEL_TRACER_EXPORTER_ZIPKIN_ENDPOINT, format[OTEL_YAML_BUFSIZ] = "", service_name[OTEL_YAML_BUFSIZ] = "default-service";
		char                                        ipv4[OTEL_YAML_BUFSIZ] = "", ipv6[OTEL_YAML_BUFSIZ] = "";

		rc = yaml_get_node(otelc_fyd, &(tracer->err), 1, OTEL_TRACER_EXPORTER_DESC, OTEL_YAML_TRACER_PREFIX OTEL_YAML_EXPORTERS, name,
		                   OTEL_YAML_ARG_STR(0, EXPORTERS, endpoint),
		                   OTEL_YAML_ARG_STR(0, EXPORTERS, format),
		                   OTEL_YAML_ARG_STR(0, EXPORTERS, service_name),
		                   OTEL_YAML_ARG_STR(0, EXPORTERS, ipv4),
		                   OTEL_YAML_ARG_STR(0, EXPORTERS, ipv6),
		                   OTEL_YAML_END);
		if (rc == OTELC_RET_ERROR)
			OTELC_RETURN_INT(OTELC_RET_ERROR);

		if (strcasecmp(format, "json") == 0)
			options.format = otel_exporter_zipkin::TransportFormat::kJson;
		else if (strcasecmp(format, "protobuf") == 0)
			options.format = otel_exporter_zipkin::TransportFormat::kProtobuf;
		else if (*format != '\0')
			OTEL_TRACER_RETURN_INT("Invalid Zipkin exporter format: '%s'", format);

		options.endpoint     = endpoint;
		options.service_name = service_name;
		options.ipv4         = ipv4;
		options.ipv6         = ipv6;

		exporter_maybe = otel::make_unique_nothrow<otel_exporter_zipkin::ZipkinExporter>(options);
		if (OTEL_NULL(exporter_maybe))
			OTEL_TRACER_ERROR(OTEL_TRACER_EXPORTER_FAILED("Zipkin"));
#else
		OTEL_TRACER_ERROR(OTEL_TRACER_EXPORTER_NOT_SUPPORTED("Zipkin"));
#endif /* HAVE_OTEL_EXPORTER_ZIPKIN */
	}
	else {
		OTEL_TRACER_ERROR("Invalid exporter type: '%s'", type);
	}

	if (OTEL_NULL(exporter_maybe))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	exporter = std::move(exporter_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_tracer_exporter_destroy - destroys the tracer exporter
 *
 * SYNOPSIS
 *   void otel_tracer_exporter_destroy(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Cleans up resources associated with the tracer exporter, such as closing
 *   any open file streams.  This should be called during shutdown to ensure a
 *   clean exit.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_tracer_exporter_destroy(void)
{
	OTELC_FUNC("");

	otel_exporter_logfile_close(otel_tracer_logfile);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_meter_exporter_create - creates a new metric exporter
 *
 * SYNOPSIS
 *   int otel_meter_exporter_create(struct otelc_meter *meter, std::unique_ptr<otel_sdk_metrics::PushMetricExporter> &exporter, const char *name)
 *
 * ARGUMENTS
 *   meter    - meter instance
 *   exporter - unique pointer to store the created metric exporter
 *   name     - name of the exporter configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Creates a new metric exporter based on the configuration provided in the
 *   YAML file.  The exporter is responsible for sending metric data to a
 *   backend, such as an OTLP-compatible collector.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_meter_exporter_create(struct otelc_meter *meter, std::unique_ptr<otel_sdk_metrics::PushMetricExporter> &exporter, const char *name)
{
	std::unique_ptr<otel_sdk_metrics::PushMetricExporter> exporter_maybe{};
	char                                                  type[OTEL_YAML_BUFSIZ] = "";
	int                                                   rc;

	OTELC_FUNC("%p, <exporter>, \"%s\"", meter, OTELC_STR_ARG(name));

	if (OTEL_NULL(meter))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	rc = yaml_get_node(otelc_fyd, &(meter->err), 1, OTEL_METER_EXPORTER_DESC, OTEL_YAML_METER_PREFIX OTEL_YAML_EXPORTERS, name,
	                   OTEL_YAML_ARG_STR(1, EXPORTERS, type),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (strcasecmp(type, OTEL_EXPORTER_ELASTICSEARCH) == 0) {
		OTEL_METER_ERROR(OTEL_METER_EXPORTER_NOT_SUPPORTED("Elasticsearch"));
	}
	else if (strcasecmp(type, OTEL_EXPORTER_IN_MEMORY) == 0) {
#ifdef HAVE_OTEL_EXPORTER_IN_MEMORY
		/* <opentelemetry/exporters/memory/in_memory_metric_data.h> */
		int64_t buffer_size = otel_exporter_memory::MAX_BUFFER_SIZE;

		rc = yaml_get_node(otelc_fyd, &(meter->err), 1, OTEL_METER_EXPORTER_DESC, OTEL_YAML_METER_PREFIX OTEL_YAML_EXPORTERS, name,
		                   OTEL_YAML_ARG_INT64(0, EXPORTERS, buffer_size, 16, 65536),
		                   OTEL_YAML_END);
		if (rc == OTELC_RET_ERROR)
			OTELC_RETURN_INT(OTELC_RET_ERROR);

//		const auto data = std::shared_ptr<otel_exporter_memory::InMemoryMetricData>{new(std::nothrow) otel_exporter_memory::SimpleAggregateInMemoryMetricData()};
		const auto data = otel::make_shared_nothrow<otel_exporter_memory::CircularBufferInMemoryMetricData>(buffer_size);
		if (!OTEL_NULL(data))
			exporter_maybe = otel_exporter_memory::InMemoryMetricExporterFactory::Create(data);
		if (OTEL_NULL(exporter_maybe))
			OTEL_METER_ERROR(OTEL_METER_EXPORTER_FAILED("In-Memory"));
#else
		OTEL_METER_ERROR(OTEL_METER_EXPORTER_NOT_SUPPORTED("In-Memory"));
#endif /* HAVE_OTEL_EXPORTER_IN_MEMORY */
	}
	OTEL_EXPORTER_CASE_OSTREAM(METER, otel_exporter_metrics::OStreamMetricExporter, otel_meter_logfile, &(meter->err))
	OTEL_EXPORTER_CASE_OTLP_FILE(METER, OtlpFileMetricExporter, &(meter->err))
	OTEL_EXPORTER_CASE_OTLP_GRPC(METER, OtlpGrpcMetricExporter, &(meter->err))
	OTEL_EXPORTER_CASE_OTLP_HTTP(METER, OtlpHttpMetricExporter, &(meter->err))
	else if (strcasecmp(type, OTEL_EXPORTER_ZIPKIN) == 0) {
		OTEL_METER_ERROR(OTEL_METER_EXPORTER_NOT_SUPPORTED("Zipkin"));
	}
	else {
		OTEL_METER_ERROR("Invalid exporter type: '%s'", type);
	}

	if (OTEL_NULL(exporter_maybe))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	exporter = std::move(exporter_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_meter_exporter_destroy - destroys the meter exporter
 *
 * SYNOPSIS
 *   void otel_meter_exporter_destroy(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Cleans up resources associated with the meter exporter, such as closing
 *   any open file streams.  This should be called during shutdown to ensure a
 *   clean exit.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_meter_exporter_destroy(void)
{
	OTELC_FUNC("");

	otel_exporter_logfile_close(otel_meter_logfile);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_logger_exporter_create - creates a new log record exporter
 *
 * SYNOPSIS
 *   int otel_logger_exporter_create(struct otelc_logger *logger, std::unique_ptr<otel_sdk_logs::LogRecordExporter> &exporter, const char *name)
 *
 * ARGUMENTS
 *   logger   - logger instance
 *   exporter - unique pointer to store the created log record exporter
 *   name     - name of the exporter configuration node, or nullptr for default
 *
 * DESCRIPTION
 *   Creates a new log record exporter based on the configuration provided in
 *   the YAML file.  The exporter is responsible for sending log data to a
 *   backend, such as an OTLP-compatible collector.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_logger_exporter_create(struct otelc_logger *logger, std::unique_ptr<otel_sdk_logs::LogRecordExporter> &exporter, const char *name)
{
	std::unique_ptr<otel_sdk_logs::LogRecordExporter> exporter_maybe{};
	char                                              type[OTEL_YAML_BUFSIZ] = "";
	int                                               rc;

	OTELC_FUNC("%p, <exporter>, \"%s\"", logger, OTELC_STR_ARG(name));

	if (OTEL_NULL(logger))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	rc = yaml_get_node(otelc_fyd, &(logger->err), 1, OTEL_LOGGER_EXPORTER_DESC, OTEL_YAML_LOGGER_PREFIX OTEL_YAML_EXPORTERS, name,
	                   OTEL_YAML_ARG_STR(1, EXPORTERS, type),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (strcasecmp(type, OTEL_EXPORTER_ELASTICSEARCH) == 0) {
#ifdef HAVE_OTEL_EXPORTER_ELASTICSEARCH
		/***
		 * The default values are defined in the include file
		 * <opentelemetry/exporters/elasticsearch/es_log_record_exporter.h>.
		 */
		otel_exporter_logs::ElasticsearchExporterOptions  options{};
		std::multimap<std::string, std::string>           es_http_headers{};
		struct otelc_text_map                            *http_headers = nullptr;
		char                                              host[OTEL_YAML_BUFSIZ] = "localhost", index[OTEL_YAML_BUFSIZ] = "logs";
		int64_t                                           port = 9200, response_timeout = 30;
		int                                               debug = false;

		rc = yaml_get_node(otelc_fyd, &(logger->err), 1, OTEL_LOGGER_EXPORTER_DESC, OTEL_YAML_LOGGER_PREFIX OTEL_YAML_EXPORTERS, name,
		                   OTEL_YAML_ARG_STR(0, EXPORTERS, host),
		                   OTEL_YAML_ARG_INT64(0, EXPORTERS, port, 1, 65535),
		                   OTEL_YAML_ARG_STR(0, EXPORTERS, index),
		                   OTEL_YAML_ARG_INT64(0, EXPORTERS, response_timeout, 1, 3600),
		                   OTEL_YAML_ARG_BOOL(0, EXPORTERS, debug),
		                   OTEL_YAML_ARG_MAP(0, EXPORTERS, http_headers),
		                   OTEL_YAML_END);
		OTEL_DEFER_DPTR_FREE(struct otelc_text_map, http_headers, otelc_text_map_destroy);
		if (rc == OTELC_RET_ERROR)
			OTELC_RETURN_INT(OTELC_RET_ERROR);

		if (!OTEL_NULL(http_headers))
			for (size_t i = 0; i < http_headers->count; i++) {
				try {
					OTEL_DBG_THROW();
					es_http_headers.emplace(std::string{http_headers->key[i]}, std::string{http_headers->value[i]});
				}
				OTEL_CATCH_SIGNAL_RETURN( , OTEL_LOGGER_RETURN_INT, "Unable to add HTTP header")
			}

		options.host_             = host;
		options.port_             = port;
		options.index_            = index;
		options.response_timeout_ = response_timeout;
		options.console_debug_    = debug;
		options.http_headers_     = std::move(es_http_headers);

		exporter_maybe = otel::make_unique_nothrow<otel_exporter_logs::ElasticsearchLogRecordExporter>(options);
		if (OTEL_NULL(exporter_maybe))
			OTEL_LOGGER_ERROR(OTEL_LOGGER_EXPORTER_FAILED("Elasticsearch"));
		else
			OTEL_CAST_STATIC(otel_exporter_logs::ElasticsearchLogRecordExporter *, exporter_maybe.get())->MaybeSpawnBackgroundThread();
#else
		OTEL_LOGGER_ERROR(OTEL_LOGGER_EXPORTER_NOT_SUPPORTED("Elasticsearch"));
#endif /* HAVE_OTEL_EXPORTER_ELASTICSEARCH */
	}
	else if (strcasecmp(type, OTEL_EXPORTER_IN_MEMORY) == 0) {
		OTEL_LOGGER_ERROR(OTEL_LOGGER_EXPORTER_NOT_SUPPORTED("In-Memory"));
	}
	OTEL_EXPORTER_CASE_OSTREAM(LOGGER, otel_exporter_logs::OStreamLogRecordExporter, otel_logger_logfile, &(logger->err))
	OTEL_EXPORTER_CASE_OTLP_FILE(LOGGER, OtlpFileLogRecordExporter, &(logger->err))
	OTEL_EXPORTER_CASE_OTLP_GRPC(LOGGER, OtlpGrpcLogRecordExporter, &(logger->err))
	OTEL_EXPORTER_CASE_OTLP_HTTP(LOGGER, OtlpHttpLogRecordExporter, &(logger->err))
	else if (strcasecmp(type, OTEL_EXPORTER_ZIPKIN) == 0) {
		OTEL_LOGGER_ERROR(OTEL_LOGGER_EXPORTER_NOT_SUPPORTED("Zipkin"));
	}
	else {
		OTEL_LOGGER_ERROR("Invalid exporter type: '%s'", type);
	}

	if (OTEL_NULL(exporter_maybe))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	exporter = std::move(exporter_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_logger_exporter_destroy - destroys the logger exporter
 *
 * SYNOPSIS
 *   void otel_logger_exporter_destroy(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Cleans up resources associated with the logger exporter, such as closing
 *   any open file streams.  This should be called during shutdown to ensure a
 *   clean exit.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_logger_exporter_destroy(void)
{
	OTELC_FUNC("");

	otel_exporter_logfile_close(otel_logger_logfile);

	OTELC_RETURN();
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
