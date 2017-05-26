INC_DIR= /Users/ZhangZhiyi/Develop/Tools-Libs/ARToolKit5-MACOS/include
LIB_DIR= /Users/ZhangZhiyi/Develop/Tools-Libs/ARToolKit5-MACOS/lib

CC=cc
LDFLAG= -L$(LIB_DIR)/macosx-universal -L$(LIB_DIR)
LIBS= -lARgsub_lite -lARvideo -lAR -lARICP -lAR -framework Accelerate -framework QTKit -framework CoreVideo -framework Carbon -framework GLUT -framework OpenGL -framework Cocoa -ljpeg
CFLAG= -O -DHAVE_NFT=1 -I$(INC_DIR)

OBJS =
HEADERS =

all: simpleLite

simpleLite: simpleLite.o $(OBJS)
	cc -o simpleLite simpleLite.o $(OBJS) $(LDFLAG) $(LIBS)

simpleLite.o: simpleLite.c $(HEADERS)
	cc -c $(CFLAG) simpleLite.c

clean:
	rm -f *.o
	rm -f simpleLite

allclean:
	rm -f *.o
	rm -f simpleLite
