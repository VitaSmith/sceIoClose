#include <psp2kern/types.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/io/fcntl.h>

#include <stdio.h>
#include <string.h>
#include <vitasdkkern.h>
#include <taihen.h>

extern int module_get_export_func(SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, uintptr_t *func);

static char log_msg[256];

static void log_print(const char* msg)
{
    SceUID fd = ksceIoOpen("ux0:data/io.log", SCE_O_CREAT | SCE_O_APPEND | SCE_O_WRONLY, 0777);

    if (fd >= 0) {
        ksceIoWrite(fd, msg, strlen(msg));
        ksceIoClose(fd);
    }
}

#define printf(...) do { snprintf(log_msg, sizeof(log_msg), __VA_ARGS__); log_print(log_msg); } while(0)
#define perr        printf

// Hooks
static tai_hook_ref_t open_ref;
static tai_hook_ref_t close_ref;
static SceUID         open_id = -1;
static SceUID         close_id = -1;

SceUID hook_user_open(const char *path, int flags, SceMode mode, void *args)
{
    int state;
    ENTER_SYSCALL(state);
    char k_path[256];
    SceUID fd = TAI_CONTINUE(SceUID, open_ref, path, flags, mode, args);
    // Copy the user pointer to kernel space
    ksceKernelStrncpyUserToKernel(k_path, (uintptr_t)path, 256);
    // Log our call
    printf("sceIoOpen('%s') = 0x%08X\n", k_path, fd);
    EXIT_SYSCALL(state);
    return fd;
}

int hook_user_close(SceUID fd)
{
    int state;
    ENTER_SYSCALL(state);
    // The call to TAI_CONTINUE() below will crash the system!
    // A workaround is to replace that call with the line commented below:
    // int r = ksceIoClose(ksceKernelKernelUidForUserUid(ksceKernelGetProcessId(), fd));
    int r = TAI_CONTINUE(int, close_ref, fd);
    printf("sceIoClose(0x%08X) = 0x%08X\n", fd, r);
    EXIT_SYSCALL(state);
    return r;
}

// plugin entry
void _start() __attribute__((weak, alias("module_start")));
int module_start(SceSize argc, const void *args)
{
    printf("Loading IO logger kernel driver...\n");

    // sceIoOpen Hook
    open_id = taiHookFunctionExportForKernel(KERNEL_PID, &open_ref,
                                   "SceIofilemgr", TAI_ANY_LIBRARY,
                                   0xCC67B6FD, hook_user_open);
    printf("- sceIoOpen Hook ID: 0x%08X\n", open_id);

    // sceIoClose Hook
    close_id = taiHookFunctionExportForKernel(KERNEL_PID, &close_ref,
                                   "SceIofilemgr", TAI_ANY_LIBRARY,
                                   0xC70B8886, hook_user_close);
    printf("- sceIoClose Hook ID: 0x%08X\n", close_id);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
    printf("Unloading IO logger kernel driver...\n");
    if (close_id > 0)
        taiHookReleaseForKernel(close_id, close_ref);
    if (open_id > 0)
        taiHookReleaseForKernel(open_id, open_ref);
    return SCE_KERNEL_STOP_SUCCESS;
}
