## Process this file with automake to produce Makefile.in

SUBDIRS = src po pixmaps

EXTRA_DIST = \
	autogen.sh \
	searchmonkey-0-8-2.glade \
	searchmonkey.desktop \
	pixmaps/* \
	rpmbuild/* \
	bsdports/*

desktopdir = $(datadir)/applications
desktop_in_files = searchmonkey.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@
