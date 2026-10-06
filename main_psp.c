/*
 * PSP platform layer for Forgeworks.
 * The game renders into a RAM buffer which is copied to VRAM each frame
 * (double-buffered). Saves go to ms0:/PSP/SAVEDATA/FORGEWORKS/.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspge.h>
#include <string.h>
#include <stdio.h>

#include "common.h"

PSP_MODULE_INFO("Forgeworks", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define SAVE_ROOT "ms0:/PSP/SAVEDATA"
#define SAVE_DIR SAVE_ROOT "/FORGEWORKS"
#define FRAME_PIXELS (FB_STRIDE * SCREEN_H)

static volatile int running = 1;
static u32 backbuf[FRAME_PIXELS] __attribute__((aligned(64)));

/* ---------------------------------------------------------------- HOME button / exit */
static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    running = 0;
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

/* ---------------------------------------------------------------- saving */
static void save_path(char *out, int n, const char *name)
{
    snprintf(out, n, "%s/%s", SAVE_DIR, name);
}

int plat_save(const char *name, const void *data, int size)
{
    char path[128];
    sceIoMkdir(SAVE_ROOT, 0777);
    sceIoMkdir(SAVE_DIR, 0777);
    save_path(path, sizeof(path), name);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return 0;
    int written = sceIoWrite(fd, data, size);
    sceIoClose(fd);
    return written == size;
}

int plat_load(const char *name, void *data, int size)
{
    char path[128];
    save_path(path, sizeof(path), name);
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) return 0;
    int got = sceIoRead(fd, data, size);
    sceIoClose(fd);
    return got == size;
}

void plat_delete(const char *name)
{
    char path[128];
    save_path(path, sizeof(path), name);
    sceIoRemove(path);
}

/* ---------------------------------------------------------------- input */
static u32 map_buttons(unsigned int b)
{
    u32 o = 0;
    if (b & PSP_CTRL_UP) o |= BTN_UP;
    if (b & PSP_CTRL_DOWN) o |= BTN_DOWN;
    if (b & PSP_CTRL_LEFT) o |= BTN_LEFT;
    if (b & PSP_CTRL_RIGHT) o |= BTN_RIGHT;
    if (b & PSP_CTRL_CROSS) o |= BTN_CROSS;
    if (b & PSP_CTRL_CIRCLE) o |= BTN_CIRCLE;
    if (b & PSP_CTRL_SQUARE) o |= BTN_SQUARE;
    if (b & PSP_CTRL_TRIANGLE) o |= BTN_TRIANGLE;
    if (b & PSP_CTRL_LTRIGGER) o |= BTN_L;
    if (b & PSP_CTRL_RTRIGGER) o |= BTN_R;
    if (b & PSP_CTRL_START) o |= BTN_START;
    if (b & PSP_CTRL_SELECT) o |= BTN_SELECT;
    return o;
}

/* ---------------------------------------------------------------- main */
int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    setup_callbacks();
    scePowerSetClockFrequency(333, 333, 166);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    /* uncached view of VRAM, two framebuffers back to back */
    u32 *vram = (u32 *)(uintptr_t)(0x40000000u | (u32)(uintptr_t)sceGeEdramGetAddr());
    sceDisplaySetMode(0, SCREEN_W, SCREEN_H);
    memset(vram, 0, FRAME_PIXELS * 4 * 2);
    sceDisplaySetFrameBuf(vram, FB_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);

    game_init();
    int draw = 1;
    while (running) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        game_frame(map_buttons(pad.Buttons), (int)pad.Lx - 128, (int)pad.Ly - 128, backbuf);

        u32 *dst = vram + draw * FRAME_PIXELS;
        memcpy(dst, backbuf, FRAME_PIXELS * 4);
        sceDisplayWaitVblankStart();
        sceDisplaySetFrameBuf(dst, FB_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
        draw ^= 1;
    }
    sceKernelExitGame();
    return 0;
}
