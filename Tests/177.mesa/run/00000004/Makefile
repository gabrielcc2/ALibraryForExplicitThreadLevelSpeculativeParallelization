###############################################################################
# SPECtools98 
#   Version 0.3
#   This makefile should NOT be changed other than to customize for a 
#   particular benchmark.  Users may overide variables by placing them
#   in the Makefile.spec file.  Or by using the "runspec" program to 
#   build the Makefile.spec file for them.
###############################################################################
#	BENCHMARK	-- standard definitions for this benchmark

NUMBER    = 177
NAME      = mesa
EXEBASE   = mesa
BENCHLANG = C
SOURCES   = accum.c alpha.c alphabuf.c api1.c api2.c attrib.c \
	    bitmap.c blend.c clip.c colortab.c context.c copypix.c \
	    depth.c dlist.c drawpix.c enable.c eval.c feedback.c \
	    fog.c get.c hash.c image.c light.c lines.c logic.c \
	    masking.c matrix.c misc.c mmath.c osmesa.c pb.c pixel.c \
	    pointers.c points.c polygon.c quads.c rastpos.c \
	    readpix.c rect.c scissor.c shade.c span.c stencil.c \
	    teximage.c texobj.c texstate.c texture.c triangle.c \
	    varray.c vb.c vbfill.c vbrender.c vbxform.c winpos.c \
	    xform.c mesa4.c
NEED_MATH = yes

# Include standard template for this suite
-include ../../../../Makefile.defaults
-include ../../../Makefile.defaults
-include ../../Makefile.defaults
-include ../Makefile.defaults

nomakefiles:
	@echo "*** Whoops!  I didn't find a Makefile.defaults file."
	@echo "*** Your distribution is probably corrupt."
	@echo
