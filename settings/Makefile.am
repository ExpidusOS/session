bin_PROGRAMS = expidus1-session-settings

expidus1_session_settings_SOURCES = \
	main.c \
	session-editor.c \
	esae-dialog.c \
	esae-dialog.h \
	esae-model.c \
	esae-model.h \
	esae-window.c \
	esae-window.h \
	expidus1-session-marshal.c \
	expidus1-session-marshal.h \
	expidus1-session-settings-common.h \
	expidus1-session-settings_ui.h \
	essm-client-dbus-client.h \
	essm-client-dbus-client.c \
	essm-manager-dbus-client.h \
	essm-manager-dbus-client.c

expidus1_session_settings_CFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"expidus1-session-settings\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libessm \
	$(LIBEXPIDUS1UTIL_CFLAGS) \
	$(LIBEXPIDUS1UI_CFLAGS) \
	$(ESCONF_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GMODULE_CFLAGS)

expidus1_session_settings_LDADD = \
	$(top_builddir)/libessm/libessm.la \
	$(LIBEXPIDUS1UTIL_CFLAGS) \
	$(LIBEXPIDUS1UI_LIBS) \
	$(ESCONF_LIBS) \
	$(GIO_LIBS) \
	$(GMODULE_LIBS)

settingsdir = $(sysconfdir)/xdg/expidus1/esconf/expidus-perchannel-xml
settings_DATA = expidus1-session.xml

desktopdir = $(datadir)/applications
desktop_in_files = expidus-session-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@


if MAINTAINER_MODE

BUILT_SOURCES = \
	expidus1-session-marshal.c \
	expidus1-session-marshal.h \
	expidus1-session-settings_ui.h \
	essm-client-dbus-client.h \
	essm-client-dbus-client.c \
	essm-manager-dbus-client.h \
	essm-manager-dbus-client.c

expidus1-session-marshal.h: stamp-expidus1-session-marshal.h
	@true
stamp-expidus1-session-marshal.h: expidus1-session-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=expidus1_session_marshal expidus1-session-marshal.list --header > expidus1-session-marshal.h
	echo timestamp > $(@F)
expidus1-session-marshal.c: expidus1-session-marshal.list Makefile
	$(AM_V_GEN) echo '#include "expidus1-session-marshal.h"' > $@
	glib-genmarshal --prefix=expidus1_session_marshal expidus1-session-marshal.list --body >> expidus1-session-marshal.c

expidus1-session-settings_ui.h: expidus1-session-settings.ui
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=expidus1_session_settings_ui $< >$@

essm-client-dbus-client.c essm-client-dbus-client.h : $(top_srcdir)/expidus1-session/essm-client-dbus.xml Makefile.am
	gdbus-codegen \
		--c-namespace=Essm \
		--interface-prefix=com.expidus.Session. \
		--generate-c-code=essm-client-dbus-client \
		$(top_srcdir)/expidus1-session/essm-client-dbus.xml

essm-manager-dbus-client.c essm-manager-dbus-client.h : $(top_srcdir)/expidus1-session/essm-manager-dbus.xml Makefile.am
	gdbus-codegen \
		--c-namespace=Essm \
		--interface-prefix=com.expidus.Session. \
		--generate-c-code=essm-manager-dbus-client \
		$(top_srcdir)/expidus1-session/essm-manager-dbus.xml

endif


EXTRA_DIST = \
	$(desktop_in_files) \
	expidus1-session.xml \
	expidus1-session-settings.ui

DISTCLEANFILES = \
	$(desktop_DATA)
