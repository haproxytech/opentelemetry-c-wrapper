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
#include "test-util.h"


static struct otelc_tracer **registered_tracer = NULL;
static struct otelc_meter  **registered_meter  = NULL;
static struct otelc_logger **registered_logger = NULL;

int tests_run = 0, tests_passed = 0, tests_failed = 0;


/***
 * NAME
 *   test_report - reports a test result
 *
 * SYNOPSIS
 *   void test_report(const char *name, int result)
 *
 * ARGUMENTS
 *   name   - name of the test
 *   result - TEST_PASS or TEST_FAIL
 *
 * DESCRIPTION
 *   Reports the result of a test and updates the global counters.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void test_report(const char *name, int result)
{
	tests_run++;

	if (result == TEST_PASS) {
		tests_passed++;
		OTELC_LOG(stdout, "  PASS: %s", name);
	} else {
		tests_failed++;
		OTELC_LOG(stderr, "  FAIL: %s", name);
	}
}


/***
 * NAME
 *   test_usage - prints the program usage information
 *
 * SYNOPSIS
 *   void test_usage(const char *program_name)
 *
 * ARGUMENTS
 *   program_name - name of the program executable
 *
 * DESCRIPTION
 *   Outputs the command-line usage summary to the standard output.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void test_usage(const char *program_name)
{
	(void)printf("\nUsage: %s [OPTION]...\n\n", program_name);
	(void)printf("Options are:\n");
	(void)printf("  -c, --config=FILE   OpenTelemetry configuration file (default: %s).\n", DEFAULT_CFG_FILE);
	(void)printf("  -h, --help          Show this text.\n");
	(void)printf("  -V, --version       Show program version.\n\n");
}


/***
 * NAME
 *   test_init - initializes the test environment and parses arguments
 *
 * SYNOPSIS
 *   int test_init(int argc, char **argv, const char *banner, const char **cfg_file)
 *
 * ARGUMENTS
 *   argc     - number of command-line arguments
 *   argv     - array of command-line argument strings
 *   banner   - test suite name for the header banner
 *   cfg_file - pointer to store the resolved configuration file path
 *
 * DESCRIPTION
 *   Parses command-line options (-c, -h, -V), resolves the YAML
 *   configuration file path relative to the binary location, verifies
 *   the file exists, prints the banner, and initializes the
 *   OpenTelemetry runtime.  If -h or -V is given, the corresponding
 *   output is produced and a non-negative exit code is returned.
 *
 * RETURN VALUE
 *   Returns -1 on success (caller should continue), or a non-negative
 *   exit code if the program should terminate immediately.
 */
int test_init(int argc, char **argv, const char *banner, const char **cfg_file)
{
	static const struct option longopts[] = {
		{ "config",  required_argument, NULL, 'c' },
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'V' },
		{ NULL,      0,                 NULL, 0   }
	};
	static char  cfg_path[PATH_MAX];
	const char  *cfg = DEFAULT_CFG_FILE;
	int          c;

	(void)setvbuf(stdout, NULL, _IOLBF, 0);
	(void)setvbuf(stderr, NULL, _IOLBF, 0);

	while ((c = getopt_long(argc, argv, "c:hV", longopts, NULL)) != EOF) {
		if (c == 'c')
			cfg = optarg;
		else if (c == 'h') {
			test_usage(basename(argv[0]));

			return EX_OK;
		}
		else if (c == 'V') {
			(void)printf("%s v%s\n", basename(argv[0]), OTELC_PACKAGE_VERSION);

			return EX_OK;
		}
	}

	/***
	 * Resolve the config file path relative to the binary location,
	 * since the binary ends up in test/.libs during the build.
	 */
	if (strcmp(cfg, DEFAULT_CFG_FILE) == 0) {
		int rc = snprintf(cfg_path, sizeof(cfg_path), "%s/%s", dirname(argv[0]), cfg);
		if (OTELC_IN_RANGE(rc, 0, (int)(sizeof(cfg_path) - 1))) {
			if (access(cfg_path, F_OK) == -1) {
				char *path = strdup(cfg_path);

				if (_nNULL(path)) {
					rc = snprintf(cfg_path, sizeof(cfg_path), "%s/../%s", dirname(path), cfg);
					OTELC_SFREE(path);
				} else {
					rc = -1;
				}
			}

			if (OTELC_IN_RANGE(rc, 0, (int)(sizeof(cfg_path) - 1)))
				cfg = cfg_path;
		}
	}

	if (access(cfg, F_OK) == -1) {
		OTELC_LOG(stderr, "ERROR: Unable to find the configuration file '%s'", cfg);

		return EX_NOINPUT;
	}

	OTELC_LOG(stdout, "--- OpenTelemetry C Wrapper: %s ---", banner);
	OTELC_LOG(stdout, "Config: %s", cfg);

#ifdef OTELC_DBG_MEM
	{
		static struct otelc_dbg_mem_data dbg_mem_data[1000000];
		static struct otelc_dbg_mem      dbg_mem;

		if (otelc_dbg_mem_init(&dbg_mem, dbg_mem_data, OTELC_TABLESIZE(dbg_mem_data)) == OTELC_RET_ERROR) {
			OTELC_LOG(stderr, "ERROR: Unable to initialize memory debugger");

			return EX_SOFTWARE;
		}
	}
#endif

	(void)otelc_runtime();
	otelc_ext_init(NULL, NULL, NULL);

	*cfg_file = cfg;

	return -1;
}


/***
 * NAME
 *   test_set_tracer - registers a tracer for automatic cleanup
 *
 * SYNOPSIS
 *   void test_set_tracer(struct otelc_tracer **tracer)
 *
 * ARGUMENTS
 *   tracer - address of the tracer pointer variable
 *
 * DESCRIPTION
 *   Registers a tracer pointer so that test_done() can destroy it
 *   automatically during cleanup.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void test_set_tracer(struct otelc_tracer **tracer)
{
	registered_tracer = tracer;
}


/***
 * NAME
 *   test_set_meter - registers a meter for automatic cleanup
 *
 * SYNOPSIS
 *   void test_set_meter(struct otelc_meter **meter)
 *
 * ARGUMENTS
 *   meter - address of the meter pointer variable
 *
 * DESCRIPTION
 *   Registers a meter pointer so that test_done() can destroy it
 *   automatically during cleanup.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void test_set_meter(struct otelc_meter **meter)
{
	registered_meter = meter;
}


/***
 * NAME
 *   test_set_logger - registers a logger for automatic cleanup
 *
 * SYNOPSIS
 *   void test_set_logger(struct otelc_logger **logger)
 *
 * ARGUMENTS
 *   logger - address of the logger pointer variable
 *
 * DESCRIPTION
 *   Registers a logger pointer so that test_done() can destroy it
 *   automatically during cleanup.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void test_set_logger(struct otelc_logger **logger)
{
	registered_logger = logger;
}


/***
 * NAME
 *   test_done - performs common test cleanup and prints the summary
 *
 * SYNOPSIS
 *   int test_done(int retval, char *otel_err)
 *
 * ARGUMENTS
 *   retval   - current exit status from the caller
 *   otel_err - error string allocated by the library, or NULL
 *
 * DESCRIPTION
 *   Destroys any registered tracer, meter, or logger, deinitializes
 *   the OpenTelemetry library, frees the error string if it is
 *   non-NULL, and prints the test results summary.
 *
 * RETURN VALUE
 *   Returns EX_SOFTWARE if any tests failed, otherwise returns the
 *   value passed in via retval.
 */
int test_done(int retval, char *otel_err)
{
	otelc_deinit(registered_tracer, registered_meter, registered_logger);

	OTELC_SFREE(otel_err);

	return test_summary(retval);
}


/***
 * NAME
 *   test_summary - prints the test results summary
 *
 * SYNOPSIS
 *   int test_summary(int retval)
 *
 * ARGUMENTS
 *   retval - current exit status from the caller
 *
 * DESCRIPTION
 *   Prints the final counts of passed, failed, and total tests.  If
 *   any tests have failed, the return value is overridden to
 *   EX_SOFTWARE.
 *
 * RETURN VALUE
 *   Returns EX_SOFTWARE if any tests failed, otherwise returns the
 *   value passed in via retval.
 */
int test_summary(int retval)
{
	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "--- Results: %d passed, %d failed, %d total ---", tests_passed, tests_failed, tests_run);

	if (tests_failed > 0)
		retval = EX_SOFTWARE;

	return retval;
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
