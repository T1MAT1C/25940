#!/bin/sh
build/31_server.out &
SERVER=$!
sleep 1

build/32_client.out b 50 &
build/32_client.out a 100 &

trap 'kill $SERVER 2>/dev/null; pkill -P $$ 2>/dev/null' INT TERM
wait $SERVER