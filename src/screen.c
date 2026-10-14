#include "warpd.h"

void screen_selection_mode(void)
{
	size_t i;
	size_t n;
	screen_t screens[MAX_SCREENS];
	struct input_event *ev;
	const char *screen_chars = config_get("screen_chars");

	platform->screen_list(screens, &n);
	assert(strlen(screen_chars) >= n);

	for (i = 0; i < n; i++) {
		struct hint hint;
		int w, h;
		platform->screen_get_dimensions(screens[i], &w, &h);

		hint.x = w / 2 - 25;
		hint.y = h / 2 - 25;
		hint.w = 50;
		hint.h = 50;

		hint.label[0] = screen_chars[i];
		hint.label[1] = 0;

		platform->hint_draw(screens[i], &hint, 1);
	}

	platform->commit();

	platform->input_grab_keyboard();
	while (1) {
		ev = platform->input_next_event(0);
		/* NULL is the config-reload signal; there is no timeout here. */
		if (ev && ev->pressed)
			break;
	}
	platform->input_ungrab_keyboard();

	for (i = 0; i < n; i++) {
		const char *key = input_event_tostr(ev);

		if (key[0] == screen_chars[i] && key[1] == 0) {
			int w, h;
			platform->screen_get_dimensions(screens[i], &w, &h);
			platform->mouse_move(screens[i], w/2, h/2);
		}
	}

	for (i = 0; i < n; i++)
		platform->screen_clear(screens[i]);

	platform->commit();
}

/*
 * Move the pointer to the centre of the next screen, wrapping around. Unlike
 * screen_selection_mode() this draws nothing, waits for no input and leaves
 * no mode active afterwards.
 */
void screen_next(void)
{
	size_t i, n;
	int w, h;
	screen_t cur;
	screen_t screens[MAX_SCREENS];

	platform->screen_list(screens, &n);
	if (n < 2)
		return;

	platform->mouse_get_position(&cur, NULL, NULL);
	for (i = 0; i < n; i++)
		if (screens[i] == cur)
			break;

	cur = screens[i < n ? (i + 1) % n : 0];
	platform->screen_get_dimensions(cur, &w, &h);
	platform->mouse_move(cur, w / 2, h / 2);
}

/*
 * Show where the pointer is: two rings growing outward from it over about
 * half a second, in the cursor colour. Circles where the platform can draw
 * them, square outlines otherwise. Draws on its own and waits for no input.
 */
void pulse(void)
{
	const char *color = config_get("cursor_color");
	const int waves = 2;
	const int frames = 10;
	int x, y, sw, sh;
	int wave, frame;
	screen_t scr;

	platform->mouse_get_position(&scr, &x, &y);
	platform->screen_get_dimensions(scr, &sw, &sh);

	const int thickness = sh / 300 > 3 ? sh / 300 : 3;
	const int step = sh / 60;

	for (wave = 0; wave < waves; wave++) {
		for (frame = 1; frame <= frames; frame++) {
			const int r = 8 + step * frame;

			platform->screen_clear(scr);
			if (platform->screen_draw_ring) {
				platform->screen_draw_ring(scr, x, y, r, thickness, color);
			} else {
				platform->screen_draw_box(scr, x - r, y - r, 2 * r, thickness, color);
				platform->screen_draw_box(scr, x - r, y + r - thickness, 2 * r, thickness, color);
				platform->screen_draw_box(scr, x - r, y - r, thickness, 2 * r, color);
				platform->screen_draw_box(scr, x + r - thickness, y - r, thickness, 2 * r, color);
			}
			platform->commit();

			usleep(25000);
		}
	}

	platform->screen_clear(scr);
	platform->commit();
}
