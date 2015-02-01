$benchnum  = '177';
$benchname = 'mesa';
$exename   = 'mesa';
$benchlang = 'C';
@base_exe  = ($exename);
$abstol    = 6;

@sources=qw(accum.c alpha.c alphabuf.c api1.c api2.c attrib.c bitmap.c blend.c
	    clip.c colortab.c context.c copypix.c depth.c dlist.c drawpix.c
	    enable.c eval.c feedback.c fog.c get.c hash.c image.c light.c
	    lines.c logic.c masking.c matrix.c misc.c mmath.c osmesa.c pb.c
	    pixel.c pointers.c points.c polygon.c quads.c rastpos.c readpix.c
	    rect.c scissor.c shade.c span.c stencil.c teximage.c texobj.c
	    texstate.c texture.c triangle.c varray.c vb.c vbfill.c vbrender.c
	    vbxform.c winpos.c xform.c .c mesa4.c);
$need_math='yes';
$obiwan    = 1;
$skiptol   = 6;

sub invoke {
    my ($me) = @_;
    my $name;
    my @rc;
    my $frames;

    my $size = $me->size;
    if($size eq 'test') {
	$frames = 10;
    } elsif($size eq 'train') {
	$frames = 500;
    } elsif($size eq 'ref') {
	$frames = 1000;
    } else {
	main::Log(0, $me->benchmark,": Doesn't support size '",$me->size,"'\n");
	return undef;
    }

    my $exe = $me->exe_file;
    for ($me->input_files_base) {
	if (($name) = m/(.*).in$/) {
	    push (@rc, { 'command' => $exe, 
			 'args'    => [ "-frames $frames", "-meshfile $name.in",
			                "-ppmfile $name.ppm" ], 
			});
	}
    }
    return @rc;
}

1;
