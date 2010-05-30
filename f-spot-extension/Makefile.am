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

all: Phomo.dll

Phomo.dll: Phomo.cs Phomo.glade Phomo.addin.xml
	gmcs -debug -target:library $(SOURCES) $(PACKAGES) $(ASSEMBLIES) $(RESOURCES)

clean-local:
	rm -f *.dll *~ *.bak .mpack *.gladep
	
PHONY:
	install clean-local all mpack
