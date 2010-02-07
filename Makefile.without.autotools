OBJS =		Phomo.o

LIBS = -ljpeg -lboost_program_options -lboost_filesystem -lboost_thread -lexiv2 
#    -lpthread -lboost_system -lexpat -lpng -lz

CXXFLAGS = -I/usr/include/ -Wall #-DPHOMO_TIMER
#LDFLAGS = -static
           

CXX = g++-4.3

TARGET =	phomo

all: release

release: CXXFLAGS += -O2
release: $(TARGET)

debug: CXXFLAGS += -g
debug: $(TARGET)

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS) $(LDFLAGS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/bin

uninstall:
	rm /usr/bin/$(TARGET)
