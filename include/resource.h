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
#ifndef _OPENTELEMETRY_C_WRAPPER_RESOURCE_H_
#define _OPENTELEMETRY_C_WRAPPER_RESOURCE_H_

int  otel_resource_create(const char *desc, const char *path, otel_sdk_resource::Resource &resource, char **err);

#ifdef DEBUG
void otel_debug_attributes(const otel_sdk_resource::ResourceAttributes &attributes, const char *msg);

#  define otel_debug_resource(r,m)     otel_debug_attributes((r).GetAttributes(), m)
#else
#  define otel_debug_attributes(...)   while (0)
#  define otel_debug_resource(...)     while (0)
#endif

#endif /* _OPENTELEMETRY_C_WRAPPER_RESOURCE_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
