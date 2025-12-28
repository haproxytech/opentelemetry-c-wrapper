#!/bin/sh
#
SH_NAME="[$(basename "${0}" .sh)]"
 SH_URL="http://localhost:9200/"

. "$(dirname "${0}")/.env"


sh_elasticsearch_wait()
{
	echo "${SH_NAME} Waiting for Elasticsearch to be healthy..."

	# Wait until Elasticsearch cluster is healthy
	until curl -s -u "elastic:${ELASTIC_PASSWORD}" "${SH_URL}_cluster/health" | grep -qE '"status":"(green|yellow)"'; do
		echo "${SH_NAME} Elasticsearch is not ready yet..."
		sleep 5
	done
	echo "${SH_NAME} Elasticsearch is ready"
}

sh_elasticsearch_set_password()
{
	local _arg_user="${1}"
	local _arg_pwd="${2}"

	curl -s -u "${_arg_user}:${_arg_pwd}" "${SH_URL}_security/_authenticate" | grep -q "unable to authenticate user" || return

	# Use Elasticsearch REST API to reset the password for the user
	#
	echo "${SH_NAME} Resetting password for '${_arg_user}' -> '${_arg_pwd}'"

	curl -s -X POST "${SH_URL}_security/user/${_arg_user}/_password" \
	     -H "Content-Type: application/json" \
	     -u "elastic:${ELASTIC_PASSWORD}" \
	     -d '{ "password": "'"${_arg_pwd}"'" }' >/dev/null

	echo "${SH_NAME} Password reset for '${_arg_user}' completed"
}


sh_elasticsearch_wait

echo "${SH_NAME} Verifying built-in user passwords..."

sh_elasticsearch_set_password kibana_system "${KIBANA_PASSWORD}"
sh_elasticsearch_set_password apm_system    "${APM_PASSWORD}"

echo "${SH_NAME} All passwords verified"
