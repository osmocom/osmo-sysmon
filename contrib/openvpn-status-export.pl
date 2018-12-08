#!/usr/bin/perl -w
use strict;

# Script to export the OpenVPN daemon status information (which clients
# are connected from where) as a JSON file that can be served via HTTP.
#
# (C) 2015 by sysmocom - s.f.m.c. GmbH, All rights reserved.
# Author: Harald Welte

use JSON;
use Linux::Inotify2;
use Net::Netmask;

my $OPENVPN_STATE_FILE = "/var/tmp/openvpn.status";
my $JSON_OUTPUT_FILE = "/var/www/openvpn/status.json";

my $srcip_table = {
	'Destination 1' => [
		'127.0.0.0/8',
		],
	'Peer 2' => [
		'8.8.0.0/16', '1.2.3.0/18',
		],
};

my %netblocks;

sub read_netmask_table($)
{
	my ($t) = @_;

	foreach my $k (keys %$t) {
		my $table = {};
		foreach my $net (@{$$t{$k}}) {
			my $block = new Net::Netmask($net);
			$block->storeNetblock($table);
		}
		$netblocks{$k} = $table;
	}
}

sub classify_srcip($)
{
	my ($ip) = @_;
	foreach my $k (%netblocks) {
		my $block = findNetblock($ip, $netblocks{$k});
		if ($block) {
			return $k;
		}
	}
	return undef;
}

# read the openvpn.status file and parse it, return hash reference to
# its contents.
sub get_openvpn_clients($)
{
	my ($fname) = @_;
	my $state = 'init';
	my $href;
	my @clients;

	$$href{version} = 1;

	open(INFILE, "<", $fname);
	while (my $line = <INFILE>) {
		chomp($line);
		if ($line =~ /^OpenVPN CLIENT LIST$/) {
			$state = 'client_list';
		} elsif ($line =~ /^ROUTING\ TABLE$/) {
			$state = 'routing_table';
		} else {
			if ($state eq 'client_list') {
				my %cl;
				if ($line =~ /^Updated,(.*)/) {
					$$href{updated} = $1;
				} elsif ($line =~ /^(\S+),([0-9\.]+)\:(\d+),(\d+),(\d+),(.*)$/) {
					$cl{name} = $1;
					$cl{srcip} = $2;
					$cl{operator} = classify_srcip($2);
					$cl{srcport} = $3 + 0;
					$cl{bytes_rx} = $4 + 0;
					$cl{bytes_tx} = $5 + 0;
					$cl{connected_since} = $6;
					push(@clients, \%cl);
				}
			}
		}
	}
	close(INFILE);

	$$href{clients} = \@clients;

	return $href;
}

# inotify handler to re-parse/convert openvpn.status on any change
sub status_in_handler
{
	my $e = shift;

	# read/parse openvpn.status
	my $cl = get_openvpn_clients($e->fullname);

	# write result to file
	open(OUTFILE, ">", $JSON_OUTPUT_FILE);
	print(OUTFILE to_json($cl, { pretty => 1 }));
	close(OUTFILE);

	# also print it to console for debugging
	print(to_json($cl, { pretty => 1 }));
}



# main

read_netmask_table($srcip_table);

my $inotify = new Linux::Inotify2 or die("Can't create inotify object: $!");
$inotify->watch($OPENVPN_STATE_FILE, IN_MODIFY, \&status_in_handler);

# endless loop, wait for inotify enents
1 while $inotify->poll;
