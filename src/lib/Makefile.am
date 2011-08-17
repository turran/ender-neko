# lets force for now the destination path until we find a way to get the installed neko path
bindingsdir = /usr/lib/neko/
bindings_LTLIBRARIES = ender.la

ender_la_SOURCES = ender.c
ender_la_LIBADD = @NEKO_LIBS@ @ESCEN_LIBS@
ender_la_LDFLAGS = -no-undefined -module -avoid-version
ender_la_CFLAGS = @ESCEN_CFLAGS@
module_la_LIBTOOLFLAGS = --tag=disable-static

install-data-hook:
	mv -f $(bindingsdir)ender.so $(bindingsdir)ender.ndll

uninstall-hook:
	rm $(bindingsdir)ender.ndll

