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


#ifdef DEBUG

/***
 * NAME
 *   otel_debug_attributes - prints resource attributes for debugging
 *
 * SYNOPSIS
 *   void otel_debug_attributes(const otel_sdk_resource::ResourceAttributes &attributes, const char *msg)
 *
 * ARGUMENTS
 *   attributes - the resource attributes to be printed
 *   msg        - a descriptive message to print with the attributes
 *
 * DESCRIPTION
 *   A debugging function that prints the key-value pairs of the given resource
 *   attributes to standard output.  This is useful for verifying the attributes
 *   that have been detected and applied to a resource.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_debug_attributes(const otel_sdk_resource::ResourceAttributes &attributes, const char *msg)
{
	OTELC_FUNC("<attributes>, \"%s\"", OTELC_STR_ARG(msg));

	OTELC_LOG(stdout, "%s (%zu)", OTELC_STR_ARG(msg), attributes.size());

	if (attributes.size() <= 0)
		OTELC_RETURN();

	for (const auto &it : attributes)
		if (it.second.index() == otel_sdk_common::kTypeString)
			OTELC_LOG(stdout, "  %s: %s (%zu)", it.first.c_str(), otel_nostd::get<std::string>(it.second).c_str(), it.second.index());
		else
			OTELC_LOG(stdout, "  %s: (%zu)", it.first.c_str(), it.second.index());

	OTELC_RETURN();
}

#endif /* DEBUG */


/***
 * NAME
 *   otel_resource_detector - detects resource attributes from the environment
 *
 * SYNOPSIS
 *   static int otel_resource_detector(otel_sdk_resource::ResourceAttributes &attributes)
 *
 * ARGUMENTS
 *   attributes - a map to store the detected resource attributes
 *
 * DESCRIPTION
 *   Detects resource attributes from environment variables, such as
 *   OTEL_RESOURCE_ATTRIBUTES and OTEL_SERVICE_NAME.  This allows for the
 *   configuration of resource information without modifying the application
 *   code.
 *
 *   - OTEL_RESOURCE_ATTRIBUTES: Key-value pairs to be used as resource
 *     attributes.
 *   - OTEL_SERVICE_NAME: Sets the value of the service.name resource attribute.
 *     If service.name is also provided in OTEL_RESOURCE_ATTRIBUTES, then
 *     OTEL_SERVICE_NAME takes precedence.
 *
 *   Example: 'export OTEL_RESOURCE_ATTRIBUTES="service.name=test"'
 *
 * RETURN VALUE
 *   Returns the number of attributes detected, or 0 if none are found.
 */
static int otel_resource_detector(otel_sdk_resource::ResourceAttributes &attributes)
{
	otel_sdk_resource::OTELResourceDetector detector{};
	int                                     retval;

	OTELC_FUNC("<attributes>");

	const auto resource   = detector.Detect();
	auto attributes_maybe = resource.GetAttributes();

	retval = attributes_maybe.size();
	if (retval > 0) {
		otel_debug_attributes(attributes_maybe, "env OTEL_RESOURCE_ATTRIBUTES");

		attributes = std::move(attributes_maybe);
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otel_resource_create - creates a resource from environment and YAML
 *
 * SYNOPSIS
 *   int otel_resource_create(const char *desc, const char *path, otel_sdk_resource::Resource &resource, char **err)
 *
 * ARGUMENTS
 *   desc     - description of the node, used in error messages
 *   path     - the path to the node in the YAML document
 *   resource - a reference to store the created resource
 *   err      - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Creates a resource by combining attributes detected from the environment
 *   (via otel_resource_detector) with attributes specified in a YAML
 *   configuration file.
 *
 * RETURN VALUE
 *   Returns the number of attributes added from the YAML file,
 *   or OTELC_RET_ERROR on failure.
 */
int otel_resource_create(const char *desc, const char *path, otel_sdk_resource::Resource &resource, char **err)
{
	otel_sdk_resource::ResourceAttributes  res_attr{};
	struct otelc_text_map                 *resources = nullptr;
	int                                    rc, retval = 0;

	OTELC_FUNC("\"%s\", \"%s\", <resource>, %p:%p", OTELC_STR_ARG(desc), OTELC_STR_ARG(path), OTELC_DPTR_ARGS(err));

	if (OTEL_NULL(desc))
		OTEL_ERR_RETURN_INT("Resource description not specified");
	else if (OTEL_NULL(path))
		OTEL_ERR_RETURN_INT("Resource path not specified");

	(void)otel_resource_detector(res_attr);

	rc = yaml_get_node(otelc_fyd, err, 0, desc, path, nullptr, OTEL_YAML_ARG_MAP(0, PROVIDERS, resources), OTEL_YAML_END);
	OTEL_DEFER_DPTR_FREE(struct otelc_text_map, resources, otelc_text_map_destroy);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (!OTEL_NULL(resources))
		for (size_t i = 0; i < resources->count; i++) {
			std::pair<std::unordered_map<std::string, otel_sdk_common::OwnedAttributeValue>::iterator, bool> emplace_status{};

			try {
				OTEL_DBG_THROW();
				emplace_status = res_attr.emplace(std::string{resources->key[i]}, std::string{resources->value[i]});
			}
			OTEL_CATCH_SIGNAL_RETURN( , OTEL_ERR_RETURN_INT, "Unable to add resource attribute")

			if (emplace_status.second)
				retval++;
			else
				OTELC_DBG(DEBUG, "Unable to add resource attribute: duplicate id '%s'", resources->key[i]);
		}

	otel_debug_attributes(res_attr, "provider attributes");

	resource = otel_sdk_resource::Resource::Create(std::move(res_attr));

	otel_debug_resource(resource, "provider resource");

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
