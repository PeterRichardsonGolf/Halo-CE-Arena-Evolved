/*
D3D8_SHADER_PREP.H

What d3d8_gl.c has of a map's shaders made while its loading screen is up
(d3d8_shader_prep.inl, included at the end of d3d8_gl.c, whose state it
shares): the declarations its few hooks use, and, in a build without it
(HALO_SHADER_PREP is not defined: OpenGL ES, the web, the consoles, the
dedicated server), the hooks as nothing.

The hooks, in d3d8_gl.c:
- D3DDevice_CreateVertexShader: prep_vertex_program_made, to find the vertex
  programs a key names.
- vertex_shader_get and fragment_shader_get: the shader's compile asked for
  and not waited on, while the loading screen makes one (prep_compile), with
  the packed mask the key names (prep_packed_mask).
- program_get: a program the loading screen linked already (prep_prelinked).
- prepare_draw: the first draw of a program in a map notes its key in the
  map's own list (prep_note).
- D3DDevice_Present: prep_present, each frame.
*/

#ifndef __HALO_LINUX_D3D8_SHADER_PREP_H
#define __HALO_LINUX_D3D8_SHADER_PREP_H

#ifdef HALO_SHADER_PREP

struct vertex_shader_object;
struct nv2a_pixel_shader_key;

/* every program's draw is noted again for each map: a program's noted is this when it is (prep_note) */
static unsigned long shader_prep_serial = 1;
/* the program the loading screen linked for the program_get now running, or 0 */
static GLuint prep_prelinked;

static void prep_vertex_program_made(struct vertex_shader_object *object);
static void prep_note(unsigned long *noted, struct vertex_shader_object *object, BOOL immediate, BOOL lit,
	unsigned long packed_mask, const struct nv2a_pixel_shader_key *pixel);
static void prep_present(void);
static unsigned long prep_packed_mask(void);
static GLuint prep_compile(GLenum type, const char *source, const char *what);

#else

#define shader_prep_serial 0UL
#define prep_prelinked 0
#define prep_vertex_program_made(object) ((void)0)
#define prep_note(noted, object, immediate, lit, packed_mask, pixel) ((void)0)
#define prep_present() ((void)0)
#define prep_packed_mask() (device.vertex_shader->packed_mask)
#define prep_compile xgpu_compile_shader

#endif

#endif
