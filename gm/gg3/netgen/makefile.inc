##############################################################################
#																			 #
#	Makefile for netgen								 #
#																			 #
#       created by Joachim Schoeberl												 #
#																			 #
##############################################################################

# load architecture dependent makefile	
include ../../../../ug.conf

default: $(objects) 
	$(ARCH_AR) $(ARCH_ARFLAGS)  ../../../../lib/libnetgen.a $(objects)

.PRECIOUS:   .cc
.SUFFIXES:   .cc
# default rule
.cc.o:
	$(ARCH_C++) $(UG_C++FLAGS) -I../general -I.. $<

# clean up
clean:
	rm -f *.o

