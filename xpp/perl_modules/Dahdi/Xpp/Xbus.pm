package Dahdi::Xpp::Xbus;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2007, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;
use Dahdi::Utils;
use Dahdi::Xpp::Xpd;

my $proc_base = "/proc/xpp";

sub xpds($) {
	my $xbus = shift;
	return @{$xbus->{XPDS}};
}

sub by_number($) {
	my $busnumber = shift;
	die "Missing xbus number parameter" unless defined $busnumber;
	my @xbuses = Dahdi::Xpp::xbuses();

	my ($xbus) = grep { $_->num == $busnumber } @xbuses;
	return $xbus;
}

sub by_label($) {
	my $label = shift;
	die "Missing xbus label parameter" unless defined $label;
	my @xbuses = Dahdi::Xpp::xbuses();

	my ($xbus) = grep { $_->label eq $label } @xbuses;
	return $xbus;
}

sub get_xpd_by_number($$) {
	my $xbus = shift;
	my $xpdid = shift;
	die "Missing XPD id parameter" unless defined $xpdid;
	$xpdid = sprintf("%02d", $xpdid);
	my @xpds = $xbus->xpds;
	my ($wanted) = grep { $_->id eq $xpdid } @xpds;
	return $wanted;
}

sub xbus_attr_path($$) {
	my ($busnum, @attr) = @_;
	foreach my $attr (@attr) {
		my $file = sprintf "$Dahdi::Xpp::sysfs_astribanks/xbus-%02d/$attr", $busnum;
		unless(-f $file) {
			my $procfile = sprintf "/proc/xpp/XBUS-%02d/$attr", $busnum;
			warn "$0: OLD DRIVER: missing '$file'. Fall back to '$procfile'\n";
			$file = $procfile;
		}
		next unless -f $file;
		return $file;
	}
	return undef;
}

sub xbus_getattr($$) {
	my $xbus = shift || die;
	my $attr = shift || die;
	$attr = lc($attr);
	my $file = xbus_attr_path($xbus->num, lc($attr));

	open(F, $file) || die "Failed opening '$file': $!";
	my $val = <F>;
	close F;
	chomp $val;
	return $val;
}

sub read_attrs() {
	my $xbus = shift || die;
	my @attrnames = qw(CONNECTOR LABEL STATUS);
	my @attrs;

	foreach my $attr (@attrnames) {
		my $val = xbus_getattr($xbus, $attr);
		if($attr eq 'STATUS') {
			# Some values are in all caps as well
			$val = uc($val);
		} elsif($attr eq 'LABEL') {
			# Fix badly burned labels.
			$val =~ s/[[:^print:]]/_/g;
		}
		$xbus->{$attr} = $val;
	}
}

sub new($$) {
	my $pack = shift or die "Wasn't called as a class method\n";
	my $num = shift;
	my $xbus_dir = "$Dahdi::Xpp::sysfs_astribanks/xbus-$num";
	my $self = {
		NUM		=> $num,
		NAME		=> "XBUS-$num",
		SYSFS_DIR	=> $xbus_dir,
		};
	bless $self, $pack;
	$self->read_attrs;
	# Get transport related info
	my $transport = "$xbus_dir/transport";
	my ($usbdev) = glob("$transport/usb_device:*");
	if(defined $usbdev) {	# It's USB
		if($usbdev =~ /.*usb_device:usbdev(\d+)\.(\d+)/) {
			my $busnum = $1;
			my $devnum = $2;
			#printf STDERR "DEBUG: %03d/%03d\n", $busnum, $devnum;
			$self->{USB_DEVNAME} = sprintf("%03d/%03d", $busnum, $devnum);
		} else {
			warn "Bad USB transport='$transport' usbdev='$usbdev'\n";
		}
	}
	@{$self->{XPDS}} = ();
	opendir(D, $xbus_dir) || die "Failed opendir($xbus_dir): $!";
	while(my $entry = readdir D) {
		$entry =~ /^([0-9]+):([0-9]+):([0-9]+)$/ or next;
		my ($busnum, $unit, $subunit) = ($1, $2, $3);
		my $procdir = "/proc/xpp/XBUS-$busnum/XPD-$unit$subunit";
		#print STDERR "busnum=$busnum, unit=$unit, subunit=$subunit procdir=$procdir\n";
		my $xpd = Dahdi::Xpp::Xpd->new($self, $unit, $subunit, $procdir, "$xbus_dir/$entry");
		push(@{$self->{XPDS}}, $xpd);
	}
	closedir D;
	@{$self->{XPDS}} = sort { $a->id <=> $b->id } @{$self->{XPDS}};
	return $self;
}

sub pretty_xpds($) {
		my $xbus = shift;
		my @xpds = sort { $a->id <=> $b->id } $xbus->xpds();
		my @xpd_types = map { $_->type } @xpds;
		my $last_type = '';
		my $mult = 0;
		my $xpdstr = '';
		foreach my $curr (@xpd_types) {
			if(!$last_type || ($curr eq $last_type)) {
				$mult++;
			} else {
				if($mult == 1) {
					$xpdstr .= "$last_type ";
				} elsif($mult) {
					$xpdstr .= "$last_type*$mult ";
				}
				$mult = 1;
			}
			$last_type = $curr;
		}
		if($mult == 1) {
			$xpdstr .= "$last_type ";
		} elsif($mult) {
			$xpdstr .= "$last_type*$mult ";
		}
		$xpdstr =~ s/\s*$//;	# trim trailing space
		return $xpdstr;
}

1;
