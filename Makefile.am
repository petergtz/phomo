bin_PROGRAMS = phomo

phomo_SOURCES = Phomo.cpp

LIBS = -lboost_program_options -ljpeg -lexiv2 -lboost_thread -lboost_filesystem

# the following explicit version dependency is, because boost GIL 1.40 (using MPL) does 
# not compile with GCC 4.4
CXX=g++-4.3

AM_CXXFLAGS = -Wall
