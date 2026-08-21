/*
 * Android touch/gamepad input bridge.
 *
 * The OpenTouch layer (Clibs_OpenTouch/descent) talks to the engine only
 * through this flat API, so it never has to include D2X-XL's headers - and this
 * file is the only place that knows about tControlInfo. The dxx-redux and
 * dxx-rebirth forks expose the same functions, which is what lets one copy of
 * the glue drive all three engines.
 */

#ifndef DXX_ANDROID_TOUCH_INPUT_H
#define DXX_ANDROID_TOUCH_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Actions, kept independent of OpenTouch's PORT_ACT_* codes; the glue
 * translates. The first entries match the other two engines value for value, so
 * do not reorder them - the D2X-XL-only ones are appended.
 *
 * ENERGY_SHIELD and TOGGLE_BOMB have no equivalent in tControlInfo and are
 * accepted but ignored here. */
enum dxx_touch_action
{
	DXX_TA_FIRE_PRIMARY,
	DXX_TA_FIRE_SECONDARY,
	DXX_TA_FIRE_FLARE,
	DXX_TA_DROP_BOMB,
	DXX_TA_CYCLE_PRIMARY,
	DXX_TA_CYCLE_SECONDARY,
	DXX_TA_ACCELERATE,
	DXX_TA_REVERSE,
	DXX_TA_SLIDE_LEFT,
	DXX_TA_SLIDE_RIGHT,
	DXX_TA_SLIDE_UP,
	DXX_TA_SLIDE_DOWN,
	DXX_TA_BANK_LEFT,
	DXX_TA_BANK_RIGHT,
	DXX_TA_AUTOMAP,
	DXX_TA_REAR_VIEW,
	DXX_TA_AFTERBURNER,
	DXX_TA_HEADLIGHT,
	DXX_TA_ENERGY_SHIELD,	/* no counterpart here */
	DXX_TA_TOGGLE_BOMB,	/* no counterpart here */
	DXX_TA_ZOOM,		/* D2X-XL only from here down */
	DXX_TA_CLOAK,
	DXX_TA_INVULNERABILITY,
	DXX_TA_SLOW_MOTION,
	DXX_TA_BULLET_TIME,
	DXX_TA_TOGGLE_ICONS,
	DXX_TA_MAX
};

/* Screen mode, so the touch layer knows which control set to show. */
enum dxx_touch_screen
{
	DXX_TS_BLANK,
	DXX_TS_MENU,
	DXX_TS_GAME,
	DXX_TS_MAP
};

/* Axis rates, normalised to -1..1.
 *
 * "absolute" is a stick deflection and holds until changed; "relative" is a
 * swipe/gyro delta that expires shortly after arriving, matching how the engine
 * treats joystick vs mouse input. */
void dxx_touch_axis_forward(float v);
void dxx_touch_axis_sideways(float v);
void dxx_touch_axis_vertical(float v);
void dxx_touch_axis_pitch(float v, int relative);
void dxx_touch_axis_heading(float v, int relative);
void dxx_touch_axis_bank(float v, int relative);

void dxx_touch_action(int state, int action);

/* Select a weapon by its number key (1-5 primary, 6-0 secondary). D2X-XL has no
 * select-weapon count in tControlInfo, so this goes in as a key event. */
void dxx_touch_select_weapon(int number_key);

int dxx_touch_screen_mode(void);

/* Called from CControlsManager::Read() just before it clamps, so touch input is
 * bounded exactly like keyboard, mouse and joystick input. The bounds are passed
 * in because they are the manager's own private state, and pitch has a different
 * one from the rest. */
void dxx_touch_apply_controls(int max_turn_rate, int max_pitch_rate);

#ifdef __cplusplus
}
#endif

#endif /* DXX_ANDROID_TOUCH_INPUT_H */
