AM_CPPFLAGS =								\
	-I$(top_srcdir)

noinst_LTLIBRARIES = libessm.la

libessm_la_SOURCES =                    \
	essm-util.h							\
	essm-util.c							\
	essm-shutdown-common.h

libessm_la_CFLAGS =							\
	$(LIBX11_CFLAGS)						\
	$(LIBEXPIDUS1UI_CFLAGS)                    \
	$(LIBEXPIDUS1UTIL_CFLAGS)                  \
	$(ESCONF_CFLAGS)

if HAVE_OS_CYGWIN
libessm_la_LDFLAGS =						\
	-no-undefined
endif

libessm_la_LIBADD =							\
	$(LIBX11_LIBS)							\
	$(LIBEXPIDUS1UI_LIBS)						\
	$(LIBEXPIDUS1UTIL_LIBS)                    \
	$(ESCONF_LIBS)
