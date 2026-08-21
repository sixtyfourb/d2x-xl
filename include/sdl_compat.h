#ifndef _SDL_COMPAT_H
#define _SDL_COMPAT_H

#ifdef __macosx__
#	include <SDL/SDL.h>
#else
#	include <SDL.h>
#endif

// Shims for the SDL calls whose signature changed between SDL 1.2 and SDL2 and
// that are used all over the engine, so the call sites do not each need a
// version check. Anything used in only one place is handled where it is used.

#if SDL_VERSION_ATLEAST (2, 0, 0)

// SDL2 wants a name for every thread, which it reports to debuggers and crash
// handlers. Deriving it from the thread function costs nothing and beats naming
// them all by hand.
#	define D2CreateThread(_fn, _data)	SDL_CreateThread (_fn, #_fn, _data)

// SDL_putenv is gone; SDL_setenv takes the value separately and an overwrite
// flag.
#	define D2SetEnv(_name, _value)		SDL_setenv (_name, _value, 1)

#else

#	define D2CreateThread(_fn, _data)	SDL_CreateThread (_fn, _data)
// Both arguments have to be literals here, which is all this is used for.
#	define D2SetEnv(_name, _value)		SDL_putenv (const_cast<char*> (_name "=" _value))

#endif

#endif //_SDL_COMPAT_H
