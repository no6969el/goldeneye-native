/*
 * os_sp.c — where the game hands work to the RSP, and therefore where the port
 * takes it away.
 *
 * This is the most load-bearing file in the shim. Everything else moves bytes
 * around; this decides what happens to a frame.
 *
 * On N64 the game builds an OSTask and calls osSpTaskLoad/osSpTaskStartGo. The
 * RSP then runs one of two microcode programs:
 *
 *   graphics — gsp3D, the game's F3D dialect. Walks a display list, transforms
 *              vertices, and feeds the RDP. In the port this becomes the
 *              display-list interpreter and RT64.
 *   audio    — aspMain. Runs the 16-command ABI over a command list. In the
 *              port this becomes src/ultra/audio/acmd_interp.
 *
 * WHY DISPATCH IS BY CALLBACK
 *
 * The shim must not know about RT64. It is libultra's replacement, and libultra
 * does not know what a renderer is. The port registers handlers at startup;
 * this file only decides WHICH handler a task belongs to, and says so out loud
 * when it cannot tell.
 *
 * HOW A TASK IS CLASSIFIED
 *
 * OSTask::t.type carries M_GFXTASK or M_AUDTASK. That field is set by the
 * gSPTask macros and the audio manager, so it is reliable — but it is also just
 * a number in a struct the game filled in, so an unrecognised value is reported
 * rather than guessed at. Guessing here would mean feeding an audio command
 * list to the display-list walker, which produces a spectacular and completely
 * misleading crash.
 */

#include <stdio.h>

#include <ultra64.h>

#include "os_sp.h"

static GeSpTaskHandler ge_gfx_handler   = NULL;
static GeSpTaskHandler ge_audio_handler = NULL;
static void           *ge_gfx_user      = NULL;
static void           *ge_audio_user    = NULL;

static u32 ge_gfx_tasks   = 0;
static u32 ge_audio_tasks = 0;
static u32 ge_unknown_tasks = 0;

/* The task most recently loaded but not yet started. */
static OSTask *ge_pending = NULL;

void geSpSetGraphicsHandler(GeSpTaskHandler fn, void *user)
{
    ge_gfx_handler = fn;
    ge_gfx_user    = user;
}

void geSpSetAudioHandler(GeSpTaskHandler fn, void *user)
{
    ge_audio_handler = fn;
    ge_audio_user    = user;
}

static void geSpRun(OSTask *task)
{
    if (task == NULL) {
        return;
    }

    switch (task->t.type) {
    case M_GFXTASK:
        ++ge_gfx_tasks;
        if (ge_gfx_handler != NULL) {
            ge_gfx_handler(task, ge_gfx_user);
        } else {
            static int once = 0;
            if (!once) {
                once = 1;
                fprintf(stderr, "[ge] graphics task with no handler registered "
                                "-- nothing will be drawn\n");
            }
        }
        break;

    case M_AUDTASK:
        ++ge_audio_tasks;
        if (ge_audio_handler != NULL) {
            ge_audio_handler(task, ge_audio_user);
        } else {
            static int once = 0;
            if (!once) {
                once = 1;
                fprintf(stderr, "[ge] audio task with no handler registered "
                                "-- silence\n");
            }
        }
        break;

    default: {
        /*
         * Deliberately not routed anywhere. An unknown task type means either
         * the game is doing something this port has not seen, or the OSTask was
         * built wrong -- and running it as graphics on a hunch would corrupt
         * RDRAM and crash somewhere unrelated.
         */
        static int once = 0;
        ++ge_unknown_tasks;
        if (!once) {
            once = 1;
            fprintf(stderr, "[ge] SP task of unknown type %u -- not dispatched\n",
                    (unsigned)task->t.type);
        }
        break;
    }
    }
}

/*
 * osSpTaskLoad stages the task; osSpTaskStartGo runs it. The game always calls
 * them in that order, and separating them matters: the ucode and its data are
 * expected to be resident before the task starts, and a port that ran the work
 * inside Load would execute a frame before the game finished describing it.
 */
void osSpTaskLoad(OSTask *task)
{
    ge_pending = task;
}

void osSpTaskStartGo(OSTask *task)
{
    /*
     * Prefer the argument. The pair is always called with the same pointer, but
     * trusting the staged one would hide a mismatch rather than tolerate it.
     */
    OSTask *run = (task != NULL) ? task : ge_pending;
    ge_pending = NULL;
    geSpRun(run);

    /*
     * SIGNAL COMPLETION. This was missing, and it is why the port ran 600
     * frames without ever drawing.
     *
     * On hardware the RSP raises an interrupt when a task finishes, libultra
     * turns that into OS_EVENT_SP, and src/sched.c has registered its own
     * interruptQ for it (sched.c:178). __scHandleRSP then clears
     * sc->curRSPTask, which is what lets __scSchedule dispatch the NEXT task.
     *
     * Here a task is a synchronous call, so by this line it has already
     * finished -- but nothing told the scheduler. It set curRSPTask on
     * dispatch and never cleared it, concluded the RSP was permanently busy,
     * and dispatched exactly one task for the life of the process. The
     * graphics task sat in cmdQ forever while the game happily built a new
     * display list every frame.
     *
     * OS_EVENT_DP as well when the task uses the RDP: the scheduler tracks
     * curRDPTask separately and gates OS_SC_LAST_TASK/SWAPBUFFER on it.
     */
    if (run != NULL) {
        const unsigned flags = run->t.flags;
        geSpSendEventC(4 /* OS_EVENT_SP */);
        if ((flags & OS_TASK_DP_WAIT) == 0) {
            geSpSendEventC(9 /* OS_EVENT_DP */);
        }
    }
}

/*
 * On hardware, yielding preempts a long graphics task so audio can run. Here a
 * task is a synchronous function call that has already completed by the time
 * anything could ask it to yield, so there is nothing to interrupt.
 *
 * osSpTaskYielded therefore reports "ran to completion", which is true, and is
 * the answer that makes the game's own scheduler take the correct branch --
 * claiming a yield would make it wait forever for a resume that never comes.
 */
void osSpTaskYield(void)
{
}

OSYieldResult osSpTaskYielded(OSTask *task)
{
    (void)task;
    /*
     * libultra returns OS_TASK_YIELDED (0x1) when the task actually yielded and
     * zero when it ran to completion. Here it always ran to completion, so zero
     * is the truthful answer -- and the one that makes the game's scheduler
     * take the right branch. Claiming a yield would leave it waiting forever
     * for a resume that never comes.
     */
    return 0;
}

void geSpGetCounts(u32 *gfx, u32 *audio, u32 *unknown)
{
    if (gfx != NULL)     { *gfx = ge_gfx_tasks; }
    if (audio != NULL)   { *audio = ge_audio_tasks; }
    if (unknown != NULL) { *unknown = ge_unknown_tasks; }
}


/*
 * Expose a task's display-list address and length.
 *
 * os_sp.c is compiled with the decomp's headers on the include path, so it can
 * see OSTask's layout; the C++ side of the port deliberately is not. Rather
 * than pull <PR/sptask.h> into C++ -- where it would collide with the shim's
 * own OS types, which is the drift tools/check_abi.sh exists to catch -- hand
 * out the two fields the probe needs.
 */
void geSpTaskDisplayList(struct OSTask_s *task, unsigned int *dl,
                         unsigned int *len)
{
    if (task == 0) {
        if (dl)  *dl = 0;
        if (len) *len = 0;
        return;
    }
    if (dl)  *dl  = (unsigned int)(uintptr_t)((OSTask *)task)->t.data_ptr;
    if (len) *len = (unsigned int)((OSTask *)task)->t.data_size;
}
