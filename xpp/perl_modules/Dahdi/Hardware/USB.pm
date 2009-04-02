package Dahdi::Hardware::USB;
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
use Dahdi::Hardware;
use Dahdi::Xpp::Mpp;

our @ISA = qw(Dahdi::Hardware);

my %usb_ids = (
	# from wcusb
	'06e6:831c'	=> { DRIVER => 'wcusb', DESCRIPTION => 'Wildcard S100U USB FXS Interface' },
	'06e6:831e'	=> { DRIVER => 'wcusb2', DESCRIPTION => 'Wildcard S110U USB FXS Interface' },
	'06e6:b210'	=> { DRIVER => 'wc_usb_phone', DESCRIPTION => 'Wildcard Phone Test driver' },

	# from xpp_usb
	'e4e4:1130'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-8/16 no-firmware' },
	'e4e4:1131'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-8/16 USB-firmware' },
	'e4e4:1132'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-8/16 FPGA-firmware' },
	'e4e4:1140'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-BRI no-firmware' },
	'e4e4:1141'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-BRI USB-firmware' },
	'e4e4:1142'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-BRI FPGA-firmware' },
	'e4e4:1150'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-multi no-firmware' },
	'e4e4:1151'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-multi USB-firmware' },
	'e4e4:1152'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-multi FPGA-firmware' },
	'e4e4:1160'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-modular no-firmware' },
	'e4e4:1161'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-modular USB-firmware' },
	'e4e4:1162'	=> { DRIVER => 'xpp_usb', DESCRIPTION => 'Astribank-modular FPGA-firmware' },
	
	# Sangoma USB FXO:
	'10c4:8461'	=> { DRIVER => 'wanpipe', DESCRIPTION => 'Sangoma WANPIPE USB-FXO Device' },
	);


$ENV{PATH} .= ":/usr/sbin:/sbin:/usr/bin:/bin";

sub usb_sorter() {
	return $a->hardware_name cmp $b->hardware_name;
}

sub mpp_addinfo($) {
	my $self = shift || die;

	my $mppinfo = Dahdi::Xpp::Mpp->new($self);
	$self->{MPPINFO} = $mppinfo if defined $mppinfo;
}

sub new($@) {
	my $pack = shift or die "Wasn't called as a class method\n";
	my %attr = @_;
	my $name = sprintf("usb:%s", $attr{PRIV_DEVICE_NAME});
	my $self = Dahdi::Hardware->new($name, 'USB');
	%{$self} = (%{$self}, %attr);
	bless $self, $pack;
	return $self;
}

sub readval($) {
	my $fname = shift || warn;
	open(F, $fname) || warn "Failed opening '$fname': $!";
	my $val = <F>;
	close F;
	chomp $val;
	warn "$fname is empty" unless defined $val and $val;
	return $val;
}

sub set_transport($$) {
	my $pack = shift || die;
	my $xbus = shift || die;
	my $xbus_dir = shift;
	my $transportdir = "$xbus_dir/transport";
	my $hwdev;
	if(! -e "$transportdir/ep_00") {
		warn "A trasnport in '$transportdir' is not USB";
		return undef;
	}
	my ($usbdev) = glob("$transportdir/usb_device:*");
	my $busnum;
	my $devnum;
	# Different kernels...
	if(defined $usbdev) {	# It's USB
		if($usbdev =~ /.*usb_device:usbdev(\d+)\.(\d+)/) {
			$busnum = $1;
			$devnum = $2;
		} else {
			warn "Bad USB transportdir='$transportdir' usbdev='$usbdev'\n";
		}
	} elsif(-d "$transportdir/usb_endpoint") {
		$busnum = readval("$transportdir/busnum");
		$devnum = readval("$transportdir/devnum");
	}
	my $usbname = sprintf("%03d/%03d", $busnum, $devnum);
	#printf STDERR "DEBUG: %03d/%03d\n", $busnum, $devnum;
	$xbus->{USB_DEVNAME} = $usbname;
	$hwdev = Dahdi::Hardware->device_by_hwname("usb:$usbname");
	#print "set_transport: ", $hwdev, "\n";
	$xbus->{TRANSPORT} = $hwdev;
	$hwdev->{XBUS} = $xbus;
	$hwdev->{LOADED} = 'xpp_usb';
	$xbus->{IS_TWINSTAR} = $hwdev->is_twinstar;
	return $hwdev;
}

sub scan_devices($) {
	my $pack = shift || die;
	my $usb_device_list = "/proc/bus/usb/devices";
	return unless (-r $usb_device_list);

	my @devices;
	open(F, $usb_device_list) || die "Failed to open $usb_device_list: $!";
	local $/ = '';
	while(<F>) {
		my @lines = split(/\n/);
		my ($tline) = grep(/^T/, @lines);
		my ($pline) = grep(/^P/, @lines);
		my ($sline) = grep(/^S:.*SerialNumber=/, @lines);
		my ($busnum,$devnum) = ($tline =~ /Bus=(\w+)\W.*Dev#=\s*(\w+)\W/);
		my $devname = sprintf("%03d/%03d", $busnum, $devnum);
		my ($vendor,$product) = ($pline =~ /Vendor=(\w+)\W.*ProdID=(\w+)\W/);
		my $serial;
		if(defined $sline) {
			$sline =~ /SerialNumber=(.*)/;
			$serial = $1;
			#$serial =~ s/[[:^print:]]/_/g;
		}
		my $model = $usb_ids{"$vendor:$product"};
		next unless defined $model;
		my $d = Dahdi::Hardware::USB->new(
			IS_ASTRIBANK		=> ($model->{DRIVER} eq 'xpp_usb')?1:0,
			PRIV_DEVICE_NAME	=> $devname,
			VENDOR			=> $vendor,
			PRODUCT			=> $product,
			SERIAL			=> $serial,
			DESCRIPTION		=> $model->{DESCRIPTION},
			DRIVER			=> $model->{DRIVER},
			);
		push(@devices, $d);
	}
	close F;
	@devices = sort usb_sorter @devices;
	return @devices;
}

1;
