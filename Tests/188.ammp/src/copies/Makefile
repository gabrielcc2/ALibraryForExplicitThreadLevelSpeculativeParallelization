###############################################################################
# SPECtools2000
#   Version 0.3
#   This makefile should NOT be changed other than to customize for a 
#   particular benchmark.  Users may overide variables by placing them
#   in the Makefile.spec file.  Or by using the "runspec" program to 
#   build the Makefile.spec file for them.
###############################################################################
#	BENCHMARK	-- standard definitions for this benchmark

NUMBER    = 188
NAME      = ammp
EXEBASE   = ammp
BENCHLANG = C
SOURCES   = ammp.c box.c eval.c analyze.c anonbon.c noel.c animate.c \
	    atoms.c bonds.c angle.c gsdg.c hybrid.c unonbon.c \
	    rectmm.c vnonbon.c tgroup.c tset.c tailor.c torsion.c \
	    optimist.c variable.c math.c mom.c monitor.c \
	    significance.c random.c tether.c restrain.c
NEED_MATH=yes
BENCH_FLAGS = -DSPEC_CPU2000

# Include standard template for this suite
-include ../../../../Makefile.defaults
-include ../../../Makefile.defaults
-include ../../Makefile.defaults
-include ../Makefile.defaults

nomakefiles:
	@echo "*** Whoops!  I didn't find a Makefile.defaults file."
	@echo "*** Your distribution is probably corrupt."
	@echo
