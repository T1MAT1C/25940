#!/bin/sh
build/32_server.out &
SERVER=$!
sleep 1

build/32_client.out b 5 &
build/32_client.out a 10 &

trap 'kill $SERVER 2>/dev/null; pkill -P $$ 2>/dev/null' INT TERM
wait $SERVER