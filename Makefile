CC = x86_64-w64-mingw32-gcc

CFLAGS = -O2 -Wall

default: all
all: bridge.exe

bridge.exe: main.c
	$(CC) -masm=intel -mwindows -o $@ $(CFLAGS) $<
xiv_bridge: main.c
	$(CC) -masm=intel -mwindows -o "/Applications/XIV on Mac.app/Contents/Resources/discord_bridge.exe" $(CFLAGS) $<

clean:
	rm -v bridge.exe
