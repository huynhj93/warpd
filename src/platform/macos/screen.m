/*
 * warpd - A modal keyboard-driven pointing system.
 *
 * © 2019 Raheman Vaiya (see: LICENSE).
 */

#include "macos.h"

struct screen screens[32];
size_t nr_screens;

static void draw_hook(void *arg, NSView *view)
{
	struct box *b = arg;
	macos_draw_box(b->scr, b->color, b->x, b->y, b->w, b->h, 0);
}

void osx_screen_draw_box(struct screen *scr, int x, int y, int w, int h, const char *color)
{
	assert(scr->nr_boxes < MAX_BOXES);
	struct box *b = &scr->boxes[scr->nr_boxes++];

	b->x = x;
	b->y = y;
	b->w = w;
	b->h = h;
	b->scr = scr;
	b->color = nscolor_from_hex(color);

	window_register_draw_hook(scr->overlay, draw_hook, b);
}

void osx_screen_list(struct screen *rscreens[MAX_SCREENS], size_t *n)
{
	size_t i;

	for (i = 0; i < nr_screens; i++)
		rscreens[i] = &screens[i];

	*n = nr_screens;
}

void osx_screen_clear(struct screen *scr)
{
	scr->nr_boxes = 0;
	scr->overlay->nr_hooks = 0;
}

void osx_screen_get_dimensions(struct screen *scr, int *w, int *h)
{
	*w = scr->w;
	*h = scr->h;
}

/* Set on the main thread when macOS reports a display change. */
static int screens_dirty;

/* Must run on the main thread. */
static void build_screens(void)
{
	nr_screens = 0;
	for (NSScreen *screen in NSScreen.screens) {
		struct screen *scr;

		if (nr_screens == MAX_SCREENS)
			break;

		scr = &screens[nr_screens++];
		memset(scr, 0, sizeof *scr);

		scr->x = screen.frame.origin.x;
		scr->y = screen.frame.origin.y;
		scr->w = screen.frame.size.width;
		scr->h = screen.frame.size.height;

		scr->overlay = create_overlay_window(scr->x, scr->y, scr->w, scr->h);
	}
}

/*
 * The screen list used to be read once at startup, so after docking or
 * undocking warpd worked with displays that no longer existed. Rebuild it
 * (and the per-screen overlay windows) when it has changed. Called between
 * modes, so nothing is drawing on the old overlays.
 */
void osx_screen_refresh(void)
{
	dispatch_sync(dispatch_get_main_queue(), ^{
		size_t i;

		if (!screens_dirty)
			return;
		screens_dirty = 0;

		for (i = 0; i < nr_screens; i++) {
			/* Borderless NSWindows are released when closed. */
			[screens[i].overlay->win close];
			free(screens[i].overlay);
		}

		build_screens();
	});
}

void macos_init_screen(void)
{
	build_screens();

	[[NSNotificationCenter defaultCenter]
	    addObserverForName:NSApplicationDidChangeScreenParametersNotification
			object:nil
			 queue:[NSOperationQueue mainQueue]
		    usingBlock:^(NSNotification *note) {
			    screens_dirty = 1;
		    }];
}
