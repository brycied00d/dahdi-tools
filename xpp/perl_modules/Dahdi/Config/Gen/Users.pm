package Dahdi::Config::Gen::Users;
use strict;

use File::Basename;
use Dahdi::Config::Gen qw(is_true);

sub new($$$) {
	my $pack = shift || die;
	my $gconfig = shift || die;
	my $genopts = shift || die;
	my $file = $ENV{USERS_FILE} || "/etc/asterisk/users.conf";
	my $self = {
			FILE	=> $file,
			GCONFIG	=> $gconfig,
			GENOPTS	=> $genopts,
		};
	bless $self, $pack;
	return $self;
}

sub gen_channel($) {
	my $self = shift || die;
	my $chan = shift || die;
	my $gconfig = $self->{GCONFIG};
	my $type = $chan->type;
	my $num = $chan->num;
	die "channel $num type $type is not an analog channel\n" if $chan->span->is_digital();
	my $exten = $gconfig->{'base_exten'} + $num;
	my $sig = $gconfig->{'chan_dahdi_signalling'}{$type};
	my $full_name = "$type $num";

	die "missing default_chan_dahdi_signalling for chan #$num type $type" unless $sig;
	print << "EOF";
[$exten]
callwaiting = yes
context = numberplan-custom-1
fullname = $full_name
cid_number = $exten
hasagent = no
hasdirectory = no
hasiax = no
hasmanager = no
hassip = no
hasvoicemail = yes
host = dynamic
mailbox = $exten
threewaycalling = yes
vmsecret = 1234
secret = 1234
signalling = $sig
dahdichan = $num
registeriax = no
registersip = no
canreinvite = no
nat = no
dtmfmode = rfc2833
disallow = all
allow = all

EOF
}

# generate users.conf . The specific users.conf is strictly oriented
# towards using with the asterisk-gui .
#
# This code could have generated a much simpler and smaller
# configuration file, had there been minimal level of support for
# configuration templates in the asterisk configuration rewriting. Right
# now Asterisk's configuration rewriting simply freaks out in the face
# of templates: http://bugs.digium.com/11442 .
sub generate($) {
	my $self = shift || die;
	my $file = $self->{FILE};
	my $gconfig = $self->{GCONFIG};
	my $genopts = $self->{GENOPTS};
	#Dahdi::Config::Gen::show_gconfig($gconfig);
	my @spans = @_;
	warn "Empty configuration -- no spans\n" unless @spans;
	rename "$file", "$file.bak"
		or $! == 2	# ENOENT (No dependency on Errno.pm)
		or die "Failed to backup old config: $!\n";
	print "Generating $file\n" if $genopts->{verbose};
	open(F, ">$file") || die "$0: Failed to open $file: $!\n";
	my $old = select F;
	print <<"HEAD";
;!
;! Automatically generated configuration file
;! Filename: @{[basename($file)]} ($file)
;! Generator: $0
;! Creation Date: @{[scalar(localtime)]}
;!
[general]
;
; Full name of a user
;
fullname = New User
;
; Starting point of allocation of extensions
;
userbase = @{[$gconfig->{'base_exten'}+1]}
;
; Create voicemail mailbox and use use macro-stdexten
;
hasvoicemail = yes
;
; Set voicemail mailbox @{[$gconfig->{'base_exten'}+1]} password to 1234
;
vmsecret = 1234
;
; Create SIP Peer
;
hassip = no
;
; Create IAX friend
;
hasiax = no
;
; Create Agent friend
;
hasagent = no
;
; Create H.323 friend
;
;hash323 = yes
;
; Create manager entry
;
hasmanager = no
;
; Remaining options are not specific to users.conf entries but are general.
;
callwaiting = yes
threewaycalling = yes
callwaitingcallerid = yes
transfer = yes
canpark = yes
cancallforward = yes
callreturn = yes
callgroup = 1
pickupgroup = 1
localextenlength = @{[length($gconfig->{'base_exten'})]}


HEAD
	foreach my $span (@spans) {
		next unless grep { $_ eq $span->type} ( 'FXS', 'IN', 'OUT' );
		printf "; Span %d: %s %s\n", $span->num, $span->name, $span->description;
		foreach my $chan ($span->chans()) {
			$self->gen_channel($chan);
		}
		print "\n";
	}
	close F;
	select $old;
}

1;

__END__

=head1 NAME

users - Generate configuration for users.conf.

=head1 SYNOPSIS

 use Dahdi::Config::Gen::Users;

 my $cfg = new Dahdi::Config::Gen::Users(\%global_config, \%genopts);
 $cfg->generate(@span_list);

=head1 DESCRIPTION

Generate the F</etc/asterisk/users.conf> which is used by asterisk(1) 
and AsteriskGUI.

Its location may be overriden via the environment variable F<USERS_FILE>.
