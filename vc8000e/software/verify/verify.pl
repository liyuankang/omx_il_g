#
# run encoder for different configurations.
#

$tb_file = "tb.cfg";
$hevc_file = "hevc.bin";
$report_file = "report.txt";

$usage = "perl verify.pl config_file\n";

die "$usage" if (@ARGV < 1);

$pattern_file = $ARGV[0];

write_tb_cfg();

parse_cfg($pattern_file);

$exe_file = $HENC;
$yuv_list_file = $YUV;

open(PAR_LIST, "$PARAS") || die "Cannot open PAR_LIST file $PARAS!\n"; 
open(YUV_LIST, "$yuv_list_file") || die "Cannot open YUV_LIST file $yuv_list_file!\n"; 
open(RPT_FILE, ">$report_file") || die "Cannot open RPT_FILE file $report_file!\n"; 

$stream_count = 0;
while ($line = <YUV_LIST>)
{
	print $line;
	chomp $line;
	$head = substr($line, 0,1);
	if ($head eq "#")
	{
		print "skip $line\n";
		next;
	}
	@files = split(/\s/, $line);
	$cfg_files[$stream_count] = $files[0];
	$yuv_files[$stream_count] = $files[1];
	$stream_count++;
}
	
print RPT_FILE "\nVERIFY REPORT\n";
$pass_count = 0;
$fail_count = 0;

while ($para = <PAR_LIST>)
{
	chomp $para;
	if (!($para =~ /[#-]/))
	{
		next;
	}
	
	$head = substr($para, 0,1);
	if ($head eq "#")
	{
		print "note: $para\n";
		next;
	}
	
	print RPT_FILE "\n####################################################################\n";
	print RPT_FILE "Checking paramter: $para\n";
	
	for($i=0; $i<$stream_count; $i++)
	{
		$cfg_file = $cfg_files[$i]; $cfg_file = $BASE_DIR . "/" . $cfg_file; 
		$yuv_file = $yuv_files[$i]; $yuv_file = $BASE_DIR . "/" . $yuv_file; 

		$enc_options = $para;
		
	    # parse the stream config
    	get_stream_cfg($cfg_file);
	    
	    # run encoder
		$cmd = "$HENC -w$width -h$height $enc_options -i $yuv_file -o $hevc_file";
		print "$cmd\n";
		`$cmd`;
		
		# run decoder
		$cmd = "$HDEC -b $hevc_file -o dec_output.yuv";
		print "$cmd\n";
		`$cmd`;
		
		# compare
		$cmd = "diff -s -q deblock.yuv dec_output.yuv";
		print "$cmd\n";
		$result = `$cmd`;
		if ($result =~ /are identical$/)
		{
			print RPT_FILE "PASS $yuv_file\n";
			print ">>>>>PASS FILE:$yuv_file PARAM:$para\n";
			$pass_count++;
		}
		else
		{
			print RPT_FILE "FAIL $yuv_file\n";
			print ">>>>>FAIL FILE:$yuv_file PARAM:$para\n";
			$fail_count++;
		}
		#print $result;
	}
}

$total_count = $pass_count+$fail_count;

print RPT_FILE "\n\n$total_count Patterns Run, $pass_count PASSED, $fail_count FAILED!\n";
print "\n<<<<<REPORT>>>>>\n$total_count Patterns Run, $pass_count PASSED, $fail_count FAILED!\n\n";

close(YUV_LIST);
close(PAR_LIST);
close(RTP_FILE);

###############################################################################

sub write_tb_cfg()
{
	my $line;
	
	open(TB_FD, ">$tb_file") || die "Cannot open to write the out.cfg file $tb_file\n";
	print TB_FD "deblock.yuv";
	close(TB_FD);
}

sub parse_cfg()
{
	my $cfg_file = shift;
	my $line;
	
	open(CFG_FD, "$cfg_file") || die "Cannot open cfg file $cfg_file\n";
	while($line = <CFG_FD>) {
		if ($line =~ /BASE=(\S+)/)
		{
			$BASE_DIR = $1;
		}
		if ($line =~ /HENC=(\S+)/)
		{
			$HENC = $1;
		}
		if ($line =~ /HDEC=(\S+)/)
		{
			$HDEC = $1;
		}
		if ($line =~ /YUV=(\S+)/)
		{
			$YUV = $1;
		}
		if ($line =~ /PARAS=(\S+)/)
		{
			$PARAS = $1;
		}
	}
	close(CFG_FD);
}

sub get_stream_cfg()
{
	my $cfg_file = shift;
	my $line;
	
	open(CFG_FD, "$cfg_file") || die "Cannot open cfg file $cfg_file\n";
	while($line = <CFG_FD>)
	{
		if ($line =~ /FrameRate\s*\:\s*([0-9]+)/)
		{
			$framerate = $1;
		}
		if ($line =~ /SourceWidth\s*\:\s*([0-9]+)/)
		{
			$width = $1;
		}
		if ($line =~ /SourceHeight\s*\:\s*([0-9]+)/)
		{
			$height = $1;
		}
		if ($line =~ /FramesToBeEncoded\s*\:\s*([0-9]+)/)
		{
			$total_frames = $1;
		}
	}
	close(CFG_FD);
}
