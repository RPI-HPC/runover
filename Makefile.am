AUTOMAKE_OPTIONS = foreign

man1_MANS = runover.man

bin_PROGRAMS = runover

runover_SOURCES = runover.c ca.h qo.h av.c av.h cfp.c cfp.h

runover_CPPFLAGS = -DRO_CONFIG_SCRIPT=\"$(RO_CONFIG_SCRIPT)\" -D RO_MACHINE_SCRIPT=\"$(RO_MACHINE_SCRIPT)\" $(AM_CPPFLAGS)

EXTRA_DIST = \
	$(man1_MANS) \
	config-script.sh \
	machine-script.sh \
	ToDo.txt

