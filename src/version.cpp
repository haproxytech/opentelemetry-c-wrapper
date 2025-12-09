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
#include "opentelemetry-c-wrapper/version.h"


/***
 * NAME
 *   otelc_version - gets library version
 *
 * SYNOPSIS
 *   const char *otelc_version(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Returns the library version in character string form.
 *
 * RETURN VALUE
 *   Returns the library version.
 */
const char *otelc_version(void)
{
	return OTELC_PACKAGE_VERSION "-" OTELC_STRINGIFY(OTELC_PACKAGE_BUILD);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
