#!/bin/sh

# A helper script to delete all shared message queue mappings.
killall client
killall dataserver

KEYS=$(ipcs -q | awk '{print $3}' | sed '/^[[:space:]]*$/d' | tail -n+3)

while read -r key; do
	echo "Deleting message queue with key $key"
	ipcrm -Q $key
done <<< "$KEYS"

ipcs -m
