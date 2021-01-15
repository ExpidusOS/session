
AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libessm                                         \
	-DG_LOG_DOMAIN=\"expidus1-session\"				\
	-DLIBDIR=\"$(libdir)\"						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DESSM_SHUTDOWN_HELPER_CMD=\"$(HELPER_PATH_PREFIX)/expidus1/session/essm-shutdown-helper\" \
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE					\
	-DUPOWER_ENABLE_DEPRECATED 						\
	$(PLATFORM_CPPFLAGS)

man_MANS = expidus1-session.1

bin_PROGRAMS = expidus1-session

expidus1_session_built_sources =						\
	essm-chooser-icon.h						\
	essm-client-dbus.c						\
	essm-client-dbus.h						\
	essm-manager-dbus.c						\
	essm-manager-dbus.h						\
	essm-marshal.c							\
	essm-marshal.h

expidus1_session_SOURCES =							\
	$(expidus1_session_built_sources)					\
	ice-layer.c							\
	ice-layer.h							\
	main.c								\
	sm-layer.c							\
	sm-layer.h							\
	essm-chooser.c							\
	essm-chooser.h							\
	essm-client.c							\
	essm-client.h							\
	essm-client-dbus.h						\
	essm-compat-gnome.c						\
	essm-compat-gnome.h						\
	essm-compat-kde.c						\
	essm-compat-kde.h						\
	essm-consolekit.c						\
	essm-consolekit.h						\
	essm-dns.c							\
	essm-dns.h							\
	essm-error.c							\
	essm-error.h							\
	essm-fadeout.c							\
	essm-fadeout.h							\
	essm-global.c							\
	essm-global.h							\
	essm-legacy.c							\
	essm-legacy.h							\
	essm-logout-dialog.c						\
	essm-logout-dialog.h						\
	essm-manager.c							\
	essm-manager.h							\
	essm-properties.c						\
	essm-properties.h						\
	essm-shutdown-fallback.c				\
	essm-shutdown-fallback.h				\
	essm-shutdown.c							\
	essm-shutdown.h							\
	essm-startup.c							\
	essm-startup.h							\
	expidus-screensaver.c						\
	expidus-screensaver.h						\
	essm-systemd.c							\
	essm-systemd.h


expidus1_session_CFLAGS =							\
	$(LIBSM_CFLAGS)							\
	$(LIBX11_CFLAGS)						\
	$(LIBEXPIDUS1UI_CFLAGS)						\
	$(GIO_CFLAGS)							\
	$(LIBWNCK_CFLAGS)						\
	$(POLKIT_CFLAGS)						\
	$(ESCONF_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(PLATFORM_CFLAGS)

expidus1_session_LDFLAGS =							\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

expidus1_session_LDADD =							\
	$(top_builddir)/libessm/libessm.la				\
	$(LIBSM_LDFLAGS)						\
	$(LIBSM_LIBS)							\
	$(LIBX11_LDFLAGS)						\
	$(LIBX11_LIBS)							\
	$(LIBEXPIDUS1UI_LIBS)						\
	$(GMODULE_LIBS)							\
	$(GIO_LIBS)								\
	$(LIBWNCK_LIBS)							\
	$(POLKIT_LIBS)							\
	$(ESCONF_LIBS)							\
	$(LIBM)

if HAVE_POLKIT

@INTLTOOL_POLICY_RULE@

polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_DATA =					\
	com.expidus.session.policy
.in2.in:
	sed "s|[@]HELPER_PATH_PREFIX@|${HELPER_PATH_PREFIX}|" $< > $@

endif

if MAINTAINER_MODE

essm-chooser-icon.h: $(srcdir)/essm-chooser-icon.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --raw --stream --name=essm_chooser_icon_data $< > $@

essm-marshal.h: $(srcdir)/essm-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=essm_marshal --internal --header $< > $@

essm-marshal.c: $(srcdir)/essm-marshal.list Makefile
	$(AM_V_GEN) echo "#include <expidus1-session/essm-marshal.h>" > $@ \
	&& glib-genmarshal --prefix=essm_marshal --body $< >> $@

essm-client-dbus.c essm-client-dbus.h : $(srcdir)/essm-client-dbus.xml Makefile.am
	gdbus-codegen \
		--c-namespace=EssmDbus \
		--interface-prefix=com.expidus.Session. \
		--generate-c-code=essm-client-dbus \
		$(srcdir)/essm-client-dbus.xml

essm-manager-dbus.c essm-manager-dbus.h : $(srcdir)/essm-manager-dbus.xml Makefile.am
	gdbus-codegen \
		--c-namespace=EssmDbus \
		--interface-prefix=com.expidus.Session. \
		--generate-c-code=essm-manager-dbus \
		$(srcdir)/essm-manager-dbus.xml

BUILT_SOURCES =								\
	$(expidus1_session_built_sources)

endif

EXTRA_DIST =								\
	$(man_MANS)							\
	essm-chooser-icon.png						\
	essm-marshal.list						\
	essm-client-dbus.xml						\
	essm-manager-dbus.xml					\
	com.expidus.session.policy.in2

DISTCLEANFILES =							\
	$(BUILT_SOURCES)				\
	com.expidus.session.policy					\
	com.expidus.session.policy.in

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
