#!/usr/bin/perl

use File::Basename;

$usage = "perl $0 <src_file> <dst_file>\n";
die "$usage" if(@ARGV < 2);

sub is_script {
  my @exts = qw(.c .cpp .h);
  my $fn = shift;
  my ($name, $dir, $ext) = fileparse($fn, @exts);
  return ($ext eq "");
}
sub split_csrc {
  my $fd = shift;
  my $stage = 0;
  my $header = "", $body = "", $body1 = "", $lic = "";
  while(<$fd>) {
    if($stage == 0) {
      next if /^\b*$/;
      $stage = /^\b*\/\*/ ? 1 : 2;
      $header = $_ if $stage == 1;
      $body = $_ if $stage == 2;
    } elsif ($stage == 1) {
      $header .= $_;
      $stage = 2 if(/.*\*\//);
    } elsif (/MODULE_LICENSE\("[^"]*"\);/){
      $body1 = $body;
      $body = "";
      $lic = "$&\n";
    } else {
      $body .= $_;
    }
  }
  if ($lic eq "") {
    $body1 = $body;
    $body = "";
  }
  return ($header, $body1, $lic, $body);
}
sub split_script {
  my $fd = shift;
  my $stage = 0;
  my $header = "", $body = "";
  while(<$fd>) {
    if($stage == 0) {
      next if /^\b*$/;
      $stage = /^#/ ? 1 : 2;
      $header = $_ if $stage == 1;
      $body = $_ if $stage == 2;
    } elsif ($stage == 1) {
      if(/^#/) {
        $header .= $_;
      } else {
        $body .= $_;
        $stage = 2;
      }
    } else {
      $body .= $_;
    }
  }
  return ($header, $body, "", "");
}

open my $src, "< $ARGV[0]" or die "cannot open source file $ARGV[1]";
open my $dst, "< $ARGV[1]" or die "cannot open destination file $ARGV[2]";
my $b_script = is_script($ARGV[0]);
my ($header0, $body01, $lic0, $body02) = $b_script ? split_script($dst) : split_csrc($dst);
close $dst;
open $dst, "> $ARGV[1]" or die "cannot write destination file $ARGV[2]";
($header, $body1, $lic, $body2) = $b_script ? split_script($src) : split_csrc($src);
print $dst $header0 . $body1 . $lic0 . $body2;
close $dst;
close $src;
print "$ARGV[0] -> $ARGV[1]\n";
