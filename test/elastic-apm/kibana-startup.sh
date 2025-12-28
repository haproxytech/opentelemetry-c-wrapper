#!/bin/sh -eu
#
SH_NAME="[$(basename "${0}" .sh)]"
 SH_URL="http://elasticsearch:9200/"


echo "${SH_NAME} Verifying kibana_system credentials..."
until curl -s -u "kibana_system:${KIBANA_PASSWORD}" "${SH_URL}_security/_authenticate" | grep -q '"username":"kibana_system"'; do
	echo "${SH_NAME} Authentication failed - password mismatch or ES not ready"
	sleep 5
done
echo "${SH_NAME} Authentication OK, starting Kibana"

exec /usr/local/bin/kibana-docker
