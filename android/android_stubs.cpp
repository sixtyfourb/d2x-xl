// Stand-ins for things the desktop build has and the Android build does not.

#include "gl_android.h"

#include <SDL.h>
#include "descent.h"

//------------------------------------------------------------------------------

// main/update.cpp is not built here: it fetches a new build over libcurl, which
// is not how an APK is updated, and libcurl is not part of the NDK. The main
// menu still asks.

int32_t CheckForUpdate (void)
{
return 0;
}

//------------------------------------------------------------------------------

// Two-sided stencil, from GL_EXT_stencil_two_side. gl4es advertises neither that
// extension nor GL_ATI_separate_stencil, so SetupStencilOps leaves
// bSeparateStencilOps at 0 and the shadow code takes its per-face path instead -
// this exists to satisfy the linker. If it ever runs, the detection and the
// renderer have disagreed, so say so rather than silently drawing wrong shadows.

extern "C" void APIENTRY glActiveStencilFaceEXT (GLenum face)
{
static bool bWarned = false;

if (!bWarned) {
	bWarned = true;
	SDL_Log ("glActiveStencilFaceEXT called, but gl4es has no two-sided stencil");
	}
(void) face;
}

//------------------------------------------------------------------------------
//eof
