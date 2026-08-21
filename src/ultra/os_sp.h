/*
 * os_sp.h — RSP task dispatch. See os_sp.c for the reasoning.
 *
 * The port registers one handler per microcode kind at startup. The shim never
 * names RT64 or the audio interpreter itself: it is libultra's replacement, and
 * libultra does not know what a renderer is.
 */
#ifndef GE_ULTRA_OS_SP_H
#define GE_ULTRA_OS_SP_H

#ifdef __cplusplus
extern "C" {
#endif

struct OSTask_s;

typedef void (*GeSpTaskHandler)(struct OSTask_s *task, void *user);

void geSpSetGraphicsHandler(GeSpTaskHandler fn, void *user);
void geSpSetAudioHandler(GeSpTaskHandler fn, void *user);

/*
 * Task counts since startup: graphics, audio, and tasks whose type was not
 * recognised. The third is the interesting one -- it should be zero, and if it
 * is not, something is building tasks this port does not understand.
 */
void geSpGetCounts(unsigned int *gfx, unsigned int *audio, unsigned int *unknown);

/*
 * Deliver an OS event (OS_EVENT_SP / OS_EVENT_DP) from C. os_sp.c needs this to
 * tell the game's scheduler that a task finished; without it the scheduler
 * believes the RSP never became free again.
 */
int geSpSendEventC(int event);

/*
 * A task's display-list address and length, for code that cannot see OSTask's
 * layout. See the definition in os_sp.c.
 */
void geSpTaskDisplayList(struct OSTask_s *task, unsigned int *dl,
                         unsigned int *len);

#ifdef __cplusplus
}
#endif

#endif /* GE_ULTRA_OS_SP_H */
