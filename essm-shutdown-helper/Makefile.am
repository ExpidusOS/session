essm_shutdown_helperdir = $(HELPER_PATH_PREFIX)/expidus1/session
essm_shutdown_helper_PROGRAMS = 					\
	essm-shutdown-helper

essm_shutdown_helper_SOURCES =						\
	main.c

essm_shutdown_helper_CFLAGS =						\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libessm                 \
	$(LIBEXPIDUS1UTIL_CFLAGS)

essm_shutdown_helper_LDFLAGS =						\
	$(LIBEXPIDUS1UTIL_LIBS)
