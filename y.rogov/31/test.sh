#!/bin/sh
build/31_server.out &
SERVER=$!
sleep 1

build/32_client.out bbbbbb 2000000 &
build/32_client.out aaaa 1000000 &

trap 'kill $SERVER 2>/dev/null; pkill -P $$ 2>/dev/null' INT TERM
wait $SERVER