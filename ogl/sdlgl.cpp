/*
 *
 * Graphics functions for SDL-GL.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#ifdef __macosx__
# include <SDL/SDL.h>
# ifdef SDL_IMAGE
#  include <SDL/SDL_image.h>
# endif
#else
# include <SDL.h>
# ifdef SDL_IMAGE
#  include <SDL_image.h>
# endif
#endif

#include <dlfcn.h>
#include <EGL/egl.h>

#include "descent.h"
#include "ogl_defs.h"
#include "ogl_lib.h"
#include "vers_id.h"
#include "error.h"
#include "u_mem.h"
#include "args.h"
#include "gamepal.h"
#include "oof.h"
#include "descent.h"
#include "menu.h"
#include "screens.h"
#include "sdlgl.h"
#include "config.h"

//------------------------------------------------------------------------------

// SDL2 dropped the surface flags this code is written against. Map the ones we
// use onto the window flags that replaced them, so SDL_VIDEO_FLAGS keeps its
// meaning under both versions.

#if SDL_VERSION_ATLEAST (2, 0, 0)
#	ifndef SDL_OPENGL
#		define SDL_OPENGL		SDL_WINDOW_OPENGL
#	endif
#	ifndef SDL_FULLSCREEN
#		define SDL_FULLSCREEN	SDL_WINDOW_FULLSCREEN
#	endif
#	ifndef SDL_NOFRAME
#		define SDL_NOFRAME		SDL_WINDOW_BORDERLESS
#	endif
#	ifndef SDL_DOUBLEBUF
#		define SDL_DOUBLEBUF	0	// implied by SDL_GL_DOUBLEBUFFER
#	endif
#	ifndef SDL_HWSURFACE
#		define SDL_HWSURFACE	0	// no such thing any more
#	endif
#endif

#define SDL_VIDEO_FLAGS	(SDL_OPENGL | SDL_DOUBLEBUF | SDL_HWSURFACE | \
	(gameConfig.bBorderless ? SDL_NOFRAME : ogl.m_states.bFullScreen ? SDL_FULLSCREEN : 0))

//------------------------------------------------------------------------------

#if SDL_VERSION_ATLEAST (2, 0, 0)

// SDL1 kept one implicit video surface; SDL2 makes the window and the GL
// context explicit, so we own them for the lifetime of the program and reuse
// them across mode changes (recreating them would invalidate every texture).

static SDL_Window* sdlWindow = NULL;
static SDL_GLContext sdlGlContext = NULL;

SDL_Window* SdlGlGetWindow (void)
{
return sdlWindow;
}

#endif

//------------------------------------------------------------------------------

static uint16_t gammaRamp [512];

//------------------------------------------------------------------------------

void InitGammaRamp (void)
{
	int32_t i, j;
	uint16_t *pg = gammaRamp;

for (i = 256, j = 0; i; i--, j += 256, pg++)
	*pg = j;
memset (pg, 0xff, 256 * sizeof (*pg));
}

//------------------------------------------------------------------------------

int32_t SdlGlSetBrightnessInternal (void)
{
#if SDL_VERSION_ATLEAST (2, 0, 0)
// Gamma belongs to a window in SDL2, and plenty of targets (Android among them)
// have no gamma hardware to give - a failure here just means the palette
// effects do not get the extra brightness, so it is not worth an error.
if (!sdlWindow)
	return -1;
return SDL_SetWindowGammaRamp (sdlWindow,
	                            (Uint16*) (gammaRamp + paletteManager.RedEffect () * 4),
	                            (Uint16*) (gammaRamp + paletteManager.GreenEffect () * 4),
	                            (Uint16*) (gammaRamp + paletteManager.BlueEffect () * 4));
#else
return SDL_SetGammaRamp ((Uint16*) (gammaRamp + paletteManager.RedEffect () * 4),
	                      (Uint16*) (gammaRamp + paletteManager.GreenEffect () * 4),
	                      (Uint16*) (gammaRamp + paletteManager.BlueEffect () * 4));
#endif
}

//------------------------------------------------------------------------------

int32_t SdlGlVideoModeOK (int32_t w, int32_t h)
{
PrintLog (1, "checking video mode (%d X %d)\n", w, h);
#if SDL_VERSION_ATLEAST (2, 0, 0)
// SDL2 has no mode probe: a GL window is created at whatever size is asked for
// and the driver picks the pixel format, which SdlGlInitWindow reads back from
// the context it actually got.
int32_t nColorBits = FindArg ("-gl_16bpp") ? 16 : 32;
#else
int32_t nColorBits = SDL_VideoModeOK (w, h, FindArg ("-gl_16bpp") ? 16 : 32, SDL_VIDEO_FLAGS);
#endif
PrintLog (0, "SDL suggests %d bits/pixel\n", nColorBits);
if (!nColorBits) {
	PrintLog (-1);
	return 0;
	}
ogl.m_states.nColorBits = nColorBits;
PrintLog (-1);
return 1;
}

//------------------------------------------------------------------------------

int32_t SdlGlSetAttribute (const char *szSwitch, const char *szAttr, SDL_GLattr attr, int32_t value)
{
	int32_t	i;

if (szSwitch && (i = FindArg (szSwitch)) && appConfig [i + 1])
	attr = (SDL_GLattr) atoi (appConfig [i + 1]);
i = SDL_GL_SetAttribute (attr, value);
/***/PrintLog (0, "setting %s to %d %s\n", szAttr, value, (i == -1) ? "failed" : "succeeded");
return i;
}

//------------------------------------------------------------------------------

void SdlGlInitAttributes (void)
{
	int32_t t;

/***/PrintLog (1, "setting OpenGL attributes\n");
SdlGlSetAttribute ("-gl_red", "SDL_GL_RED_SIZE", SDL_GL_RED_SIZE, 8);
SdlGlSetAttribute ("-gl_green", "SDL_GL_GREEN_SIZE", SDL_GL_GREEN_SIZE, 8);
SdlGlSetAttribute ("-gl_blue", "SDL_GL_BLUE_SIZE", SDL_GL_BLUE_SIZE, 8);
SdlGlSetAttribute ("-gl_alpha", "SDL_GL_ALPHA_SIZE", SDL_GL_ALPHA_SIZE, 8);
SdlGlSetAttribute ("-gl_buffer", "SDL_GL_BUFFER_SIZE", SDL_GL_BUFFER_SIZE, 32);
SdlGlSetAttribute ("-gl_stencil", "SDL_GL_STENCIL_SIZE", SDL_GL_STENCIL_SIZE, 8);
if (0 < (t = FindArg ("-gl_depth")) && appConfig [t+1]) {
	ogl.m_states.nDepthBits = atoi (appConfig [t + 1]);
	if (ogl.m_states.nDepthBits <= 0)
		ogl.m_states.nDepthBits = 24;
	else if (ogl.m_states.nDepthBits > 24)
		ogl.m_states.nDepthBits = 24;
	SdlGlSetAttribute (NULL, "SDL_GL_DEPTH_SIZE", SDL_GL_DEPTH_SIZE, ogl.m_states.nDepthBits);
	SdlGlSetAttribute (NULL, "SDL_GL_STENCIL_SIZE", SDL_GL_STENCIL_SIZE, 8);
	}
SdlGlSetAttribute (NULL, "SDL_GL_DOUBLEBUFFER", SDL_GL_DOUBLEBUFFER, 1);
#if SDL_VERSION_ATLEAST (2, 0, 0) && defined (__ANDROID__)
// Ask for GLES 2 explicitly. Nothing here asked for a context version at all,
// and gl4es needs an ES 2 context to compile the shaders it emulates the fixed
// pipeline with - without one glCreateShader returns 0 and every compile fails
// with an empty error log, which is exactly what happened.
SdlGlSetAttribute (NULL, "SDL_GL_CONTEXT_PROFILE_MASK", SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
SdlGlSetAttribute (NULL, "SDL_GL_CONTEXT_MAJOR_VERSION", SDL_GL_CONTEXT_MAJOR_VERSION, 2);
SdlGlSetAttribute (NULL, "SDL_GL_CONTEXT_MINOR_VERSION", SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
if (ogl.m_features.bQuadBuffers/*.Apply ()*/)
	SdlGlSetAttribute (NULL, "SDL_GL_STEREO", SDL_GL_STEREO, 1);
if (ogl.m_states.bFSAA) {
	SdlGlSetAttribute (NULL, "SDL_GL_MULTISAMPLEBUFFERS", SDL_GL_MULTISAMPLEBUFFERS, 1);
	SdlGlSetAttribute (NULL, "SDL_GL_MULTISAMPLESAMPLES", SDL_GL_MULTISAMPLESAMPLES, 4);
	}
PrintLog (-1);
}

//------------------------------------------------------------------------------

int32_t SdlGlInitWindow (int32_t w, int32_t h, int32_t bForce)
{
	int32_t			bRebuild = 0;
	GLint			i;

if (ogl.m_states.bInitialized) {
	if (!bForce && (w == ogl.m_states.nCurWidth) && (h == ogl.m_states.nCurHeight) &&
		(ogl.m_states.bCurFullScreen == ogl.m_states.bFullScreen) &&
		(ogl.m_states.bCurBorderless == gameConfig.bBorderless))
		return -1;
	ogl.Update (1); // blank screen/window
	ogl.Update (1);
	if ((w != ogl.m_states.nCurWidth) || (h != ogl.m_states.nCurHeight) ||
		 (ogl.m_states.bCurFullScreen != ogl.m_states.bFullScreen) ||
		 (ogl.m_states.bCurBorderless != gameConfig.bBorderless)) {
		textureManager.Destroy ();//if we are or were fullscreen, changing vid mode will invalidate current textures
		bRebuild = 1;
		}
	}
if (w < 0)
	w = ogl.m_states.nCurWidth;
if (h < 0)
	h = ogl.m_states.nCurHeight;
if ((w < 0) || (h < 0))
	return -1;
SdlGlInitAttributes ();
#if USE_IRRLICHT
if (!IrrInit (w, h, (bool) ogl.m_states.bFullScreen))
	return 0;
#else
#if SDL_VERSION_ATLEAST (2, 0, 0)
/***/PrintLog (1, "creating SDL window (%dx%dx%d, %s)\n", w, h, ogl.m_states.nColorBits, ogl.m_states.bFullScreen ? "fullscreen" : "windowed");
if (!SdlGlVideoModeOK (w, h)) {
	PrintLog (-1);
	Error ("Could not set %dx%dx%d opengl video mode\n", w, h, ogl.m_states.nColorBits);
	return 0;
	}

// Fullscreen means the desktop resolution with the game rendered into it:
// SDL_WINDOW_FULLSCREEN would change the display mode, which is both slower to
// switch and hostile to anything sharing the screen.
uint32_t windowFlags = SDL_WINDOW_OPENGL |
					   (gameConfig.bBorderless ? SDL_WINDOW_BORDERLESS : 0) |
					   (ogl.m_states.bFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

if (!sdlWindow) {
	sdlWindow = SDL_CreateWindow ("D2X-XL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, windowFlags);
	if (!sdlWindow) {
		PrintLog (-1);
		Error ("Could not create a %dx%d window: %s\n", w, h, SDL_GetError ());
		return 0;
		}
	}
else {
	// Reuse the window: recreating it would throw away the GL context, and with
	// it every texture the caller has just decided to keep.
	SDL_SetWindowFullscreen (sdlWindow, ogl.m_states.bFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if (!ogl.m_states.bFullScreen)
		SDL_SetWindowSize (sdlWindow, w, h);
	SDL_SetWindowBordered (sdlWindow, gameConfig.bBorderless ? SDL_FALSE : SDL_TRUE);
	}

if (!sdlGlContext) {
	sdlGlContext = SDL_GL_CreateContext (sdlWindow);
	if (!sdlGlContext) {
		PrintLog (-1);
		Error ("Could not create an OpenGL context: %s\n", SDL_GetError ());
		return 0;
		}
#	ifdef __ANDROID__
	// Before any GL call. gl4es is built without its init constructor because SDL
	// owns the context, so until this runs it has no entry points to forward to
	// and the first GL call walks into a null pointer - which is what happened
	// here: this function ends by setting up the draw buffer and the GL state,
	// long before the engine reaches the extension setup where glewInit used to
	// be called.
	D2XLInitGL ();

	// And bind our context again. gl4es probes the hardware by creating a pbuffer
	// context of its own, making it current and destroying it, without putting
	// back what was there - so on return nothing is current, and every GL call
	// quietly does nothing: glCreateShader hands back 0 and glGetError reports no
	// error at all, which is what made this look like missing shader support.
	// Unbind first: SDL remembers which context it made current and will skip the
	// work if asked for the one it already believes is bound. gl4es dropped the
	// binding behind its back, so without the detour SDL does nothing, EGL has no
	// current context, and every GL call from here on quietly does nothing at all.
	SDL_GL_MakeCurrent (sdlWindow, NULL);
	if (SDL_GL_MakeCurrent (sdlWindow, sdlGlContext) < 0)
		PrintLog (0, "could not make the GL context current again: %s\n", SDL_GetError ());

	{	// What the engine will actually be drawing into, measured here rather than
		// inferred: a shader object that comes back 0 with no error means the calls
		// are going nowhere.
		int32_t nProfile = 0, nMajor = 0;
		GLuint shader;

		SDL_GL_GetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, &nProfile);
		SDL_GL_GetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, &nMajor);
		shader = glCreateShader (GL_VERTEX_SHADER);

		// The same call again, but taken straight from the driver rather than
		// through gl4es, plus what EGL thinks is current. gl4es returning 0 here
		// means the driver returned 0, and a driver does that when nothing is
		// current - so ask EGL directly rather than infer it.
		GLuint (*pfnRealCreateShader) (GLenum) = NULL;
		void* hGles = dlopen ("libGLESv2.so", RTLD_LOCAL | RTLD_NOW);
		if (hGles)
			pfnRealCreateShader = (GLuint (*) (GLenum)) dlsym (hGles, "glCreateShader");

		PrintLog (0, "context: profile %d, major %d, GL_VERSION '%s', gl4es shader %u, driver shader %u, EGL ctx %p, err 0x%x\n",
		          nProfile, nMajor, (const char*) glGetString (GL_VERSION), shader,
		          pfnRealCreateShader ? pfnRealCreateShader (GL_VERTEX_SHADER) : 0,
		          (void*) eglGetCurrentContext (), glGetError ());
		if (shader)
			glDeleteShader (shader);
	}
#	endif
	}
PrintLog (-1);
#else
SDL_putenv (const_cast<char*>("SDL_VIDEO_CENTERED=1"));
/***/PrintLog (1, "setting SDL video mode (%dx%dx%d, %s)\n", w, h, ogl.m_states.nColorBits, ogl.m_states.bFullScreen ? "fullscreen" : "windowed");
if (!SdlGlVideoModeOK (w, h) ||
	 !SDL_SetVideoMode (w, h, ogl.m_states.nColorBits, SDL_VIDEO_FLAGS)) {
	PrintLog (-1);
	Error ("Could not set %dx%dx%d opengl video mode\n", w, h, ogl.m_states.nColorBits);
	return 0;
	}
PrintLog (-1);
#endif
#endif
#if SDL_VERSION_ATLEAST (2, 0, 0)
// SDL2 has no video_mem to ask about, so the texture quality cap stays at
// whatever the config asked for. Read the pixel format back from the context
// instead of the GL state: GL_RED_BITS and friends are legacy queries that a
// core or ES profile need not answer.
	{
		int32_t nRed = 0, nGreen = 0, nBlue = 0, nAlpha = 0;
		SDL_GL_GetAttribute (SDL_GL_RED_SIZE, &nRed);
		SDL_GL_GetAttribute (SDL_GL_GREEN_SIZE, &nGreen);
		SDL_GL_GetAttribute (SDL_GL_BLUE_SIZE, &nBlue);
		SDL_GL_GetAttribute (SDL_GL_ALPHA_SIZE, &nAlpha);
		int32_t nDepth = 0, nStencil = 0;
		SDL_GL_GetAttribute (SDL_GL_DEPTH_SIZE, &nDepth);
		SDL_GL_GetAttribute (SDL_GL_STENCIL_SIZE, &nStencil);
		ogl.m_states.nColorBits = nRed + nGreen + nBlue + nAlpha;
		ogl.m_states.nDepthBits = nDepth;
		ogl.m_states.nStencilBits = nStencil;
		(void) i;
	}
#else
const SDL_VideoInfo* pVideoInfo = SDL_GetVideoInfo ();
if (pVideoInfo->video_mem) {
	if (pVideoInfo->video_mem < 256 * 1024 * 1024)
		gameStates.render.nMaxTextureQuality = 1;
	else if (pVideoInfo->video_mem < 512 * 1024 * 1024)
		gameStates.render.nMaxTextureQuality = 2;
	}
ogl.m_states.nColorBits = 0;
glGetIntegerv (GL_RED_BITS, &i);
ogl.m_states.nColorBits += i;
glGetIntegerv (GL_GREEN_BITS, &i);
ogl.m_states.nColorBits += i;
glGetIntegerv (GL_BLUE_BITS, &i);
ogl.m_states.nColorBits += i;
glGetIntegerv (GL_ALPHA_BITS, &i);
ogl.m_states.nColorBits += i;
glGetIntegerv (GL_DEPTH_BITS, &ogl.m_states.nDepthBits);
glGetIntegerv (GL_STENCIL_BITS, &ogl.m_states.nStencilBits);
#endif
ogl.m_features.bStencilBuffer = (ogl.m_states.nStencilBits > 0);
if (!ogl.m_features.bQuadBuffers/*.Apply ()*/)
	ogl.m_states.nStereo = 0;
else {
	glGetIntegerv (GL_STEREO, &ogl.m_states.nStereo);
	ogl.m_features.bStereoBuffers = (ogl.m_states.nStereo > 0);
	}
SDL_ShowCursor (0);
ogl.m_states.nCurWidth = w;
ogl.m_states.nCurHeight = h;
ogl.m_states.bCurFullScreen = ogl.m_states.bFullScreen;
ogl.m_states.bCurBorderless = gameConfig.bBorderless;
if (ogl.m_states.bInitialized && bRebuild) {
	ogl.SetViewport (0, 0, w, h);
	if (gameStates.app.bGameRunning) {
		//paletteManager.ResumeEffect ();
		ogl.RebuildContext (1);
		}
	else
		fontManager.Remap ();
	}
D2SetCaption ();
ogl.SelectDrawBuffer (0);
ogl.InitState ();
ogl.m_states.bInitialized = 1;
return 1;
}

//------------------------------------------------------------------------------

void SdlGlDestroyWindow (void)
{
if (ogl.m_states.bInitialized) {
	ResetTextures (0, 0);
#if !USE_IRRLICHT
	SDL_ShowCursor (1);
#endif
	}
}

//------------------------------------------------------------------------------

void SdlGlDoFullScreenInternal (int32_t bForce)
{
SdlGlInitWindow (ogl.m_states.nCurWidth, ogl.m_states.nCurHeight, bForce);
}

//------------------------------------------------------------------------------

void SdlGlSwapBuffersInternal (void)
{
#if !USE_IRRLICHT
#	if SDL_VERSION_ATLEAST (2, 0, 0)
if (sdlWindow)
	SDL_GL_SwapWindow (sdlWindow);
#	else
SDL_GL_SwapBuffers ();
#	endif
#endif
}

//------------------------------------------------------------------------------

void SdlGlClose (void)
{
SdlGlDestroyWindow ();
#if SDL_VERSION_ATLEAST (2, 0, 0)
// SdlGlDestroyWindow is called on every mode change, so the window and context
// only go away here, at shutdown.
if (sdlGlContext) {
	SDL_GL_DeleteContext (sdlGlContext);
	sdlGlContext = NULL;
	}
if (sdlWindow) {
	SDL_DestroyWindow (sdlWindow);
	sdlWindow = NULL;
	}
ogl.m_states.bInitialized = 0;
#endif
}

//------------------------------------------------------------------------------
