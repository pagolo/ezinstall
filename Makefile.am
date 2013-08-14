## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src

ezinstalldocdir = ${prefix}/doc/ezinstall
ezinstalldoc_DATA = \
	README.md\
	LICENSE\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS
	
ezinstallcssdir = ${prefix}/public_html
ezinstallcss_DATA = ezinstall.css

EXTRA_DIST = $(ezinstalldoc_DATA) $(ezinstallcss_DATA)



AM_CFLAGS = -static 

AM_LDFLAGS = -static 
