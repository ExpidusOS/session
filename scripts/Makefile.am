bin_SCRIPTS= \
	startexpidus1 \
	eslock1

# cannot be set from configure, because ${prefix} is not expanded
edit = $(SED) \
	-e "s,@_datadir_\@,$(datadir),g" \
	-e "s,@_libdir_\@,$(libdir),g" \
	-e "s,@_docdir_\@,$$(dirname $(docdir)),g" \
	-e "s,@_sysconfdir_\@,$(sysconfdir),g"

startexpidus1: Makefile $(srcdir)/startexpidus1.in
	rm -f startexpidus1 startexpidus1.tmp
	$(edit) $(srcdir)/startexpidus1.in >startexpidus1.tmp
	mv startexpidus1.tmp startexpidus1

xinitrc: Makefile xinitrc.in
	rm -f xinitrc xinitrc.tmp
	$(edit) xinitrc.in >xinitrc.tmp
	mv xinitrc.tmp xinitrc

conffilesdir=@sysconfdir@/xdg/expidus1
conffiles_DATA = \
	xinitrc \
	Xft.xrdb

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = xscreensaver.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(autostart_in_files) \
	startexpidus1.in \
	xinitrc.in \
	Xft.xrdb \
	eslock1

DISTCLEANFILES = \
	$(autostart_DATA) \
	startexpidus1 \
	xinitrc
