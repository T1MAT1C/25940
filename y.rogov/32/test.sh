#!/bin/sh
build/32_server.out &
SERVER=$!
sleep 1

build/32_client.out b 1000 &
build/32_client.out a 1000 &

trap 'kill $SERVER 2>/dev/null; pkill -P $$ 2>/dev/null' INT TERM
wait $SERVER