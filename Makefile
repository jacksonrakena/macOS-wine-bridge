CC = x86_64-w64-mingw32-gcc

CFLAGS = -O2 -Wall

default: all
all: bridge.exe

bridge.exe: main.c
	$(CC) -masm=intel -mwindows -o $@ $(CFLAGS) $< -lntdll

clean:
	rm -v bridge.exe
