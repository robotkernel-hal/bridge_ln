lib_LTLIBRARIES = libbridge_ln.la

libbridge_ln_la_SOURCES  = ln_bridge.cpp
libbridge_ln_la_CXXFLAGS = -Wall  						            \
									  -I$(top_srcdir)/src 			\
									  -I$(top_builddir) 			\
									  @ROBOTKERNEL_CFLAGS@ 			\
									  @LINKS_AND_NODES_CFLAGS@

libbridge_ln_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)		\
						   @LINKS_AND_NODES_LIBS@

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(top_srcdir)/share/ln/* $(prefix)/share/ln/
	-cp -rf $(top_builddir)/share/ln/* $(prefix)/share/ln/
