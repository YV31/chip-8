CC=gcc
LIBS=`pkg-config --libs sdl2`
CFLAGS=-Wall -Wextra -g `pkg-config --cflags sdl2` -O2
OBJS=main.o chip8.o
BIN=chip-8

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS) -I.

clean:
	rm -rf $(BIN) $(OBJS)
