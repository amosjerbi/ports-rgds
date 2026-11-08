/* Speech balloons (CD release): the floppy's text bubble, drawn over the
 * finished frame while a subtitle is current. */
#ifndef SCI_BALLOON_H
#define SCI_BALLOON_H
#include <stdint.h>

/* kgraphics reports each view cel drawn (screen rect) to find the portrait. */
void     balloon_cel_drawn(int view, int x, int y, int w, int h);
/* Changes whenever the balloon must be redrawn or removed; 0 = none. */
uint32_t balloon_signature(uint32_t now);
/* Draw the balloon into a 320x200 copy of the screen; 1 if drawn. */
int      balloon_overlay(uint8_t *frame, uint32_t now);

#endif
