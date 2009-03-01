package Dahdi::Config::Gen;
require Exporter;
@ISA = qw(Exporter);

@EXPORT_OK = qw(is_true);

use strict;

sub is_true($) {
	my $val = shift;
	return undef unless defined $val;
	return $val =~ /^(1|y|yes)$/i;
}

sub show_gconfig($) {
	my $gconfig = shift || die;

	print "Global configuration:\n";
	foreach my $key (sort keys %{$gconfig}) {
		printf "  %-20s %s\n", $key, $gconfig->{$key};
	}
}

sub bchan_range($) {
	my $span = shift || die;
	my $first_chan = ($span->chans())[0];
	my $first_num = $first_chan->num();
	my $range_start = $first_num;
	my @range;
	my $prev = undef;

	die unless $span->is_digital();
	foreach my $c (@{$span->bchan_list()}) {
		my $curr = $c + $first_num;
		if(!defined($prev)) {
			$prev = $curr;
		} elsif($curr != $prev + 1) {
			push(@range, sprintf("%d-%d", $range_start, $prev));
			$range_start = $curr;
		}
		$prev = $curr;
	}
	if($prev >= $first_num) {
		push(@range, sprintf("%d-%d", $range_start, $prev));
	}
	return join(',', @range);
}

1;
