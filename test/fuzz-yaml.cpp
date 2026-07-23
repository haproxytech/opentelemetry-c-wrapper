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
 * libFuzzer harness for the YAML configuration loader.  Every input is
 * presented to otelc_init() through a memfd-backed path, once with a
 * context name the seed corpus contains and once with a name that is
 * absent, so both the parse paths and the context lookup paths run.  A
 * successfully created context is released again through otelc_deinit().
 *
 * Build and run through test/fuzz-yaml.sh, which prepares a clang build
 * of the library instrumented with -fsanitize=address,fuzzer-no-link so
 * the fuzzer receives coverage feedback from the library code.
 */
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <opentelemetry-c-wrapper/include.h>


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
 *   that malformed configurations produce on every iteration.
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
 *   fuzz_one_name - loads the configuration under one context name
 *
 * SYNOPSIS
 *   static void fuzz_one_name(const char *path, const char *name)
 *
 * ARGUMENTS
 *   path - path of the configuration file to load
 *   name - context name to resolve within the configuration
 *
 * DESCRIPTION
 *   Runs one otelc_init()/otelc_deinit() cycle and releases the error
 *   string the library may have allocated.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void fuzz_one_name(const char *path, const char *name)
{
	struct otelc_ctx *ctx;
	char             *err = NULL;

	ctx = otelc_init(path, name, &err);
	if (ctx != NULL)
		otelc_deinit(&ctx, NULL, NULL, NULL);

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
 *   Initializes the library runtime and silences the diagnostic log
 *   handler before the first input runs.
 *
 * RETURN VALUE
 *   Always returns 0.
 */
extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;

	(void)otelc_runtime();
	otelc_ext_init(NULL, NULL, NULL);
	otelc_log_set_handler(fuzz_log_handler, NULL, false);

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
 *   Writes the input into a memfd, presents it to the library as a YAML
 *   configuration file, and loads it under a context name present in the
 *   seed corpus as well as under an absent one.
 *
 * RETURN VALUE
 *   Always returns 0.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char    path[64];
	ssize_t nbytes;
	size_t  offset = 0;
	int     fd;

	fd = memfd_create("otelc-fuzz-yaml", 0);
	if (fd == -1)
		return 0;

	while (offset < size) {
		nbytes = write(fd, data + offset, size - offset);
		if (nbytes <= 0)
			break;

		offset += (size_t)nbytes;
	}

	if (offset == size) {
		(void)snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);

		fuzz_one_name(path, "default");
		fuzz_one_name(path, "fuzz_absent_name");
	}

	(void)close(fd);

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
