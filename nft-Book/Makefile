UNAME = $(shell uname)

YG_AR_HOME = /Users/MAXWELL/Desktop/CS219/ARToolKit5OSX
ZZ_AR_HOME = /Users/ZhangZhiyi/Develop/Tools-Libs/ARToolKit5-MACOS
AR_HOME = $(ZZ_AR_HOME)

AR_CPPFLAGS = -I$(AR_HOME)/include/macosx-universal -I$(AR_HOME)/include
AR_LDFLAGS = -L$(AR_HOME)/lib/macosx-universal -L$(AR_HOME)/lib

OSG_LIBPATH = /usr/lib
OSG_LIBS = -losg -lOpenThreads -losgDB -losgFX -losgGA -losgViewer -losgUtil
ifeq "$(UNAME)" "Darwin"
    OSG_LIBPATH = /Library/frameworks
    OSG_LIBS = -framework osg -framework OpenThreads -framework osgDB -framework osgFX -framework osgGA -framework osgViewer -framework osgUtil -lstdc++
endif
OSG_CPPFLAGS =
OSG_LDFLAGS = -L$(OSG_LIBPATH)

CC=cc
CXX=c++
CPPFLAGS = $(AR_CPPFLAGS) $(OSG_CPPFLAGS)
CFLAGS = -O -DHAVE_NFT=1
CXXFLAGS = -O -DHAVE_NFT=1
LDFLAGS = $(AR_LDFLAGS) $(OSG_LDFLAGS)
LIBS = -lKPM -lAR2 -lARUtil -lARosg -lARgsub_lite -lARvideo -lAR -lARICP -lAR \
    $(OSG_LIBS) -framework Accelerate -framework QTKit -framework CoreVideo -framework Carbon -framework GLUT -framework OpenGL -framework Cocoa -ljpeg
AR=ar
ARFLAGS=-r -u
RANLIB = true

TARGET = nftBook

HEADERS = \
    trackingSub.h \
    VirtualEnvironment.h

OBJS = \
    nftBook.o \
    ARMarkerNFT.o \
    trackingSub.o \
    VirtualEnvironment.o

default build all: $(TARGET)

$(OBJS) : $(HEADERS)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm -f *.o *~ *.bak
	-rm $(TARGET)

allclean:
	-rm -f *.o *~ *.bak
	-rm $(TARGET)
