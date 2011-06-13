AUTOMAKE_OPTIONS = foreign

pkgdir = /usr/lib/
ACLOCAL_AMFLAGS = -I m4

pkg_LTLIBRARIES = antiLeech.la
antiLeech_la_CPPFLAGS = ${ANTILEECH_CPPFLAGS}
antiLeech_la_LDFLAGS = -module -avoid-version --no-la-files
antiLeech_la_SOURCES = \
	antiLeech.h \
	antiLeech_wx.h \
	CString_wx.h \
	antiLeech.cpp \
	antiLeech_wx.cpp \
	Interface.cpp

