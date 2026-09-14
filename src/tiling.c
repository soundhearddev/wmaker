/* tiling.c - scrollable tiling (niri-like) window layout, PoC
 *
 *  Window Maker window manager
 */

#include "wconfig.h"

#include <stdio.h>
#include <stdlib.h>

#include "WindowMaker.h"
#include "window.h"
#include "tiling.h"
#include "stacking.h"
#include "dock.h"

WTileStrip *wTileStripCreate(void)
{
	WTileStrip *strip = wmalloc(sizeof(WTileStrip));

	strip->first = NULL;
	strip->last = NULL;
	strip->focused = NULL;
	strip->viewport_x = 0;
	strip->enabled = False;

	return strip;
}

void wTileStripDestroy(WTileStrip *strip)
{
	WTileColumn *col, *next;

	if (!strip)
		return;

	for (col = strip->first; col != NULL; col = next) {
		next = col->next;
		wfree(col);
	}

	wfree(strip);
}

/* TODO: always half the usable width, ignores how many columns already
 * exist - see TODO in tiling.h next to wTileComputeGeometry(). */
static int default_column_width(WArea usableArea)
{
	return (usableArea.x2 - usableArea.x1) / 2;
}

void wTileAddWindow(WTileStrip *strip, WWindow *wwin, WArea usableArea,
                    int *x_ret, int *y_ret, unsigned int *width_ret, unsigned int *height_ret)
{
	WTileColumn *col;
	int cursor_x = usableArea.x1 - (strip ? strip->viewport_x : 0);

	if (!strip) {
		*x_ret = usableArea.x1;
		*y_ret = usableArea.y1;
		*width_ret = default_column_width(usableArea);
		*height_ret = usableArea.y2 - usableArea.y1;
		return;
	}

	for (col = strip->first; col != NULL; col = col->next) {
		if (col->window == wwin) {
			*x_ret = cursor_x;
			*y_ret = usableArea.y1;
			*width_ret = col->width;
			*height_ret = usableArea.y2 - usableArea.y1;
			return;
		}
		cursor_x += col->width;
	}

	col = wmalloc(sizeof(WTileColumn));
	col->window = wwin;
	col->width = default_column_width(usableArea);
	col->locked = False;
	col->next = NULL;
	col->prev = strip->last;

	if (strip->last)
		strip->last->next = col;
	else
		strip->first = col;

	strip->last = col;
	strip->focused = col;

	*x_ret = cursor_x;
	*y_ret = usableArea.y1;
	*width_ret = col->width;
	*height_ret = usableArea.y2 - usableArea.y1;
	wTileRelayout(strip, usableArea);
}

void wTileRemoveWindow(WTileStrip *strip, WWindow *wwin, WArea usableArea)
{
	WTileColumn *col;

	if (!strip || !wwin)
		return;

	for (col = strip->first; col != NULL; col = col->next) {
		if (col->window == wwin)
			break;
	}

	if (!col)
		return;

	if (col->prev)
		col->prev->next = col->next;
	else
		strip->first = col->next;

	if (col->next)
		col->next->prev = col->prev;
	else
		strip->last = col->prev;

	if (strip->focused == col)
		strip->focused = col->prev ? col->prev : col->next;

	wfree(col);

	wTileRelayout(strip, usableArea);
}

void wTileRelayout(WTileStrip *strip, WArea usableArea)
{
	WTileColumn *col;
	int cursor_x;

	if (!strip)
		return;

	cursor_x = usableArea.x1 - strip->viewport_x;

	for (col = strip->first; col != NULL; col = col->next) {
		if (col->locked) {
			/* Maximized/fullscreen - WindowMaker's own maximize
			 * code owns this window's geometry, don't touch it.
			 * Still advance cursor_x so columns after this one
			 * keep their position in the strip. */
			cursor_x += col->width;
			continue;
		}

		if (col->window && col->window->flags.mapped) {
			int win_x = cursor_x;
			int win_w = col->width;

			if (win_x < usableArea.x1) {
				int overlap = usableArea.x1 - win_x;
				win_x = usableArea.x1;     
				win_w = win_w - overlap;   
			}

			if (win_w > 0) {
				wWindowConfigure(col->window, win_x, usableArea.y1,
						 win_w, usableArea.y2 - usableArea.y1);
			} else {
				wWindowConfigure(col->window, -10000, usableArea.y1,
						 col->width, usableArea.y2 - usableArea.y1);
			}
		}
		cursor_x += col->width;
	}
}

#define WTILE_SCROLL_STEP 40

void wTileScrollBy(WTileStrip *strip, int delta_x, WArea usableArea)
{
	int step;

	if (!strip || delta_x == 0)
		return;

	step = (delta_x > 0) ? WTILE_SCROLL_STEP : -WTILE_SCROLL_STEP;

	while (abs(delta_x) > WTILE_SCROLL_STEP) {
		strip->viewport_x += step;
		delta_x -= step;
		wTileRelayout(strip, usableArea);
	}

	strip->viewport_x += delta_x;
	wTileRelayout(strip, usableArea);
}

void wTileScrollToNextColumn(WTileStrip *strip, WArea usableArea, int direction)
{
	WTileColumn *col;
	int cursor_x, target_x;
	int screen_left = usableArea.x1;

	if (!strip || !strip->first)
		return;

	cursor_x = usableArea.x1 - strip->viewport_x;

	if (direction > 0) {
		for (col = strip->first; col != NULL; col = col->next) {
			if (cursor_x > screen_left) {
				target_x = cursor_x;
				wTileScrollBy(strip, target_x - screen_left, usableArea);
				return;
			}
			cursor_x += col->width;
		}
	} else {
		WTileColumn *prev_candidate = NULL;
		int prev_x = 0;

		for (col = strip->first; col != NULL; col = col->next) {
			if (cursor_x >= screen_left)
				break;
			prev_candidate = col;
			prev_x = cursor_x;
			cursor_x += col->width;
		}
		if (prev_candidate) {
			wTileScrollBy(strip, prev_x - screen_left, usableArea);
		}
	}
}

void wTileSetLocked(WTileStrip *strip, WWindow *wwin, Bool locked)
{
	WTileColumn *col;

	if (!strip || !wwin)
		return;

	for (col = strip->first; col != NULL; col = col->next) {
		if (col->window == wwin) {
			col->locked = locked;
			return;
		}
	}
}

void wTileScrollToWindow(WTileStrip *strip, WWindow *wwin, WArea usableArea)
{
	WTileColumn *col;
	int cursor_x;
	int screen_left = usableArea.x1;
	int screen_right = usableArea.x2;
	int col_left, col_right;

	if (!strip || !wwin)
		return;

	cursor_x = usableArea.x1 - strip->viewport_x;

	for (col = strip->first; col != NULL; col = col->next) {
		if (col->window == wwin) {
			col_left = cursor_x;
			col_right = cursor_x + col->width;

			/* Already fully visible - nothing to do. */
			if (col_left >= screen_left && col_right <= screen_right)
				return;

			/* Scrolled too far right (column is to the left of
			 * the viewport) - bring its left edge to the screen's
			 * left edge. */
			if (col_left < screen_left) {
				wTileScrollBy(strip, col_left - screen_left, usableArea);
				return;
			}

			/* Scrolled too far left (column is to the right of
			 * the viewport) - bring its right edge to the
			 * screen's right edge. */
			wTileScrollBy(strip, col_right - screen_right, usableArea);
			return;
		}
		cursor_x += col->width;
	}
}