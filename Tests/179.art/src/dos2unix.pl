while($filename = shift(ARGV))
{
	open(FILE, $filename);
	while(<FILE>)
	{
		if(/(.*)\cM\n$/)
		{
			chop;
			print "$1\n";
		}
	}
}
