lib_LTLIBRARIES = libbridge_ln.la

libbridge_ln_la_SOURCES  = ln_bridge.cpp
libbridge_ln_la_CXXFLAGS = -Wall -Wno-unused-function @ROBOTKERNEL_CFLAGS@ @LINKS_AND_NODES_CFLAGS@ @LN_HELPER_CFLAGS@
libbridge_ln_la_CFLAGS 	= -Wall -Wno-unused-function @ROBOTKERNEL_CFLAGS@ @LINKS_AND_NODES_CFLAGS@ @LN_HELPER_CFLAGS@
libbridge_ln_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION) -lpthread @LINKS_AND_NODES_LIBS@

install-exec-hook:
	$(STRIP) -K bridge_add_service -K bridge_configure -K bridge_remove_service -K bridge_unconfigure --strip-unneeded $(libdir)/libbridge_ln.so
