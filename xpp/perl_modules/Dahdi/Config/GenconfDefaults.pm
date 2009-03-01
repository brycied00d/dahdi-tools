package Dahdi::Config::GenconfDefaults;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2007, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;

sub new($$) {
	my $pack = shift || die;
	my $cfg_file = shift || die;
	my $self = { GENCONF_FILE => $cfg_file };
	bless $self, $pack;
	if(!open(F, $cfg_file)) {
		if(defined($!{ENOENT})) {
			#print STDERR "No $cfg_file. Assume empty config\n";
			return $self; # Empty configuration
		}
		die "$pack: Failed to open '$cfg_file': $!\n";
	}
	#print STDERR "$pack: $cfg_file\n";
	my $array_key;
	while(<F>) {
		my ($key, $val);
		chomp;
		s/#.*$//;
		s/\s+$//;	# trim tail whitespace
		next unless /\S/;
		if(defined $array_key && /^\s+/) {
			s/^\s+//;	# trim beginning whitespace
			push(@{$self->{$array_key}}, $_);
			next; 
		}
		undef $array_key;
		($key, $val) = split(/\s+/, $_, 2);
		$key = lc($key);
		if(! defined $val) {
			$array_key = $key;
			next;
		}
		die "$cfg_file:$.: Duplicate key '$key'\n", if exists $self->{$key};
		$self->{$key} = $val;
	}
	close F;
	return $self;
}

sub dump($) {
	my $self = shift || die;
	print STDERR "Genconf dump:\n";
	foreach my $k (sort keys %$self) {
		my $val = $self->{$k};
		my $ref = ref $val;
		#print STDERR "DEBUG: '$k', '$ref', '$val'\n";
		if($ref eq '') {
			printf STDERR "%-20s %s\n", $k, $val;
		} elsif($ref eq 'SCALAR') {
			printf STDERR "%-20s %s\n", $k, ${$val};
		} elsif($ref eq 'ARRAY') {
			printf STDERR "%s\n", $k;
			foreach my $v (@{$val}) {
				printf STDERR "\t%s\n", $v;
			}
		} else {
			printf STDERR "%-20s (%s) %s\n", $k, $ref, $val;
		}
	}
}

1;
