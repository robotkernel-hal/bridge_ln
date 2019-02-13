lib_LTLIBRARIES = libbridge_ln.la

libbridge_ln_la_SOURCES  = ln_bridge.cpp \
	../../../externals/links_and_nodes/library/src/async_thread.c \
	../../../externals/links_and_nodes/library/src/aux_mem.c \
	../../../externals/links_and_nodes/library/src/event.c \
	../../../externals/links_and_nodes/library/src/iovec_mgr.c \
	../../../externals/links_and_nodes/library/src/line_assembler.c \
	../../../externals/links_and_nodes/library/src/ln_client.c \
	../../../externals/links_and_nodes/library/src/logger.c \
	../../../externals/links_and_nodes/library/src/multi_waiter.c \
	../../../externals/links_and_nodes/library/src/os_android.c \
	../../../externals/links_and_nodes/library/src/os_linux.c \
	../../../externals/links_and_nodes/library/src/os_qnx.c \
	../../../externals/links_and_nodes/library/src/os_vxworks.c \
	../../../externals/links_and_nodes/library/src/os_win32.c \
	../../../externals/links_and_nodes/library/src/port.c \
	../../../externals/links_and_nodes/library/src/posix_shm.c \
	../../../externals/links_and_nodes/library/src/publish.c \
	../../../externals/links_and_nodes/library/src/repr.c \
	../../../externals/links_and_nodes/library/src/request.c \
	../../../externals/links_and_nodes/library/src/service.c \
	../../../externals/links_and_nodes/library/src/shm.c \
	../../../externals/links_and_nodes/library/src/shm_source.c \
	../../../externals/links_and_nodes/library/src/shm_target.c \
	../../../externals/links_and_nodes/library/src/subscribe.c \
	../../../externals/links_and_nodes/library/src/thread_pool.c \
	../../../externals/links_and_nodes/library/src/process_shared_ipc.cpp

libbridge_ln_la_CXXFLAGS = -D__LINUX__ -Wall  						            \
									  -I$(top_srcdir)/externals/links_and_nodes/library/src \
									  -I$(top_srcdir)/externals/links_and_nodes/library/include \
									  -I$(top_srcdir)/src 			\
									  -I$(top_builddir) 			\
									  @ROBOTKERNEL_CFLAGS@

libbridge_ln_la_CFLAGS = -D__LINUX__ -Wall  						            \
									  -I$(top_srcdir)/externals/links_and_nodes/library/src \
									  -I$(top_srcdir)/externals/links_and_nodes/library/include \
									  -I$(top_srcdir)/src 			\
									  -I$(top_builddir) 			\
									  @ROBOTKERNEL_CFLAGS@

libbridge_ln_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION) -lpthread

USED_MESSAGES=ln/request_topic \
        ln/parameters/query \
        ln/parameters/query_dict \
        ln/parameters/override_dict_single \
        ln/parameters/request_parameter_topic \
        ln/event/call \
        ln/event/connect \
        ln/log_service

BUILT_SOURCES = ln_client_lib_messages.h
CLEANFILES = ln_client_lib_messages.h

ln_client_lib_messages.h: 
	../../../externals/links_and_nodes/library/tools/ln_generate \
		-o $(top_srcdir)/externals/links_and_nodes/library/src/ln_client_lib_messages.h $(USED_MESSAGES)

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(top_srcdir)/share/ln/* $(prefix)/share/ln/
	-cp -rf $(top_builddir)/share/ln/* $(prefix)/share/ln/
