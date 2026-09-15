/*
 *
 * SDL joystick support
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include <string.h>   // for memset
#ifdef __macosx__
# include <SDL/SDL.h>
#else
# include <SDL.h>
#endif

#include "descent.h"
#include "sdl_compat.h"
#include "joy.h"
#include "key.h"	// for the KEY_* codes JoyMenuKey returns
#include "error.h"
#include "timer.h"
#include "console.h"
#include "event.h"
#include "text.h"

extern int32_t joybutton_text []; //from KConfig.c

char bJoyPresent = 0;

int32_t joyDeadzone [UNIQUE_JOY_AXES] = {32767 / 10, 32767 / 10, 32767 / 10, 32767 / 10};	//10% of max. range

class CJoyInfo {
	public:
		int32_t		nAxes;
		int32_t		nButtons;
		tJoyAxis		axes [MAX_JOYSTICKS * JOY_MAX_AXES];
		tJoyButton	buttons [MAX_JOYSTICKS * JOY_MAX_BUTTONS];

		CJoyInfo () { memset (this, 0, sizeof (*this)); }
};

CJoyInfo joyInfo;

tSdlJoystick /*tSdlJoystick*/ sdlJoysticks [MAX_JOYSTICKS];

//------------------------------------------------------------------------------

void JoyButtonHandler (SDL_JoyButtonEvent *jbe)
{
	int32_t button;

if ((jbe->which >= MAX_JOYSTICKS) || (jbe->button > MAX_BUTTONS_PER_JOYSTICK))
	return;
button = sdlJoysticks [jbe->which].buttonMap [jbe->button] + jbe->which * MAX_BUTTONS_PER_JOYSTICK;
joyInfo.buttons [button].state = jbe->state;
switch (jbe->type) {
	case SDL_JOYBUTTONDOWN:
		joyInfo.buttons [button].xTimeWentDown = TimerGetFixedSeconds();
		joyInfo.buttons [button].numDowns++;
		break;
	case SDL_JOYBUTTONUP:
		joyInfo.buttons [button].numUps++;
		break;
	}
}

//------------------------------------------------------------------------------

void JoyHatHandler (SDL_JoyHatEvent *jhe)
{
if (jhe->which >= MAX_JOYSTICKS)
	return;
int32_t hat = sdlJoysticks [jhe->which].hatMap [jhe->hat] + jhe->which * MAX_BUTTONS_PER_JOYSTICK;
//Save last state of the hat-button
joyInfo.buttons [hat  ].lastState = joyInfo.buttons [hat  ].state;
joyInfo.buttons [hat+1].lastState = joyInfo.buttons [hat+1].state;
joyInfo.buttons [hat+2].lastState = joyInfo.buttons [hat+2].state;
joyInfo.buttons [hat+3].lastState = joyInfo.buttons [hat+3].state;

//get current state of the hat-button
joyInfo.buttons [hat  ].state = ((jhe->value & SDL_HAT_UP) > 0);
joyInfo.buttons [hat+1].state = ((jhe->value & SDL_HAT_RIGHT) > 0);
joyInfo.buttons [hat+2].state = ((jhe->value & SDL_HAT_DOWN) > 0);
joyInfo.buttons [hat+3].state = ((jhe->value & SDL_HAT_LEFT) > 0);

//determine if a hat-button up or down event based on state and lastState
for (int32_t hbi = 0; hbi < 4; hbi++) {
	if (!joyInfo.buttons [hat+hbi].lastState && joyInfo.buttons [hat+hbi].state) { //lastState up, current state down
		joyInfo.buttons [hat+hbi].xTimeWentDown
			= TimerGetFixedSeconds();
		joyInfo.buttons [hat+hbi].numDowns++;
		}
	else if (joyInfo.buttons [hat+hbi].lastState && !joyInfo.buttons [hat+hbi].state) { //lastState down, current state up
		joyInfo.buttons [hat+hbi].numUps++;
		}
	}
}

//------------------------------------------------------------------------------

void JoyAxisHandler (SDL_JoyAxisEvent *jae)
{
if (jae->which >= MAX_JOYSTICKS)
	return;
int32_t axis = sdlJoysticks [jae->which].axisMap [jae->axis] + jae->which * MAX_AXES_PER_JOYSTICK;
joyInfo.axes [axis].nValue = jae->value;
if (jae->which)
	axis = 0;
}

//------------------------------------------------------------------------------

int32_t JoyInit (void)
{
	int32_t			i, j, n;
	tSdlJoystick	*pJoystick = sdlJoysticks;

if (SDL_Init (SDL_INIT_JOYSTICK) < 0) {
#if TRACE
	console.printf(CON_VERBOSE, "sdl-joystick: initialisation failed: %s.",SDL_GetError());
#endif
	return 0;
	}
memset (&joyInfo, 0, sizeof (joyInfo));
n = SDL_NumJoysticks();

#if TRACE
console.printf(CON_VERBOSE, "sdl-joystick: found %d joysticks\n", n);
#endif
for (i = 0; (i < n) && (gameStates.input.nJoysticks < MAX_JOYSTICKS); i++) {
#if TRACE
	console.printf(CON_VERBOSE, "sdl-joystick %d: %s\n", i, SDL_JoystickNameForIndex (i));
#endif
	pJoystick->handle = SDL_JoystickOpen (i);
	if (pJoystick->handle) {
		bJoyPresent = 1;
		if((pJoystick->nAxes = SDL_JoystickNumAxes (pJoystick->handle)) > MAX_AXES_PER_JOYSTICK) {
			Warning (TXT_JOY_AXESNO, pJoystick->nAxes, MAX_AXES_PER_JOYSTICK);
			pJoystick->nAxes = MAX_AXES_PER_JOYSTICK;
			}

		if((pJoystick->nButtons = SDL_JoystickNumButtons (pJoystick->handle)) > MAX_BUTTONS_PER_JOYSTICK) {
			Warning (TXT_JOY_BUTTONNO, pJoystick->nButtons, MAX_BUTTONS_PER_JOYSTICK);
			pJoystick->nButtons = MAX_BUTTONS_PER_JOYSTICK;
			}
		if((pJoystick->nHats = SDL_JoystickNumHats (pJoystick->handle)) > MAX_HATS_PER_JOYSTICK) {
			Warning (TXT_JOY_HATNO, pJoystick->nHats, MAX_HATS_PER_JOYSTICK);
			pJoystick->nHats = MAX_HATS_PER_JOYSTICK;
			}
#if TRACE
		console.printf(CON_VERBOSE, "sdl-joystick: %d axes\n", pJoystick->nAxes);
		console.printf(CON_VERBOSE, "sdl-joystick: %d buttons\n", pJoystick->nButtons);
		console.printf(CON_VERBOSE, "sdl-joystick: %d hats\n", pJoystick->nHats);
#endif
		memset (&joyInfo, 0, sizeof (joyInfo));
		for (j = 0; j < pJoystick->nAxes; j++)
			pJoystick->axisMap [j] = joyInfo.nAxes++;
		for (j = 0; j < pJoystick->nButtons; j++)
			pJoystick->buttonMap [j] = joyInfo.nButtons++;
		for (j = 0; j < pJoystick->nHats; j++) {
			pJoystick->hatMap [j] = joyInfo.nButtons;
			//a hat counts as four buttons
			joybutton_text [joyInfo.nButtons++] = i ? TNUM_HAT2_U : TNUM_HAT_U;
			joybutton_text [joyInfo.nButtons++] = i ? TNUM_HAT2_R : TNUM_HAT_R;
			joybutton_text [joyInfo.nButtons++] = i ? TNUM_HAT2_D : TNUM_HAT_D;
			joybutton_text [joyInfo.nButtons++] = i ? TNUM_HAT2_L : TNUM_HAT_L;
			}
		pJoystick++;
		gameStates.input.nJoysticks++;
		}
	else {
#if TRACE
		console.printf(CON_VERBOSE, "sdl-joystick: initialization failed!\n");
#endif		
	}
#if TRACE
	console.printf(CON_VERBOSE, "sdl-joystick: %d axes (total)\n", joyInfo.nAxes);
	console.printf(CON_VERBOSE, "sdl-joystick: %d buttons (total)\n", joyInfo.nButtons);
#endif
	}
return bJoyPresent;
}

//------------------------------------------------------------------------------

void JoyClose (void)
{
	while (gameStates.input.nJoysticks)
		SDL_JoystickClose(sdlJoysticks [--gameStates.input.nJoysticks].handle);
}

//------------------------------------------------------------------------------

void JoyGetPos (int32_t *x, int32_t *y)
{
	int32_t axis [JOY_MAX_AXES];

if (!gameStates.input.nJoysticks) {
	*x = *y = 0;
	return;
	}
JoyReadRawAxis (JOY_ALL_AXIS, axis);
*x = JoyGetScaledReading (axis [0], 0);
*y = JoyGetScaledReading (axis [1], 1);
}

//------------------------------------------------------------------------------

int32_t JoyGetBtns (void)
{
#if 0 // This is never used?
int32_t	buttons = 0;
for (int32_t i = 0; i++; i<buttons) {
	switch (joyInfo.buttons [i].state) {
	case SDL_PRESSED:
		buttons |= 1<<i;
		break;
	case SDL_RELEASED:
		break;
		}
	}
return buttons;
#else
return 0;
#endif
}

//------------------------------------------------------------------------------

int32_t JoyGetButtonDownCnt (int32_t nButton)
{
	int32_t numDowns;

if (!gameStates.input.nJoysticks)
	return 0;
numDowns = joyInfo.buttons [nButton].numDowns;
joyInfo.buttons [nButton].numDowns = 0;
return numDowns;
}

//------------------------------------------------------------------------------

fix JoyGetButtonDownTime(int32_t nButton)
{
	fix time = 0;

if (!gameStates.input.nJoysticks)
	return 0;
switch (joyInfo.buttons [nButton].state) {
	case SDL_PRESSED:
		time = TimerGetFixedSeconds() - joyInfo.buttons [nButton].xTimeWentDown;
		joyInfo.buttons [nButton].xTimeWentDown = TimerGetFixedSeconds();
		break;
	case SDL_RELEASED:
		time = 0;
		break;
	}
return time;
}

//------------------------------------------------------------------------------

uint32_t JoyReadRawAxis (uint32_t mask, int32_t * axis)
{
	int32_t	i;
	uint32_t	axisFlags = 0;

if (!gameStates.input.nJoysticks)
	return 0;
for (i = 0; i < JOY_MAX_AXES; i++)
	if ((axis [i] = joyInfo.axes [i].nValue)) {
		axisFlags |= (1 << i);
		//joyInfo.axes [i].nValue = 0;
		}
return axisFlags;
}

//------------------------------------------------------------------------------

void JoyFlush (void)
{
if (!gameStates.input.nJoysticks)
	return;
for (int32_t i = 0; i < MAX_JOYSTICKS * JOY_MAX_BUTTONS; i++) {
	joyInfo.buttons [i].xTimeWentDown = 0;
	joyInfo.buttons [i].numDowns = 0;
	// Forget that a button is being held, the way KeyFlush () forgets a held
	// key. Without this, accepting an item on the controls screen with the pad
	// hands the capture that still-held button to bind, and the player binds A
	// to whatever they were only trying to open.
	joyInfo.buttons [i].lastState =
	joyInfo.buttons [i].state = 0;
	}

}

//------------------------------------------------------------------------------

int32_t JoyGetButtonState (int32_t nButton)
{
if (!gameStates.input.nJoysticks)
	return 0;
if (nButton >= JOY_MAX_BUTTONS)
	return 0;
return joyInfo.buttons [nButton].state;
}

//------------------------------------------------------------------------------

void JoyGetCalVals (tJoyAxisCal *cal, int32_t nAxes)
{
if (!nAxes)
	nAxes = JOY_MAX_AXES;
for (int32_t i = 0; i < nAxes; i++)
	cal [i] = joyInfo.axes [i].cal;
}

//------------------------------------------------------------------------------

void JoySetCalVals (tJoyAxisCal *cal, int32_t nAxes)
{
	int32_t i, j;

for (i = j = 0; i < JOY_MAX_AXES; i++, j = (j + 1) % nAxes)
	joyInfo.axes [i].cal = cal [j];
}

//------------------------------------------------------------------------------

int32_t JoyGetScaledReading (int32_t raw, int32_t nAxis)
{
#if 1
return (raw + 128) / 256;
#else
int32_t d, x;

raw -= joyInfo.axes [nAxis].cal.nCenter;
if (raw < 0)
	d = joyInfo.axes [nAxis].cal.nCenter - joyInfo.axes [nAxis].cal.nMin;
else if (raw > 0)
	d = joyInfo.axes [nAxis].cal.nMax - joyInfo.axes [nAxis].cal.nCenter;
else
	d = 0;
if (d)
	x = ((raw << 7) / d);
else
	x = 0;
if (x < -128)
	x = -128;
if (x > 127)
	x = 127;
d = (joyDeadzone [nAxis % 4]) * 6;
if ((x > (-d)) && (x < d))
	x = 0;

return x;
#endif
}

//------------------------------------------------------------------------------

void JoySetSlowReading (int32_t flag)
{
}

//------------------------------------------------------------------------------

int32_t JoySetDeadzone (int32_t nRelZone, int32_t nAxis)
{
nAxis %= UNIQUE_JOY_AXES;
gameOpts->input.joystick.deadzones [nAxis] = (nRelZone > 100) ? 100 : (nRelZone < 0) ? 0 : nRelZone;
return joyDeadzone [nAxis] = (fix) FRound (32767.0f * (float) gameOpts->input.joystick.deadzones [nAxis] / 100.0f);
}

//------------------------------------------------------------------------------
//
// Driving the menus from a gamepad.
//
// D2X-XL reads the keyboard and the mouse. On a handheld with neither, the game
// renders and flies but cannot be navigated: the main menu ignores the pad,
// briefings cannot be skipped, and the controls screen - the one screen nobody
// setting a pad up can avoid - cannot be left again.
//
// Two dozen input loops read KeyInKey (). Rather than teach each of them about
// joysticks, translate the pad into the keys they already understand, here where
// the button numbering is known. A loop opts in by calling MenuInKey () instead,
// and only the loops a player has to get through do: flight code polls the
// joystick directly and never comes past here, so a button cannot mean two
// things at once.

#define JOY_MENU_REPEAT_DELAY		350		// ms held before a direction repeats
#define JOY_MENU_REPEAT_RATE		100		// ms between repeats after that
#define JOY_MENU_AXIS_THRESHOLD	(32767 / 2)

// Numbered for an XInput-shaped pad, which is what these handhelds present -
// and what SDL reports for anything claiming to be one.
#define JOY_MENU_BUTTON_A			0
#define JOY_MENU_BUTTON_B			1
#define JOY_MENU_BUTTON_X			2
#define JOY_MENU_BUTTON_Y			3
#define JOY_MENU_BUTTON_LB			4
#define JOY_MENU_BUTTON_RB			5
#define JOY_MENU_BUTTON_BACK		6
#define JOY_MENU_BUTTON_START		7

#define JOY_MENU_UP					0
#define JOY_MENU_DOWN				1
#define JOY_MENU_LEFT				2
#define JOY_MENU_RIGHT				3

//------------------------------------------------------------------------------
// Arrow keys repeat when held, and a list of thirty missions is unusable
// without it. The pad's directions are read as levels rather than as events -
// a stick has no key-up - so the repeat has to be timed here.

static int32_t JoyMenuDirection (int32_t nDir, int32_t bDown, uint32_t t)
{
	static uint32_t tRepeat [4] = {0, 0, 0, 0};

if (!bDown) {
	tRepeat [nDir] = 0;
	return 0;
	}
if (!tRepeat [nDir]) {	// went down this frame: act at once, then wait
	tRepeat [nDir] = t + JOY_MENU_REPEAT_DELAY;
	return 1;
	}
if (t < tRepeat [nDir])
	return 0;
tRepeat [nDir] = t + JOY_MENU_REPEAT_RATE;
return 1;
}

//------------------------------------------------------------------------------
// What the first pad has to say, as a key code, or 0 for nothing.
//
// Only the first pad: a second one belongs to a second player, and having it
// move the first player's menu cursor would be worse than useless.

int32_t JoyMenuKey (void)
{
	static const int32_t nDirKeys [4] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT};

	static const struct {
		int32_t	nButton;
		int32_t	nKey;
		}	buttonKeys [] = {
			{JOY_MENU_BUTTON_A,		KEY_ENTER},		// accept, and open a submenu
			{JOY_MENU_BUTTON_START,	KEY_ENTER},
			{JOY_MENU_BUTTON_B,		KEY_ESC},		// back out
			{JOY_MENU_BUTTON_BACK,	KEY_ESC},
			{JOY_MENU_BUTTON_X,		KEY_SPACEBAR},	// tick a checkbox without accepting
			{JOY_MENU_BUTTON_Y,		KEY_F1},			// what does this option do?
			{JOY_MENU_BUTTON_LB,		KEY_PAGEUP},	// a slider, ten at a time
			{JOY_MENU_BUTTON_RB,		KEY_PAGEDOWN}
			};

	int32_t	bDown [4] = {0, 0, 0, 0};
	int32_t	nKey = 0;
	int32_t	i;

if (!gameStates.input.nJoysticks)
	return 0;

	tSdlJoystick&	j = sdlJoysticks [0];
	uint32_t			t = SDL_GetTicks ();

// The hat, which JoyInit expands into four consecutive buttons in the order
// up, right, down, left.
for (i = 0; i < j.nHats; i++) {
	int32_t hat = j.hatMap [i];
	bDown [JOY_MENU_UP]    |= joyInfo.buttons [hat].state;
	bDown [JOY_MENU_RIGHT] |= joyInfo.buttons [hat + 1].state;
	bDown [JOY_MENU_DOWN]  |= joyInfo.buttons [hat + 2].state;
	bDown [JOY_MENU_LEFT]  |= joyInfo.buttons [hat + 3].state;
	}

// The left stick, which means the same as the hat once it is far enough over.
// Half deflection is the smallest threshold worth trusting: a handheld's stick
// rests off centre often enough, and the menu deadzone is not the one the
// player tuned for flying.
if (j.nAxes > 1) {
	int32_t x = joyInfo.axes [j.axisMap [0]].nValue;
	int32_t y = joyInfo.axes [j.axisMap [1]].nValue;

	if (y < -JOY_MENU_AXIS_THRESHOLD)
		bDown [JOY_MENU_UP] = 1;
	else if (y > JOY_MENU_AXIS_THRESHOLD)
		bDown [JOY_MENU_DOWN] = 1;
	if (x < -JOY_MENU_AXIS_THRESHOLD)
		bDown [JOY_MENU_LEFT] = 1;
	else if (x > JOY_MENU_AXIS_THRESHOLD)
		bDown [JOY_MENU_RIGHT] = 1;
	}

// Every direction every time, even once one has fired: the ones that were let
// go have to be seen to be let go, or they will not repeat properly next time.
for (i = 0; i < 4; i++)
	if (JoyMenuDirection (i, bDown [i], t) && !nKey)
		nKey = nDirKeys [i];
if (nKey)
	return nKey;

// Buttons need no repeat, so take them as the events they are. Read all of
// them whatever happens - an unread press stays queued and would arrive a
// frame later, out of order with whatever came next.
for (i = 0; i < int32_t (sizeofa (buttonKeys)); i++) {
	if (buttonKeys [i].nButton >= j.nButtons)
		continue;
	if ((JoyGetButtonDownCnt (j.buttonMap [buttonKeys [i].nButton]) > 0) && !nKey)
		nKey = buttonKeys [i].nKey;
	}
return nKey;
}

//------------------------------------------------------------------------------
// The keyboard first, then whatever the pad stands for.
//
// A drop-in replacement for KeyInKey () in any loop that a player without a
// keyboard has to be able to get through.

int32_t MenuInKey (void)
{
	int32_t nKey = KeyInKey ();

return nKey ? nKey : JoyMenuKey ();
}

//------------------------------------------------------------------------------
//eof
