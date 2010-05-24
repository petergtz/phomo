all: Phomo.dll

PACKAGES = \
	-pkg:f-spot \
	-pkg:gtk-sharp-2.0 \
	-pkg:glade-sharp-2.0

ASSEMBLIES = -r:Mono.Posix \
	-r:/usr/lib/f-spot/gtk-sharp-beans.dll

RESOURCES = \
	-resource:Phomo.glade \
	-resource:Phomo.addin.xml

SOURCES = \
	Phomo.cs

install: all
	mkdir -p ~/.config/f-spot/addins/
	cp *.dll ~/.config/f-spot/addins/

mpack: Phomo.dll
	mautil p Phomo.dll

%.dll: %.cs %.glade
	gmcs -debug -target:library $(SOURCES) $(PACKAGES) $(ASSEMBLIES) $(RESOURCES)

clean:
	rm -f *.dll *~ *.bak .mpack *.gladep

PHONY:
	install clean all mpack
