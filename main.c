#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include <strsafe.h>
#include <Ntdef.h>

#include "asm.c"

static HANDLE hPipe = INVALID_HANDLE_VALUE;
static int sock_fd;
static DWORD WINAPI winwrite_thread(LPVOID lpvParam);
#define ProcessWineMakeProcessSystem 1000
NTSTATUS NTAPI NtSetInformationProcess(HANDLE, PROCESS_INFORMATION_CLASS, PVOID, ULONG);

// koukuno: This implemented because if no client ever connects but there are no more user
// wine processes running, this bridge can just exit gracefully.
static HANDLE conn_evt = INVALID_HANDLE_VALUE;
static BOOL fConnected = FALSE;


DWORD WINAPI wait_for_client(LPVOID param)
{
    (void)param;

    fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

    SetEvent(conn_evt);
    return 0;
}

int main(void)
{
    DWORD dwThreadId = 0;
    HANDLE process_info, hThread = NULL;
    NTSTATUS status;
        
    printf("\n");
    status = NtSetInformationProcess(GetCurrentProcess(), ProcessWineMakeProcessSystem, &process_info, sizeof(HANDLE *));

    if (NT_ERROR(status))
    {
        printf("Wine could not make this a system process");
        return 1;
    }

    // The main loop creates an instance of the named pipe and
    // then waits for a client to connect to it. When the client
    // connects, a thread is created to handle communications
    // with that client, and this loop is free to wait for the
    // next client connect request. It is an infinite loop.

    // Open the Windows pipe to connect to the IPC client.
    // This is the "fake" pipe for the RPC clients (XIV, osu!) to connect to.
    // This pipe will only even open on port 0 (discord-ipc-0).
main_loop: // This jump statement means we can jump back to the main loop after a client disconnects.
    CloseHandle(hPipe); // Close the named pipe, if it is open. We are going to make a new one. Kernel automatically deletes the pipe once the last handle is closed.
    printf("Opening discord-ipc-0 Windows pipe.\n");

    // Create the Windows named pipe.
    hPipe = CreateNamedPipeW(
        L"\\\\.\\pipe\\discord-ipc-0", // pipe name
        PIPE_ACCESS_DUPLEX,            // read/write access
        PIPE_TYPE_BYTE |               // message type pipe
            PIPE_READMODE_BYTE |       // message-read mode
            PIPE_WAIT,                 // blocking mode
        1,                             // max. instances
        BUFSIZE,                       // output buffer size
        BUFSIZE,                       // input buffer size
        0,                             // client time-out
        NULL);                         // default security attribute

    // This would usually happen when there's already another pipe open with the same name.
    // The user may have two instances of the bridge, or the bridge didn't cleanup (CloseHandle) properly.
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        printf("CreateNamedPipe failed, GLE=%lu.\n", GetLastError());
        printf("Do you have two bridges open at once?");
        return -1;
    }

    // Create the connection event, and reset it.
    conn_evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    ResetEvent(conn_evt);

    // Wait for an RPC client to connect to the Windows pipe.
    printf("Waiting for an RPC connection in Wine...\n");
    CloseHandle(CreateThread(NULL, TRUE, wait_for_client, NULL, 0, NULL));
    for (;;)
    {
        DWORD result = WaitForSingleObject(conn_evt, INFINITE);
        if (result == WAIT_TIMEOUT)
            continue;

        if (result == WAIT_OBJECT_0)
        {
            printf("Bridge exiting, wine closing\n");
        }

        break;
    }

    if (fConnected)
    {
        printf("Connected to Windows game. Opening UNIX socket.\n");

        if ((sock_fd = l_socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        {
            printf("Failed to create UNIX socket. Exiting.\n");
            return 1;
        }

        printf("Created UNIX socket successfully. Beginning passthrough.\n");

        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;

        const char *const temp_path = get_temp_path();

        char connected = 0;
        for (int pipeNum = 0; pipeNum < 10; ++pipeNum)
        {

            snprintf(addr.sun_path, sizeof(addr.sun_path), "%sdiscord-ipc-%d", temp_path, pipeNum);
            printf("[unix] Attempting to connect to %s\n", addr.sun_path);

            if (l_connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            {
                printf("[unix] Failed to connect to UNIX socket.\n");
            }
            else
            {
                connected = 1;
                break;
            }
        }

        if (!connected)
        {
            printf("Could not connect to Discord client. Exiting.\n");
            return 1;
        }

        printf("Connection established to Discord.\n");

        hThread = CreateThread(
            NULL,            // no security attribute
            0,               // default stack size
            winwrite_thread, // thread proc
            (LPVOID)NULL,    // thread parameter
            0,               // not suspended
            &dwThreadId);    // returns thread ID

        if (hThread == NULL)
        {
            printf("CreateThread failed, GLE=%lu.\n", GetLastError());
            return 1;
        }

        for (;;)
        {
            char buf[BUFSIZE];
            DWORD bytes_read = 0;
            BOOL fSuccess = ReadFile(
                hPipe,       // handle to pipe
                buf,         // buffer to receive data
                BUFSIZE,     // size of buffer
                &bytes_read, // number of bytes read
                NULL);       // not overlapped I/O
            if (!fSuccess)
            {
                if (GetLastError() == ERROR_BROKEN_PIPE)
                {
                    printf("Pipe broken. Restarting loop.\n");
                    goto main_loop;
                }
                else
                {
                    printf("Pipe read failed. Restarting loop.\n");
                    goto main_loop;
                }
            }

            printf("%ld bytes w->l\n", bytes_read);
            /* uncomment to dump the actual data being passed from the pipe to the socket */
            /* for(int i=0;i<bytes_read;i++)putchar(buf[i]); */
            /* printf("\n"); */

            int total_written = 0, written = 0;

            while (total_written < bytes_read)
            {
                written = l_write(sock_fd, buf + total_written, bytes_read - total_written);
                if (written < 0)
                {
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

    CloseHandle(conn_evt);

    return 0;
}

static DWORD WINAPI winwrite_thread(LPVOID lpvParam)
{

    for (;;)
    {
        char buf[BUFSIZE];
        int bytes_read = l_read(sock_fd, buf, BUFSIZE);
        if (bytes_read < 0)
        {
            printf("Failed to read from socket\n");
            l_close(sock_fd);
            return 1;
        }
        else if (bytes_read == 0)
        {
            printf("EOF\n");
            break;
        }

        printf("%d bytes l->w\n", bytes_read);
        /* uncomment to dump the actual data being passed from the socket to the pipe */
        /* for(int i=0;i<bytes_read;i++)putchar(buf[i]); */
        /* printf("\n"); */

        DWORD total_written = 0, cbWritten = 0;

        while (total_written < bytes_read)
        {
            BOOL fSuccess = WriteFile(
                hPipe,                      // handle to pipe
                buf + total_written,        // buffer to write from
                bytes_read - total_written, // number of bytes to write
                &cbWritten,                 // number of bytes written
                NULL);                      // not overlapped I/O
            if (!fSuccess)
            {
                printf("Failed to write to pipe\n");
                return 1;
            }
            total_written += cbWritten;
            cbWritten = 0;
        }
    }
    return 1;
}
