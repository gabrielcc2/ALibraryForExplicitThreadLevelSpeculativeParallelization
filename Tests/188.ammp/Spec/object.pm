$benchnum  = '188';
$benchname = 'ammp';
$exename   = 'ammp';
$benchlang = 'C';
@base_exe  = ($exename);

$reltol = 0.003;
$abstol = 0.0001;

@sources=qw(ammp.c eval.c analyze.c anonbon.c noel.c animate.c atoms.c bonds.c
	    angle.c gsdg.c hybrid.c unonbon.c rectmm.c vnonbon.c tgroup.c
	    tset.c tailor.c torsion.c optimist.c variable.c math.c mom.c
	    monitor.c significance.c random.c tether.c restrain.c);
$need_math='yes';

sub invoke {
    my ($me) = @_;
    my $name;
    my @rc;

    my $exe = $me->exe_file;
    for ($me->input_files_base) {
	if (($name) = m/(.*).in$/) {
	    push (@rc, { 'command' => $exe, 
			 'args'    => [ ], 
			 'input'   => $_,
			 'output'  => "$name.out",
			 'error'   => "$name.err",
			});
	}
    }
    return @rc;
}

1;
