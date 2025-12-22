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
#ifndef OPENTELEMETRY_C_WRAPPER_INCLUDE_H
#define OPENTELEMETRY_C_WRAPPER_INCLUDE_H

#include <stdbool.h>

#ifdef OTELC_USE_INTERNAL_INCLUDES
#  include "opentelemetry-c-wrapper/define.h"
#  include "opentelemetry-c-wrapper/debug.h"
#  include "opentelemetry-c-wrapper/otel_cpp.h"
#  include "opentelemetry-c-wrapper/dbg_malloc.h"
#  include "opentelemetry-c-wrapper/util.h"
#  include "opentelemetry-c-wrapper/propagation.h"
#  include "opentelemetry-c-wrapper/span.h"
#  include "opentelemetry-c-wrapper/logger.h"
#  include "opentelemetry-c-wrapper/meter.h"
#  include "opentelemetry-c-wrapper/tracer.h"
#else
#  include <opentelemetry-c-wrapper/define.h>
#  include <opentelemetry-c-wrapper/debug.h>
#  include <opentelemetry-c-wrapper/otel_cpp.h>
#  include <opentelemetry-c-wrapper/dbg_malloc.h>
#  include <opentelemetry-c-wrapper/util.h>
#  include <opentelemetry-c-wrapper/propagation.h>
#  include <opentelemetry-c-wrapper/span.h>
#  include <opentelemetry-c-wrapper/logger.h>
#  include <opentelemetry-c-wrapper/meter.h>
#  include <opentelemetry-c-wrapper/tracer.h>
#  include <opentelemetry-c-wrapper/version.h>
#endif /* OTELC_USE_INTERNAL_INCLUDES */

#endif /* OPENTELEMETRY_C_WRAPPER_INCLUDE_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
