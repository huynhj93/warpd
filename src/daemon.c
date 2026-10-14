#include "warpd.h"
static const char *activation_keys[] = {
	"activation_key",
	"hint_activation_key",
	"grid_activation_key",
	"hint_oneshot_key",
	"screen_activation_key",
	"hint2_activation_key",
	"hint2_oneshot_key",
	"history_activation_key",
	"screen_next_key",
	"click_key",
	"pulse_key",
};

static struct input_event activation_events[sizeof activation_keys / sizeof activation_keys[0]];

static void reload_config(const char *path)
{
	size_t i;

	parse_config(path);

	init_hints();
	init_mouse();

	for (i = 0; i < sizeof activation_keys / sizeof activation_keys[0]; i++)
		input_parse_string(&activation_events[i], config_get(activation_keys[i]));

}

/*
 * Clicks carry whatever modifiers are physically held, so a click sent while
 * a hotkey's modifiers are still down reaches the app as a modified click
 * (cmd-click, alt-click, ...). Wait for them to be released; returns 0 if
 * they're still held after a second, so the caller can skip the click.
 */
static int await_modifier_release(uint8_t mods)
{
	int waited;

	for (waited = 0; mods && waited < 1000; waited += 20) {
		struct input_event *ev = platform->input_next_event(20);
		const char *name;

		if (!ev || ev->pressed)
			continue;

		name = platform->input_lookup_name(ev->code, 0);
		if (!name)
			continue;

		if (strstr(name, "meta"))
			mods &= ~PLATFORM_MOD_META;
		else if (strstr(name, "alt"))
			mods &= ~PLATFORM_MOD_ALT;
		else if (strstr(name, "control"))
			mods &= ~PLATFORM_MOD_CONTROL;
		else if (strstr(name, "shift"))
			mods &= ~PLATFORM_MOD_SHIFT;
	}

	return !mods;
}

void daemon_loop(const char *config_path)
{

	platform->monitor_file(config_path);
	reload_config(config_path);

	while (1) {
		int mode = 0;
		struct input_event *ev = platform->input_wait(activation_events,
							     sizeof(activation_events) /
							     sizeof(activation_events[0]));

		if (!ev) {
			reload_config(config_path);
			continue;
		}

		/* Between modes: pick up any display change before acting on the key. */
		if (platform->screen_refresh)
			platform->screen_refresh();

		config_input_whitelist(activation_keys, sizeof activation_keys / sizeof activation_keys[0]);

		if (config_input_match(ev, "activation_key"))
			mode = MODE_NORMAL;
		else if (config_input_match(ev, "grid_activation_key"))
			mode = MODE_GRID;
		else if (config_input_match(ev, "hint_activation_key"))
			mode = MODE_HINT;
		else if (config_input_match(ev, "hint2_activation_key"))
			mode = MODE_HINT2;
		else if (config_input_match(ev, "screen_activation_key"))
			mode = MODE_SCREEN_SELECTION;
		else if (config_input_match(ev, "history_activation_key"))
			mode = MODE_HISTORY;
		else if (config_input_match(ev, "hint2_oneshot_key")) {
			full_hint_mode(1);
			continue;
		} else if (config_input_match(ev, "hint_oneshot_key")) {
			full_hint_mode(0);
			continue;
		} else if (config_input_match(ev, "history_oneshot_key")) {
			history_hint_mode();
			continue;
		} else if (config_input_match(ev, "screen_next_key")) {
			/* The event tap grabbed the keyboard when this key matched; no mode will release it. */
			screen_next();
			platform->input_ungrab_keyboard();
			continue;
		} else if (config_input_match(ev, "pulse_key")) {
			/* The event tap grabbed the keyboard when this key matched; release it
			 * before animating so typing isn't held up. */
			platform->input_ungrab_keyboard();
			pulse();
			continue;
		} else if (config_input_match(ev, "click_key")) {
			/* ev is overwritten by the next read, so take the modifiers first. */
			uint8_t mods = ev->mods;

			platform->input_ungrab_keyboard();
			if (await_modifier_release(mods))
				platform->mouse_click(1);
			continue;
		}

		mode_loop(mode, 0, 1);
	}
}

