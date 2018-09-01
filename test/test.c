#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <taihen.h>

#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>

#include "console.h"

#define SKPRX               "ux0:tai/io_logger.skprx"
#define perr(...)           do { console_set_color(RED); printf(__VA_ARGS__); console_set_color(WHITE); } while(0);

SceUID module_id = -1;

static bool module_load(void)
{
    module_id = taiLoadStartKernelModule(SKPRX, 0, NULL, 0);
    if (module_id < 0) {
        perr("Could not load kernel module: 0x%08X\n", module_id);
        return false;
    }
    return true;
}

static bool module_unload(void)
{
    if (module_id < 0)
        return false;
    int r = taiStopUnloadKernelModule(module_id, 0, NULL, 0, NULL, NULL);
    if (r < 0) {
        perr("Could not unload kernel module: 0x%08X\n", r);
        return false;
    }
    return true;
}

static bool test_open_file(char* path)
{
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0777);
    if (fd < 0) {
        perr("Could not open '%s': 0x%08X\n", path, fd);
        return false;
    }
    sceIoClose(fd);
    return true;
}

#define DISPLAY_TEST(msg, func, ...) \
    r = func(__VA_ARGS__); \
    console_set_color(r ? GREEN : RED); printf(r ? "[PASS] " : "[FAIL] "); console_set_color(WHITE); printf("%s\n", msg)

#define DISPLAY_TEST_OR_OUT(msg, func, ...) \
    DISPLAY_TEST(msg, func, __VA_ARGS__); \
    if (!r) goto out

int main()
{
    int r = -1;
    SceCtrlData pad;

    init_video();
    console_init();

    printf("I/O logger test\n\n");
    DISPLAY_TEST_OR_OUT("Logger module can be loaded", module_load);
    DISPLAY_TEST("Open 'eboot.bin'", test_open_file, "ux0:app/IO_LOGGER/eboot.bin");
    DISPLAY_TEST("Logger module can be unloaded", module_unload);

out:
    console_set_color(CYAN);
    printf("\nPress X to exit.\n");
    do {
        sceCtrlPeekBufferPositive(0, &pad, 1);
    } while (!(pad.buttons & SCE_CTRL_CROSS));
    console_exit();
    end_video();
    sceKernelExitProcess(0);
    return 0;
}
