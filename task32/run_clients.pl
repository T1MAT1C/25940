#!/usr/bin/env perl

use Time::HiRes qw(sleep);
use IO::Handle;

my $pid = fork();

my $first = $pid != 0;

open(my $client, '|-', './client32');
$client->autoflush(1);

while (1) {
    if ($first) {
        print $client "AAAAAAAAAA\n";
        sleep(0.5);
    } else {
        print $client "BBBBBBBBBBBBBBBBBBBB\n";
        sleep(0.48 + rand(0.04));
    }
}