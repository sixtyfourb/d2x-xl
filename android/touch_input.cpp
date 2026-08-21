/*
 * Android touch/gamepad input bridge - the D2X-XL side.
 *
 * Everything the touch layer sends lands in one state block, which is folded
 * into the engine's control info once per frame from CControlsManager::Read().
 * That is the same place the keyboard, mouse and joystick contributions are
 * added, and it happens before the manager clamps, so touch input is bounded
 * like every other source rather than being able to exceed it.
 */

#include <string.h>

#include <SDL.h>

#include "descent.h"
#include "input.h"
#include "kconfig.h"
#include "touch_input.h"

#if defined (DXX_TOUCH_DEBUG)
#	include <android/log.h>
#	define TLOG(...) __android_log_print (ANDROID_LOG_INFO, "DxxTouch", __VA_ARGS__)
#else
#	define TLOG(...) do {} while (0)
#endif

//------------------------------------------------------------------------------

// A swipe or gyro delta describes movement that already happened, so it has to
// be spent and then forgotten. Holding it for a fixed span rather than clearing
// it on the next read is deliberate: the engine reads controls far more often
// than input arrives, and clearing on first read threw most of it away.
#define REL_HOLD_MS	33
#define REL_GAIN	40.0f

typedef struct tTouchState {
	float	forward, sideways, vertical;
	float	pitch, heading, bank;
	float	pitchRel, headingRel, bankRel;
	uint8_t	actionState [DXX_TA_MAX];
	uint8_t	actionPrevState [DXX_TA_MAX];
	int32_t	selectWeapon;
	uint32_t relExpiry;
} tTouchState;

static tTouchState touch;

//------------------------------------------------------------------------------

void dxx_touch_axis_forward (float v) { touch.forward = v; }
void dxx_touch_axis_sideways (float v) { touch.sideways = v; }
void dxx_touch_axis_vertical (float v) { touch.vertical = v; }

void dxx_touch_axis_pitch (float v, int relative)
{
if (relative) {
	touch.pitchRel = v;
	touch.relExpiry = SDL_GetTicks () + REL_HOLD_MS;
	}
else
	touch.pitch = v;
}

void dxx_touch_axis_heading (float v, int relative)
{
if (relative) {
	touch.headingRel = v;
	touch.relExpiry = SDL_GetTicks () + REL_HOLD_MS;
	}
else
	touch.heading = v;
}

void dxx_touch_axis_bank (float v, int relative)
{
if (relative) {
	touch.bankRel = v;
	touch.relExpiry = SDL_GetTicks () + REL_HOLD_MS;
	}
else
	touch.bank = v;
}

//------------------------------------------------------------------------------

void dxx_touch_action (int state, int action)
{
if ((action >= 0) && (action < DXX_TA_MAX))
	touch.actionState [action] = state ? 1 : 0;
}

//------------------------------------------------------------------------------

// tControlInfo has no select-weapon count - D2X-XL reads the number keys
// directly - so this goes in as a key event rather than a control value.

void dxx_touch_select_weapon (int number_key)
{
	static const SDL_Keycode digits [10] = {
		SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5,
		SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_0
	};

if ((number_key < 1) || (number_key > 10))
	return;

	SDL_Event event;

memset (&event, 0, sizeof (event));
event.type = SDL_KEYDOWN;
event.key.state = SDL_PRESSED;
event.key.keysym.sym = digits [number_key - 1];
SDL_PushEvent (&event);
event.type = SDL_KEYUP;
event.key.state = SDL_RELEASED;
SDL_PushEvent (&event);
}

//------------------------------------------------------------------------------

int dxx_touch_screen_mode (void)
{
if (gameStates.menus.nInMenu > 0)
	return DXX_TS_MENU;
if (gameStates.app.bGameRunning)
	return DXX_TS_GAME;
return DXX_TS_MENU;
}

//------------------------------------------------------------------------------

// A state is held for as long as the button is; a count is a press, so it only
// goes up on the edge. Without the edge check a held fire button would count
// once per frame, and Descent reads both.

static inline void ApplyState (int32_t action, uint8_t& state)
{
if (touch.actionState [action])
	state = 1;
}

static inline void ApplyCount (int32_t action, uint8_t& count)
{
if (touch.actionState [action] && !touch.actionPrevState [action])
	count++;
}

//------------------------------------------------------------------------------

void dxx_touch_apply_controls (int max_turn_rate, int max_pitch_rate)
{
	tControlInfo&	ci = controls [0];
	int32_t			i;

if (SDL_TICKS_PASSED (SDL_GetTicks (), touch.relExpiry))
	touch.pitchRel = touch.headingRel = touch.bankRel = 0;

/* --- Rotation --- */
ci.pitchTime += (fix) (touch.pitch * max_pitch_rate);
ci.headingTime += (fix) (touch.heading * max_turn_rate);
ci.bankTime += (fix) (touch.bank * max_turn_rate);

TLOG ("apply: abs p=%f h=%f | rate %d/%d | pitchTime=%d headingTime=%d",
      touch.pitch, touch.heading, max_pitch_rate, max_turn_rate,
      (int) ci.pitchTime, (int) ci.headingTime);

ci.pitchTime += (fix) (touch.pitchRel * REL_GAIN * max_turn_rate);
ci.headingTime += (fix) (touch.headingRel * REL_GAIN * max_turn_rate);
ci.bankTime += (fix) (touch.bankRel * REL_GAIN * max_turn_rate);

/* --- Translation --- */
ci.forwardThrustTime += (fix) (touch.forward * max_turn_rate);
ci.sidewaysThrustTime += (fix) (touch.sideways * max_turn_rate);
ci.verticalThrustTime += (fix) (touch.vertical * max_turn_rate);

/* Button pairs for the axes the sticks do not cover. */
if (touch.actionState [DXX_TA_SLIDE_UP])
	ci.verticalThrustTime += max_turn_rate;
if (touch.actionState [DXX_TA_SLIDE_DOWN])
	ci.verticalThrustTime -= max_turn_rate;
if (touch.actionState [DXX_TA_SLIDE_RIGHT])
	ci.sidewaysThrustTime += max_turn_rate;
if (touch.actionState [DXX_TA_SLIDE_LEFT])
	ci.sidewaysThrustTime -= max_turn_rate;
if (touch.actionState [DXX_TA_BANK_LEFT])
	ci.bankTime += max_turn_rate;
if (touch.actionState [DXX_TA_BANK_RIGHT])
	ci.bankTime -= max_turn_rate;
if (touch.actionState [DXX_TA_ACCELERATE])
	ci.forwardThrustTime += max_turn_rate;
if (touch.actionState [DXX_TA_REVERSE])
	ci.forwardThrustTime -= max_turn_rate;

/* --- Buttons --- */
ApplyState (DXX_TA_FIRE_PRIMARY, ci.firePrimaryState);
ApplyCount (DXX_TA_FIRE_PRIMARY, ci.firePrimaryDownCount);
ApplyState (DXX_TA_FIRE_SECONDARY, ci.fireSecondaryState);
ApplyCount (DXX_TA_FIRE_SECONDARY, ci.fireSecondaryDownCount);
ApplyCount (DXX_TA_FIRE_FLARE, ci.fireFlareDownCount);
ApplyCount (DXX_TA_DROP_BOMB, ci.dropBombDownCount);
ApplyCount (DXX_TA_CYCLE_PRIMARY, ci.cyclePrimaryCount);
ApplyCount (DXX_TA_CYCLE_SECONDARY, ci.cycleSecondaryCount);

ApplyState (DXX_TA_AUTOMAP, ci.automapState);
ApplyCount (DXX_TA_AUTOMAP, ci.automapDownCount);
ApplyState (DXX_TA_REAR_VIEW, ci.rearViewDownState);
ApplyCount (DXX_TA_REAR_VIEW, ci.rearViewDownCount);

ApplyState (DXX_TA_AFTERBURNER, ci.afterburnerState);
ApplyCount (DXX_TA_HEADLIGHT, ci.headlightCount);

/* D2X-XL's own additions. */
ApplyCount (DXX_TA_ZOOM, ci.zoomDownCount);
ApplyCount (DXX_TA_CLOAK, ci.useCloakDownCount);
ApplyCount (DXX_TA_INVULNERABILITY, ci.useInvulDownCount);
ApplyCount (DXX_TA_SLOW_MOTION, ci.slowMotionCount);
ApplyCount (DXX_TA_BULLET_TIME, ci.bulletTimeCount);
ApplyCount (DXX_TA_TOGGLE_ICONS, ci.toggleIconsCount);

for (i = 0; i < DXX_TA_MAX; i++)
	touch.actionPrevState [i] = touch.actionState [i];
}

//------------------------------------------------------------------------------
//eof
