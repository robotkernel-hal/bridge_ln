
libdir = $(prefix)/lib
lib_LTLIBRARIES = libbridge_ln.la

libbridge_ln_la_SOURCES  = ln_bridge.cpp
libbridge_ln_la_CXXFLAGS = -std=c++11 -Wall i						\
									  -I$(top_srcdir)/src 			\
									  -I$(top_builddir) 			\
									  @ROBOTKERNEL_CFLAGS@ 			\
									  @LIBSTRING_UTIL_CFLAGS@ 		\
									  @YAML_CPP_RMC_CFLAGS@			\
									  @LINKS_AND_NODES_CFLAGS@

libbridge_ln_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

