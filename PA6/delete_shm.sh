#!/bin/sh

# A helper script to delete all shared memory mappings.
killall client
killall dataserver

KEYS=$(ipcs -m | awk '{print $3}' | sed '/^[[:space:]]*$/d' | tail -n+3)

while read -r key; do
	echo "Deleting shared memory segment with key $key"
	ipcrm -M $key
done <<< "$KEYS"

ipcs -m
