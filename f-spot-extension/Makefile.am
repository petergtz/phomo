fspotdir = $(libdir)/f-spot/extensions
fspot_DATA = Phomo.dll	
SOURCES = \
	Phomo.cs

	
PACKAGES = \
	-pkg:f-spot \
	-pkg:gtk-sharp-2.0 \
	-pkg:glade-sharp-2.0

ASSEMBLIES = -r:Mono.Posix \
	-r:/usr/lib/f-spot/gtk-sharp-beans.dll

RESOURCES = \
	-resource:Phomo.glade \
	-resource:Phomo.addin.xml

Phomo.dll: Phomo.cs Phomo.glade Phomo.addin.xml
	gmcs -debug -target:library $(SOURCES) $(PACKAGES) $(ASSEMBLIES) $(RESOURCES)


CLEANFILES = *.dll *~ *.bak .mpack *.gladep
DISTCLEANFILES = Makefile
EXTRA_DIST = Phomo.cs Phomo.glade Phomo.addin.xml

	

