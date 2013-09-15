## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src po

ezinstalldocdir = ${prefix}/doc/ezinstall
ezinstalldoc_DATA = \
	README.md\
	LICENSE\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS
	
ezinstallcssdir = ${prefix}/public_html
ezinstallcss_DATA = ezinstall.css\
                    ezinstall.js

ezinstallcfgdir = ${prefix}/public_html/cgi-bin
ezinstallcfg_DATA = ezinstall.xml

INTLTOOL_FILES = intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

EXTRA_DIST = $(ezinstalldoc_DATA) $(ezinstallcss_DATA) $(ezinstallcfg_DATA) $(INTLTOOL_FILES)

DISTCLEANFILES = intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache

# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(ezinstalldocdir)
