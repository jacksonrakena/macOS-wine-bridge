#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include <strsafe.h>

struct sockaddr_un {
    unsigned short sun_family;               /* AF_UNIX */
    char           sun_path[108];            /* pathname */
};

// #define AF_UNIX     0x0001
// #define SOCK_STREAM 0x0001
#define F_SETFL     0x0004
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_CREAT     0x0200
#define O_APPEND    0x0008
#define O_NONBLOCK  0x0004
#define BUFSIZE 2048 // size of read/write buffers

__declspec(naked) void __syscall() {
	__asm__ (
            "__syscall:\n\t"
            // "push rdi\n\t"
            // "push rsi\n\t"
            // "push rdx\n\t"
            // "push r10\n\t"
            // "push r8\n\t"
            // "push r9\n\t"

            "add rax, 0x2000000\n\t"
            // "mov rdi, [rsp]\n\t"
            // "mov rsi, [rsp + 4]\n\t"
            // "mov rdx, [rsp + 8]\n\t"
            // "mov r10, [rsp + 12]\n\t"
            // "mov r8, [rsp + 16]\n\t"
            // "mov r9, [rsp + 16]\n\t"

            "syscall\n\t"
            "jnc noerror\n\t"
            "neg rax\n\t"
            "noerror:\n\t"

            // "pop r9\n\t"
            // "pop r8\n\t"
            // "pop r10\n\t"
            // "pop rdx\n\t"
            // "pop rsi\n\t"
            // "pop rdi\n\t"
            "ret"
            );
}

// technocoder: sysv_abi must be used for x86_64 unix system calls
__declspec(naked) __attribute__((sysv_abi)) unsigned int l_getpid() {
    __asm__ (
            "mov eax, 0x14\n\t"
            "jmp __syscall\n\t"
            "ret"
            );
}
__declspec(naked) __attribute__((sysv_abi)) int l_close(int fd) {
    __asm__ (
            "mov eax, 0x06\n\t"
            "jmp __syscall\n\t"
            "ret"
            );
}
__declspec(naked) __attribute__((sysv_abi)) int l_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg) {
    __asm__ (
            "mov eax, 0x5c\n\t"
            "jmp __syscall\n\t"
            "ret"
            );
}
__declspec(naked) __attribute__((sysv_abi)) int l_open(const char* filename, int flags, int mode) {
    __asm__ (
            "mov eax, 0x05\n\t"
            "jmp __syscall\n\t"
            "ret"
            );
}
__declspec(naked) __attribute__((sysv_abi)) int l_write(unsigned int fd, const char* buf, unsigned int count) {
    __asm__ (
            "mov eax, 0x04\n\t"
            "jmp __syscall\n\t"
            "ret"
            );
}
__declspec(naked) __attribute__((sysv_abi)) int l_read(unsigned int fd, char* buf, unsigned int count) {
    __asm__ (
            "mov eax, 0x03\n\t"
            "jmp __syscall\n\t"
            "ret"
            );
}
__declspec(naked) __attribute__((sysv_abi)) int l_socket(int domain, int type, int protocol) {
	__asm__ (
            "mov eax, 0x61\n\t"
            "jmp __syscall\n\t"
            "ret"
            );
}
__declspec(naked) __attribute__((sysv_abi)) int l_connect(int sockfd, const struct sockaddr *addr, unsigned int addrlen) {
	__asm__ (
            "mov eax, 0x62\n\t"
            "jmp __syscall\n\t"
            "ret"
            );
}

static const char* get_temp_path()
{
    const char* temp = getenv("TMPDIR");
    temp = temp ? temp : "/tmp";
    return temp;
}

static HANDLE hPipe = INVALID_HANDLE_VALUE;
static int sock_fd;
DWORD WINAPI winwrite_thread(LPVOID lpvParam);

// koukuno: This implemented because if no client ever connects but there are no more user
// wine processes running, this bridge can just exit gracefully.
static HANDLE conn_evt = INVALID_HANDLE_VALUE;
static BOOL fConnected = FALSE;

static HANDLE make_wine_system_process()
{
    HMODULE ntdll_mod;
    FARPROC proc;

    if ((ntdll_mod = GetModuleHandleW(L"NTDLL.DLL")) == NULL) {
        printf("Cannot find NTDLL.DLL in process map");
        return NULL;
    }


    proc = GetProcAddress(ntdll_mod, "NtSetInformationProcess");
    if (proc == NULL) {
        printf("Not a wine installation?");
        return NULL;
    }

    // This does NOT work.
    return ((HANDLE (CDECL *)(HANDLE, PROCESS_INFORMATION_CLASS))proc)(GetCurrentProcess(), 1000);
}

DWORD WINAPI wait_for_client(LPVOID param)
{
    (void)param;

    fConnected = ConnectNamedPipe(hPipe, NULL) ?
         TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

    SetEvent(conn_evt);
    return 0;
}

int main(void)
{
    DWORD  dwThreadId = 0;
    HANDLE hThread = NULL;
    HANDLE wine_evt = NULL;

//    if ((wine_evt = make_wine_system_process()) == NULL) {
//        return 1;
//    }

    // The main loop creates an instance of the named pipe and
    // then waits for a client to connect to it. When the client
    // connects, a thread is created to handle communications
    // with that client, and this loop is free to wait for the
    // next client connect request. It is an infinite loop.

    printf("Opening discord-ipc-0 Windows pipe\n");
    hPipe = CreateNamedPipeW(
            L"\\\\.\\pipe\\discord-ipc-0",             // pipe name
            PIPE_ACCESS_DUPLEX,       // read/write access
            PIPE_TYPE_BYTE |       // message type pipe
            PIPE_READMODE_BYTE |   // message-read mode
            PIPE_WAIT,                // blocking mode
            1, // max. instances
            BUFSIZE,                  // output buffer size
            BUFSIZE,                  // input buffer size
            0,                        // client time-out
            NULL);                    // default security attribute

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        printf("CreateNamedPipe failed, GLE=%lu.\n", GetLastError());
        return -1;
    }

    conn_evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    CloseHandle(CreateThread(NULL, 0, wait_for_client, NULL, 0, NULL));
    for (;;) {
        HANDLE events[] = { conn_evt };
        DWORD result = WaitForMultipleObjectsEx(1, events, FALSE, 0, FALSE);
        if (result == WAIT_TIMEOUT)
	    continue;

        if (result == 0) {
            printf("Bridge exiting, wine closing\n");
        }

        break;
    }

    if (fConnected)
    {
        printf("Client connected\n");

        printf("Creating socket\n");

        if ((sock_fd = l_socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            printf("Failed to create socket\n");
            return 1;
        }

        printf("Socket created\n");

        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;

        const char *const temp_path = get_temp_path();

        char connected = 0;
        for (int pipeNum = 0; pipeNum < 10; ++pipeNum) {

            snprintf(addr.sun_path, sizeof(addr.sun_path), "%sdiscord-ipc-%d", temp_path, pipeNum);
            printf("Attempting to connect to %s\n", addr.sun_path);

            if (l_connect(sock_fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
                printf("Failed to connect\n");
            } else {
                connected = 1;
                break;
            }
        }

        if (!connected) {
            printf("Could not connect to discord client\n");
            return 1;
        }


        printf("Connected successfully\n");

        hThread = CreateThread(
                NULL,              // no security attribute
                0,                 // default stack size
                winwrite_thread,    // thread proc
                (LPVOID) NULL,    // thread parameter
                0,                 // not suspended
                &dwThreadId);      // returns thread ID

        if (hThread == NULL)
        {
            printf("CreateThread failed, GLE=%lu.\n", GetLastError());
            return 1;
        }


        for (;;) {
            char buf[BUFSIZE];
            DWORD bytes_read = 0;
            BOOL fSuccess = ReadFile(
                    hPipe,        // handle to pipe
                    buf,    // buffer to receive data
                    BUFSIZE, // size of buffer
                    &bytes_read, // number of bytes read
                    NULL);        // not overlapped I/O
            if (!fSuccess) {
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    printf("winread EOF\n");
                    return 0;
                } else {
                    printf("Failed to read from pipe\n");
                    return 1;
                }
            }

            printf("%ld bytes w->l\n", bytes_read);
            /* uncomment to dump the actual data being passed from the pipe to the socket */
            /* for(int i=0;i<bytes_read;i++)putchar(buf[i]); */
            /* printf("\n"); */

            int total_written = 0, written = 0;

            while (total_written < bytes_read) {
                written = l_write(sock_fd, buf + total_written, bytes_read - total_written);
                if (written < 0) {
                    printf("Failed to write to socket\n");
                    return 1;
                }
                total_written += written;
                written = 0;
            }
        }
    }
    else
        // The client could not connect, so close the pipe.
        CloseHandle(hPipe);

    CloseHandle(wine_evt);
    CloseHandle(conn_evt);

    return 0;
}

DWORD WINAPI winwrite_thread(LPVOID lpvParam) {

    for (;;) {
        char buf[BUFSIZE];
        int bytes_read = l_read(sock_fd, buf, BUFSIZE);
        if (bytes_read < 0) {
            printf("Failed to read from socket\n");
            l_close(sock_fd);
            return 1;
        } else if (bytes_read == 0) {
            printf("EOF\n");
            break;
        }

        printf("%d bytes l->w\n", bytes_read);
        /* uncomment to dump the actual data being passed from the socket to the pipe */
        /* for(int i=0;i<bytes_read;i++)putchar(buf[i]); */
        /* printf("\n"); */

        DWORD total_written = 0, cbWritten = 0;

        while (total_written < bytes_read) {
            BOOL fSuccess = WriteFile(
                    hPipe,        // handle to pipe
                    buf + total_written,     // buffer to write from
                    bytes_read - total_written, // number of bytes to write
                    &cbWritten,   // number of bytes written
                    NULL);        // not overlapped I/O
            if (!fSuccess) {
                printf("Failed to write to pipe\n");
                return 1;
            }
            total_written += cbWritten;
            cbWritten = 0;
        }
    }
    return 1;
}
