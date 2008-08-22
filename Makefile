#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://mozilla.org/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is AOLserver Code and related documentation
# distributed by AOL.
# 
# The Initial Developer of the Original Code is America Online,
# Inc. Portions created by AOL are Copyright (C) 1999 America Online,
# Inc. All Rights Reserved.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License (the "GPL"), in which case the
# provisions of GPL are applicable instead of those above.  If you wish
# to allow use of your version of this file only under the terms of the
# GPL and not to allow others to use your version of this file under the
# License, indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by the GPL.
# If you do not delete the provisions above, a recipient may use your
# version of this file under either the License or the GPL.
#
#
# $Header$
#

VERSION     = 0.1

NAVISERVER  = /usr/local/ns
NSD         = $(NAVISERVER)/bin/nsd

MODNAME     = nsvfs

MOD         = nsvfs.so
MODOBJS     = nsvfs.o file.o adp.o


include $(NAVISERVER)/include/Makefile.module



DTPLITE = dtplite

doc: html-doc man-doc

version_include.man:
	echo \[vset version $(VERSION)\] > version_include.man

html-doc: version_include.man
	$(MKDIR) doc/html
	$(DTPLITE) -o doc/html html doc/src/mann/nsvfs.man

man-doc: version_include.man
	$(MKDIR) doc/man
	$(DTPLITE) -o doc/man nroff doc/src/mann/nsvfs.man



NS_TEST_CFG		= -c -d -t tests/config.tcl
NS_TEST_ALL		= all.tcl $(TCLTESTARGS)
LD_LIBRARY_PATH	= LD_LIBRARY_PATH="./::$$LD_LIBRARY_PATH"

test: all
	export $(LD_LIBRARY_PATH); $(NSD) $(NS_TEST_CFG) $(NS_TEST_ALL)

runtest: all
	export $(LD_LIBRARY_PATH); $(NSD) $(NS_TEST_CFG)

gdbtest: all
	@echo set args $(NS_TEST_CFG) $(NS_TEST_ALL) > gdb.run
	export $(LD_LIBRARY_PATH); gdb -x gdb.run $(NSD)
	rm gdb.run

gdbruntest: all
	@echo set args $(NS_TEST_CFG) $(NS_TEST_ALL) > gdb.run
	export $(LD_LIBRARY_PATH); gdb -x gdb.run $(NSD)
	rm gdb.run

memcheck: all
	export $(LD_LIBRARY_PATH); valgrind --tool=memcheck $(NSD) $(NS_TEST_CFG) $(NS_TEST_ALL)



SRCS = nsvfs.c file.c adp.c vfs.h
EXTRA = README TODO sample-config.tcl Makefile tests doc version_include.man license.terms

dist: doc all
	rm -rf $(MODNAME)-$(VERSION)
	mkdir $(MODNAME)-$(VERSION)
	$(CP) $(SRCS) $(EXTRA) $(MODNAME)-$(VERSION)
	tar czf $(MODNAME)-$(VERSION).tgz $(MODNAME)-$(VERSION)


.PHONY: doc html-doc man-doc
