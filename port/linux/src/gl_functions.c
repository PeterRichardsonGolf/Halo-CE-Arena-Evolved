/*
GL_FUNCTIONS.C

Run-time resolution of the OpenGL entry points listed in gl.h.
*/

#include "platform.h"
#define GL_FUNCTIONS_DEFINE
#include "gl.h"

#include <SDL3/SDL.h>
#include <string.h>

void ae_gl_functions_count(void); /* AE hook */

#define GL_DEFINE_FUNCTION(name) __typeof__(&name) halo_##name;
GL_FUNCTIONS(GL_DEFINE_FUNCTION)
#ifdef HALO_GLES
GL_OPTIONAL_FUNCTIONS(GL_DEFINE_FUNCTION)
#endif

int gl_functions_load(void)
{
	int success = TRUE;

/* newer than OpenGL 4.1 (macOS's newest); callers check for NULL */
#define GL_OPTIONAL_FUNCTION(name) \
	(!strcmp(#name, "glClipControl") || !strcmp(#name, "glCopyImageSubData") || \
		!strcmp(#name, "glDebugMessageCallback") || !strcmp(#name, "glBufferStorage") || \
		!strcmp(#name, "glMemoryBarrier") || !strcmp(#name, "glVertexAttribFormat") || \
		!strcmp(#name, "glVertexAttribIFormat") || !strcmp(#name, "glVertexAttribBinding") || \
		!strcmp(#name, "glBindVertexBuffer") || !strcmp(#name, "glGetQueryBufferObjectuiv") || \
		!strcmp(#name, "glBindTextures"))
#ifdef HALO_WEB
/* WebGL 2 is OpenGL ES 3.0: the 3.1 and 3.2 functions it lacks
(glCopyImageSubData, glDrawElementsBaseVertex, glMemoryBarrier) stay NULL,
and the renderer calls them only where its capabilities say the context
has them (xgpu_capabilities, d3d8_gl.c) */
#define GL_MISSING_FAILS 0
#else
#define GL_MISSING_FAILS 1
#endif
#define GL_LOAD_FUNCTION(name) \
	halo_##name = (__typeof__(halo_##name))SDL_GL_GetProcAddress(#name); \
	if (!halo_##name && !GL_OPTIONAL_FUNCTION(name)) \
	{ \
		if (GL_MISSING_FAILS) \
			platform_log("OpenGL function %s is unavailable", #name); \
		success = success && !GL_MISSING_FAILS; \
	}
	GL_FUNCTIONS(GL_LOAD_FUNCTION)
#undef GL_LOAD_FUNCTION
#if defined(HALO_GLES) && !defined(HALO_WEB)
	/* a missing one is no failure: the renderer uses each only where the
	context lists its extension (gl_initialize, d3d8_gl.c). WebGL has none of
	these, and its table is not asked */
#define GL_LOAD_OPTIONAL_FUNCTION(name) \
	halo_##name = (__typeof__(halo_##name))SDL_GL_GetProcAddress(#name);
	GL_OPTIONAL_FUNCTIONS(GL_LOAD_OPTIONAL_FUNCTION)
#undef GL_LOAD_OPTIONAL_FUNCTION
#endif
	ae_gl_functions_count(); /* AE hook */
	return success;
}

/* AE: the measuring build's counts of the GL calls that can wait on the driver (HALO_AE_FRAME_TIMES,
ae_frame_times.h): each counted, and the time inside them, once installed over the loaded pointers */
#include "ae_frame_times.h"

#ifndef HALO_GLES
#define AE_GL_COUNTED(name, counter, result_type, parameters, arguments) \
	static __typeof__(&name) ae_real_##name; \
	static result_type GLAPIENTRY ae_count_##name parameters \
	{ \
		result_type result; \
		ae_ft_count(counter); \
		ae_ft_enter(AE_FT_GL_COUNTED); \
		result = ae_real_##name arguments; \
		ae_ft_leave(AE_FT_GL_COUNTED); \
		return result; \
	}
#define AE_GL_COUNTED_VOID(name, counter, parameters, arguments) \
	static __typeof__(&name) ae_real_##name; \
	static void GLAPIENTRY ae_count_##name parameters \
	{ \
		ae_ft_count(counter); \
		ae_ft_enter(AE_FT_GL_COUNTED); \
		ae_real_##name arguments; \
		ae_ft_leave(AE_FT_GL_COUNTED); \
	}

AE_GL_COUNTED_VOID(glGetIntegerv, AE_FC_GL_GET_INTEGER, (GLenum a, GLint *b), (a, b))
AE_GL_COUNTED(glIsEnabled, AE_FC_GL_IS_ENABLED, GLboolean, (GLenum a), (a))
AE_GL_COUNTED(glGetError, AE_FC_GL_GET_ERROR, GLenum, (void), ())
AE_GL_COUNTED_VOID(glReadPixels, AE_FC_GL_READ_PIXELS, (GLint a, GLint b, GLsizei c, GLsizei d, GLenum e, GLenum f,
	void *g), (a, b, c, d, e, f, g))
AE_GL_COUNTED_VOID(glFinish, AE_FC_GL_FINISH, (void), ())
AE_GL_COUNTED_VOID(glFlush, AE_FC_GL_FLUSH, (void), ())
AE_GL_COUNTED_VOID(glGetQueryObjectuiv, AE_FC_GL_QUERY_RESULT, (GLuint a, GLenum b, GLuint *c), (a, b, c))
AE_GL_COUNTED_VOID(glGetQueryBufferObjectuiv, AE_FC_GL_QUERY_RESULT, (GLuint a, GLuint b, GLenum c, GLintptr d),
	(a, b, c, d))
AE_GL_COUNTED(glClientWaitSync, AE_FC_GL_CLIENT_WAIT, GLenum, (GLsync a, GLbitfield b, GLuint64 c), (a, b, c))
AE_GL_COUNTED(glMapBufferRange, AE_FC_GL_MAP_BUFFER, void *, (GLenum a, GLintptr b, GLsizeiptr c, GLbitfield d),
	(a, b, c, d))
AE_GL_COUNTED_VOID(glGetTexImage, AE_FC_GL_GET_TEX_IMAGE, (GLenum a, GLint b, GLenum c, GLenum d, void *e),
	(a, b, c, d, e))
AE_GL_COUNTED(glCheckFramebufferStatus, AE_FC_GL_FRAMEBUFFER_STATUS, GLenum, (GLenum a), (a))
AE_GL_COUNTED_VOID(glGetFramebufferAttachmentParameteriv, AE_FC_GL_GET_OTHER, (GLenum a, GLenum b, GLenum c,
	GLint *d), (a, b, c, d))
AE_GL_COUNTED_VOID(glGetShaderiv, AE_FC_GL_GET_OTHER, (GLuint a, GLenum b, GLint *c), (a, b, c))
AE_GL_COUNTED_VOID(glGetProgramiv, AE_FC_GL_GET_OTHER, (GLuint a, GLenum b, GLint *c), (a, b, c))
AE_GL_COUNTED(glGetUniformLocation, AE_FC_GL_GET_OTHER, GLint, (GLuint a, const GLchar *b), (a, b))
AE_GL_COUNTED(glIsQuery, AE_FC_GL_GET_OTHER, GLboolean, (GLuint a), (a))
#endif

void ae_gl_functions_count(void)
{
#ifndef HALO_GLES
	if (!ae_ft_on())
		return;
#define AE_GL_INSTALL(name) \
	if (halo_##name && !ae_real_##name) \
	{ \
		ae_real_##name = halo_##name; \
		halo_##name = (__typeof__(halo_##name))ae_count_##name; \
	}
	AE_GL_INSTALL(glGetIntegerv)
	AE_GL_INSTALL(glIsEnabled)
	AE_GL_INSTALL(glGetError)
	AE_GL_INSTALL(glReadPixels)
	AE_GL_INSTALL(glFinish)
	AE_GL_INSTALL(glFlush)
	AE_GL_INSTALL(glGetQueryObjectuiv)
	AE_GL_INSTALL(glGetQueryBufferObjectuiv)
	AE_GL_INSTALL(glClientWaitSync)
	AE_GL_INSTALL(glMapBufferRange)
	AE_GL_INSTALL(glGetTexImage)
	AE_GL_INSTALL(glCheckFramebufferStatus)
	AE_GL_INSTALL(glGetFramebufferAttachmentParameteriv)
	AE_GL_INSTALL(glGetShaderiv)
	AE_GL_INSTALL(glGetProgramiv)
	AE_GL_INSTALL(glGetUniformLocation)
	AE_GL_INSTALL(glIsQuery)
#undef AE_GL_INSTALL
#endif
}
