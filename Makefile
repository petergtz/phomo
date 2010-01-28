# CXXFLAGS =	-O2 -g -Wall -fmessage-length=0

OBJS =		Phomo.o

LIBS = -ljpeg -lboost_program_options -lboost_filesystem -lboost_thread -lexiv2

CXXFLAGS = -I/usr/include/ -Wall #-DPHOMO_TIMER
           

CXX = g++-4.3

TARGET =	phomo

all: release

release: CXXFLAGS += -O2
release: $(TARGET)

debug: CXXFLAGS += -g
debug: $(TARGET)

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
