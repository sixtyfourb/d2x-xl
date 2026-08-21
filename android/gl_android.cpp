// GL support for the Android build: bringing gl4es up, and the three GLU
// functions D2X-XL uses. gl4es declares GLU in its headers but implements none
// of it, so they would be undefined at link time.

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "gl_android.h"
#include <gl4esinit.h>
#include <SDL.h>

//------------------------------------------------------------------------------

// gl4es is built without its init constructor, because SDL owns the EGL context:
// this has to run once that context exists, and before any GL call.

void D2XLInitGL (void)
{
static bool bInitialized = false;

if (bInitialized)
	return;
bInitialized = true;
set_getprocaddress ((void* (*) (const char*)) SDL_GL_GetProcAddress);
initialize_gl4es ();
}

//------------------------------------------------------------------------------

void GLAPIENTRY gluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
GLdouble h = tan (fovy * M_PI / 360.0) * zNear;
GLdouble w = h * aspect;

glFrustum (-w, w, -h, h, zNear, zFar);
}

//------------------------------------------------------------------------------

// Object space to window space, the standard way: model then projection, divide
// through by w, then map the -1..1 cube onto the viewport.

GLint GLAPIENTRY gluProject (GLdouble objX, GLdouble objY, GLdouble objZ,
                             const GLdouble* model, const GLdouble* proj, const GLint* view,
                             GLdouble* winX, GLdouble* winY, GLdouble* winZ)
{
GLdouble in [4] = { objX, objY, objZ, 1.0 };
GLdouble out [4];
int32_t i;

for (i = 0; i < 4; i++)
	out [i] = in [0] * model [i] + in [1] * model [4 + i] + in [2] * model [8 + i] + in [3] * model [12 + i];
for (i = 0; i < 4; i++)
	in [i] = out [0] * proj [i] + out [1] * proj [4 + i] + out [2] * proj [8 + i] + out [3] * proj [12 + i];
if (in [3] == 0.0)
	return GL_FALSE;
in [0] /= in [3];
in [1] /= in [3];
in [2] /= in [3];
*winX = view [0] + (1.0 + in [0]) * view [2] / 2.0;
*winY = view [1] + (1.0 + in [1]) * view [3] / 2.0;
*winZ = (1.0 + in [2]) / 2.0;
return GL_TRUE;
}

//------------------------------------------------------------------------------

const GLubyte* GLAPIENTRY gluErrorString (GLenum error)
{
switch (error) {
	case GL_NO_ERROR:
		return (const GLubyte*) "no error";
	case GL_INVALID_ENUM:
		return (const GLubyte*) "invalid enumerant";
	case GL_INVALID_VALUE:
		return (const GLubyte*) "invalid value";
	case GL_INVALID_OPERATION:
		return (const GLubyte*) "invalid operation";
	case GL_STACK_OVERFLOW:
		return (const GLubyte*) "stack overflow";
	case GL_STACK_UNDERFLOW:
		return (const GLubyte*) "stack underflow";
	case GL_OUT_OF_MEMORY:
		return (const GLubyte*) "out of memory";
	default:
		return (const GLubyte*) "unknown error";
	}
}

//------------------------------------------------------------------------------
//eof
