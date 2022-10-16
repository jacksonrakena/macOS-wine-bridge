struct sockaddr_un
{
    unsigned short sun_family; /* AF_UNIX */
    char sun_path[108];        /* pathname */
};

// #define AF_UNIX     0x0001
// #define SOCK_STREAM 0x0001
#define F_SETFL 0x0004
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_CREAT 0x0200
#define O_APPEND 0x0008
#define O_NONBLOCK 0x0004
#define BUFSIZE 2048 // size of read/write buffers

__declspec(naked) void __syscall()
{
    __asm__(
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
        "ret");
}

// technocoder: sysv_abi must be used for x86_64 unix system calls
__declspec(naked) __attribute__((sysv_abi)) unsigned int l_getpid()
{
    __asm__(
        "mov eax, 0x14\n\t"
        "jmp __syscall\n\t"
        "ret");
}
__declspec(naked) __attribute__((sysv_abi)) int l_close(int fd)
{
    __asm__(
        "mov eax, 0x06\n\t"
        "jmp __syscall\n\t"
        "ret");
}
__declspec(naked) __attribute__((sysv_abi)) int l_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
    __asm__(
        "mov eax, 0x5c\n\t"
        "jmp __syscall\n\t"
        "ret");
}
__declspec(naked) __attribute__((sysv_abi)) int l_open(const char *filename, int flags, int mode)
{
    __asm__(
        "mov eax, 0x05\n\t"
        "jmp __syscall\n\t"
        "ret");
}
__declspec(naked) __attribute__((sysv_abi)) int l_write(unsigned int fd, const char *buf, unsigned int count)
{
    __asm__(
        "mov eax, 0x04\n\t"
        "jmp __syscall\n\t"
        "ret");
}
__declspec(naked) __attribute__((sysv_abi)) int l_read(unsigned int fd, char *buf, unsigned int count)
{
    __asm__(
        "mov eax, 0x03\n\t"
        "jmp __syscall\n\t"
        "ret");
}
__declspec(naked) __attribute__((sysv_abi)) int l_socket(int domain, int type, int protocol)
{
    __asm__(
        "mov eax, 0x61\n\t"
        "jmp __syscall\n\t"
        "ret");
}
__declspec(naked) __attribute__((sysv_abi)) int l_connect(int sockfd, const struct sockaddr *addr, unsigned int addrlen)
{
    __asm__(
        "mov eax, 0x62\n\t"
        "jmp __syscall\n\t"
        "ret");
}

static const char *get_temp_path()
{
    const char *temp = getenv("TMPDIR");
    temp = temp ? temp : "/tmp";
    return temp;
}