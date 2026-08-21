#ifndef _GL_ANDROID_H
#define _GL_ANDROID_H

// The GL headers for the Android build, in one place.
//
// D2X-XL wants desktop GL - GLSL, immediate mode, FBOs - none of which GLES
// offers, so the renderer runs through gl4es, which implements those entry
// points on top of GLES. That means there is no extension loader to include
// (GLEW) and no GLX: __unix__ is defined on Android too, and the branch
// ogl_defs.h takes for it would pull in X11.
//
// gl4es ships a GL 1.x gl.h, so GL_GLEXT_PROTOTYPES is what declares the later
// entry points. A few of those it declares only under an ARB or EXT suffix
// while exporting the core name as well; those are declared here.

#define GL_GLEXT_PROTOTYPES 1

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

#ifdef __cplusplus
extern "C" {
#endif

GLAPI void APIENTRY glDrawBuffers (GLsizei n, const GLenum *bufs);

// Brings gl4es up. Must be called once the GL context exists, and stands in for
// glewInit(); see android/gl_android.cpp.
void D2XLInitGL (void);

#ifdef __cplusplus
}
#endif

#endif //_GL_ANDROID_H
