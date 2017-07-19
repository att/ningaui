#  Master file included by all mkfiles  in subdirs to set up common crap

#  We modified this to fit the ningaui environment.  If we suck in a fresh copy
#  of XFM we will need to change it. 

gflag = -g

MKSHELL = `echo ${GLMK_SHELL:-ksh}`
CC = `if ${GLMK_CC:-gcc} --help >/dev/null 2>&1; then echo ${GLMK_CC:-gcc}; else echo cc; fi `
IFLAGS = 
CFLAGS = $gflag
LFLAGS = 

NUKE = *.o *.a 

# the & meta character does not match / and this IS important!
&:      &.o
	$CC $IFLAGS $CFLAGS -o $target $prereq $LFLAGS

&.o:    &.c
	$CC $IFLAGS $CFLAGS -c $stem.c

all:V:  $ALL

nuke:
	rm -f $NUKE

