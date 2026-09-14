/* tiling.h - scrollable tiling (niri-like) window layout, PoC
 *
 *  Window Maker window manager
 */

#ifndef WMTILING_H_
#define WMTILING_H_

#include "WindowMaker.h"

/* One column in the horizontal strip. For this PoC each column holds
 * exactly one window (no vertical stacking yet). */
typedef struct WTileColumn {
	struct WWindow *window;
	int width;			/* current column width in px */
	Bool locked;			/* True while window is maximized/fullscreen -
					 * wTileRelayout() skips this column's geometry
					 * entirely so WindowMaker's own maximize code
					 * keeps control of it. */
	struct WTileColumn *next;
	struct WTileColumn *prev;
} WTileColumn;

/* Per-workspace scroll-tiling state. */
typedef struct WTileStrip {
	WTileColumn *first;		/* leftmost column */
	WTileColumn *last;		/* rightmost column */
	WTileColumn *focused;		/* currently focused column, or NULL */
	int viewport_x;			/* horizontal scroll offset, PoC: unused for now */
	Bool enabled;			/* is scroll-tiling active for this workspace */
} WTileStrip;

/* Create an empty, disabled strip. Called from wWorkspaceMake()/wWorkspaceNew(). */
WTileStrip *wTileStripCreate(void);

/* Free a strip and all its columns. Called from wWorkspaceDelete(). */
void wTileStripDestroy(WTileStrip *strip);


void wTileAddWindow(WTileStrip *strip, struct WWindow *wwin, WArea usableArea,
                    int *x_ret, int *y_ret, unsigned int *width_ret, unsigned int *height_ret);

/* Remove wwin's column from the strip (if present) and relayout the
 * remaining columns' positions (their widths are left untouched -
 * niri-style: closing a window never resizes its neighbours). */
void wTileRemoveWindow(WTileStrip *strip, struct WWindow *wwin, WArea usableArea);


/* Recompute x-position of every column from `first` onward and apply
 * it via wWindowConfigure(). Widths are taken from each column's
 * `width` field, heights always fill usableArea. */
void wTileRelayout(WTileStrip *strip, WArea usableArea);


/* Scroll the strip's viewport by delta_x pixels (negative = scroll left).
 * Blocking, step-based animation (ScrollWM-style), no timer/event-loop
 * integration needed. */
void wTileScrollBy(WTileStrip *strip, int delta_x, WArea usableArea);
void wTileScrollToNextColumn(WTileStrip *strip, WArea usableArea, int direction);

/* Mark wwin's column as locked (True) or unlocked (False). Locked
 * columns are skipped entirely by wTileRelayout() - call this with
 * True from wMaximizeWindow() and False from wUnmaximizeWindow().
 * No-op if wwin has no column in this strip. */
void wTileSetLocked(WTileStrip *strip, struct WWindow *wwin, Bool locked);

/* Scroll the strip so wwin's column becomes fully visible, if it has
 * one and isn't already. Unlike wTileScrollToNextColumn() this jumps
 * to a specific window's column, not to the next/previous one. */
void wTileScrollToWindow(WTileStrip *strip, struct WWindow *wwin, WArea usableArea);

#endif /* WMTILING_H_ */