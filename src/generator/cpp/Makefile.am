ACLOCAL_AMFLAGS = -I m4

LDADD = @ROBOTKERNEL_LIBS@ -llttng-ust @LINKS_AND_NODES_LIBS@
#AM_LDFLAGS = -Wl,--whole-archive,librobotkernel.a,--no-whole-archive
 
bin_PROGRAMS = bridge_ln_generator
bridge_ln_generator_SOURCES = generator.cpp fs_tools.cpp
bridge_ln_generator_CXXFLAGS = -I$(top_srcdir) -I$(top_builddir)/src/main/cpp/ @ROBOTKERNEL_CFLAGS@ @LINKS_AND_NODES_CFLAGS@

