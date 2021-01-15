man_MANS =								\
	expidus1-session-logout.1

bin_PROGRAMS = 								\
	expidus1-session-logout

expidus1_session_logout_SOURCES =						\
	main.c

expidus1_session_logout_CFLAGS =						\
	-I$(top_srcdir)							\
	$(GIO_CFLAGS)							\
	$(LIBEXPIDUS1UI_CFLAGS)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

expidus1_session_logout_LDFLAGS =						\
	$(GIO_LIBS)							\
	$(LIBEXPIDUS1UI_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = expidus1-session-logout.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(man_MANS)							\
	$(desktop_in_files)

DISTCLEANFILES = 							\
	$(desktop_DATA)
