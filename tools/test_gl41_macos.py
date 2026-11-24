"""macOS's OpenGL stops at 4.1: the desktop renderer must start and draw there (port/linux/src/gl.h, gl_functions.c,
d3d8_gl.c...). Nothing in CI runs the game on a Mac, so two source checks stand in for one:

- every desktop GL function the game loads is classified here by the OpenGL version that brought it in, and every
  one newer than 4.1 is optional in gl_functions.c (GL_OPTIONAL_FUNCTION: a NULL there is no failure). A function
  added to gl.h and not classified here fails, so whoever adds it looks up its version;
- every call of an optional function, in the code the macOS build compiles (the preprocessor's blocks read with
  macOS's macros: __APPLE__, HALO_GL_NO_CLIP_CONTROL, HALO_GL_LEGACY_BINDING; not HALO_GLES / HALO_WEB / _WIN32),
  is guarded: the function's own pointer, or a capability that stands for it (GUARDS), named earlier in the same
  function or on the call's line.

0.5.2 and 0.5.3 fixed two Macs that drew nothing: glCreateQueries required (4.5), then glBufferStorage called with no
check (4.4). Both fail here.

    python3 -m pytest -q tools/test_gl41_macos.py
    AE_GL41_ROOT=<a tree> python3 tools/test_gl41_macos.py     (another tree's report)
"""
import os
import re
import sys
from pathlib import Path

ROOT = Path(os.environ.get("AE_GL41_ROOT") or Path(__file__).resolve().parent.parent)
GL_H = "port/linux/src/gl.h"
GL_FUNCTIONS_C = "port/linux/src/gl_functions.c"
SOURCES = ("port/linux/src", "port/linux/game", "port/macos/src")

# the desktop functions newer than OpenGL 4.1, by the version that brought them into core
NEWER = {
    "glMemoryBarrier": "4.2", "glCopyImageSubData": "4.3", "glDebugMessageCallback": "4.3",
    "glVertexAttribFormat": "4.3", "glVertexAttribIFormat": "4.3", "glVertexAttribBinding": "4.3",
    "glBindVertexBuffer": "4.3", "glBufferStorage": "4.4", "glBindTextures": "4.4", "glClipControl": "4.5",
    "glCreateQueries": "4.5", "glGetQueryBufferObjectuiv": "4.5",
}
# the ones in OpenGL 4.1 core or older (macOS has them)
IN_41 = set("""
glGetString glGetIntegerv glGetTexImage glGenerateMipmap glGetError glEnable glDisable glViewport glDepthRange glScissor
glClearColor glClearDepth glClearStencil glClear glColorMask glDepthMask glDepthFunc glStencilFunc glStencilOp
glStencilMask glBlendFunc glBlendFuncSeparate glIsEnabled glBlendEquation glBlendColor glCullFace glFrontFace
glPolygonMode glPolygonOffset glLineWidth glPixelStorei glReadPixels glFinish glFlush glGenTextures glDeleteTextures
glBindTexture glActiveTexture glTexImage2D glTexImage3D glTexSubImage2D glCompressedTexImage2D glCompressedTexImage3D
glTexParameteri glTexParameteriv glTexParameterf glTexParameterfv glGenSamplers glBindSampler glSamplerParameteri
glSamplerParameterf glSamplerParameterfv glGenFramebuffers glDeleteFramebuffers glBindFramebuffer
glFramebufferTexture2D glCheckFramebufferStatus glGetFramebufferAttachmentParameteriv glBlitFramebuffer glDrawBuffers
glGenRenderbuffers glDeleteRenderbuffers glBindRenderbuffer glRenderbufferStorageMultisample glFramebufferRenderbuffer
glGenBuffers glDeleteBuffers glBindBuffer glBufferData glBufferSubData glMapBufferRange glUnmapBuffer glBindBufferBase
glGenVertexArrays glBindVertexArray glEnableVertexAttribArray glDisableVertexAttribArray glVertexAttribPointer
glVertexAttribIPointer glVertexAttrib4fv glVertexAttribI4ui glDrawArrays glDrawElements glDrawElementsBaseVertex
glCreateShader glShaderSource glCompileShader glGetShaderiv glGetShaderInfoLog glDeleteShader glDeleteProgram
glCreateProgram glAttachShader glBindAttribLocation glBindFragDataLocation glLinkProgram glGetProgramiv
glGetProgramInfoLog glProgramParameteri glGetProgramBinary glProgramBinary glUseProgram glGetUniformLocation
glUniform1i glUniform1iv glUniform1f glUniform4fv glUniform2f glGenQueries glBeginQuery glEndQuery glGetQueryObjectuiv
glIsQuery glFenceSync glClientWaitSync glDeleteSync
""".split())
# a capability that stands for a function's pointer (set from it where the context is made)
GUARDS = {"glCopyImageSubData": ("HOST_GL_COPY_IMAGE",),
          # (the query buffer's mapping: made only where glBufferStorage and glGetQueryBufferObjectuiv both are,
          # visibility_results_initialize in d3d8_gl.c)
          "glGetQueryBufferObjectuiv": ("visibility_mapped",)}
# macOS's build: what its preprocessor has defined
MACOS_DEFINED = {"__APPLE__", "HALO_GL_NO_CLIP_CONTROL", "HALO_GL_LEGACY_BINDING"}


def desktop_functions():
    """gl.h's desktop list (the GL_FUNCTIONS after #else, not OpenGL ES's)"""
    text = (ROOT / GL_H).read_text()
    blocks = re.findall(r"#define GL_FUNCTIONS\(X\)(.*?)(?=\n#)", text, re.S)
    assert len(blocks) == 2, "gl.h: expected OpenGL ES's and the desktop's GL_FUNCTIONS"
    return re.findall(r"X\((gl\w+)\)", blocks[1])


def optional_functions():
    text = (ROOT / GL_FUNCTIONS_C).read_text()
    block = re.search(r"#define GL_OPTIONAL_FUNCTION\(name\)(.*?)\n(?!\t)", text, re.S)
    assert block, "gl_functions.c: no GL_OPTIONAL_FUNCTION"
    return set(re.findall(r'"(gl\w+)"', block.group(1)))


def _strip_comments(lines):
    """the lines with /* */ and // comments and string literals blanked (line count kept)"""
    out, inside = [], False
    for line in lines:
        result, at = "", 0
        while at < len(line):
            if inside:
                end = line.find("*/", at)
                if end < 0:
                    at = len(line)
                    break
                inside, at = False, end + 2
                continue
            start = line.find("/*", at)
            slash = line.find("//", at)
            if slash >= 0 and (start < 0 or slash < start):
                result += line[at:slash]
                break
            if start < 0:
                result += line[at:]
                break
            result += line[at:start]
            inside, at = True, start + 2
        out.append(re.sub(r'"(?:\\.|[^"\\])*"', '""', result))
    return out


def _condition(expression, defined):
    """an #if expression of defined()/!/&&/|| over macOS's macros; None when it holds anything else"""
    text = re.sub(r"defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)",
                  lambda m: "1" if (m.group(1) or m.group(2)) in defined else "0", expression)
    text = text.replace("&&", " and ").replace("||", " or ")
    text = re.sub(r"!(?!=)", " not ", text)
    if re.search(r"[^\s01()andortn]", text.replace("and", "").replace("not", "").replace("or", "")):
        return None
    try:
        return bool(eval(text, {"__builtins__": {}}))
    except SyntaxError:
        return None


def macos_lines(path):
    """the file's lines, comments blanked, with whether macOS's build compiles each"""
    lines = _strip_comments(path.read_text(errors="replace").splitlines())
    defined, stack, result = set(MACOS_DEFINED), [], []
    for line in lines:
        directive = re.match(r"\s*#\s*(\w+)\s*(.*)", line)
        active = all(state for state, _ in stack)
        if directive:
            word, rest = directive.group(1), directive.group(2).strip()
            if word in ("ifdef", "ifndef"):
                holds = (rest.split()[0] in defined) == (word == "ifdef")
                stack.append((holds, holds))
            elif word == "if":
                holds = _condition(rest, defined)
                holds = True if holds is None else holds   # (unknown: read as compiled, checked anyway)
                stack.append((holds, holds))
            elif word == "elif" and stack:
                _, taken = stack[-1]
                holds = _condition(rest, defined)
                holds = (True if holds is None else holds) and not taken
                stack[-1] = (holds, taken or holds)
            elif word == "else" and stack:
                _, taken = stack[-1]
                stack[-1] = (not taken, True)
            elif word == "endif" and stack:
                stack.pop()
            elif word == "define" and active and rest:
                defined.add(rest.split()[0].split("(")[0])
            elif word == "undef" and active and rest:
                defined.discard(rest.split()[0])
            result.append(("", False))
            continue
        result.append((line, active))
    return result


def unguarded_calls():
    """(file:line, function) for each optional call macOS's build compiles with no guard before it in its function"""
    found = []
    names = sorted(NEWER)
    for folder in SOURCES:
        for path in sorted((ROOT / folder).glob("*.c")) if (ROOT / folder).is_dir() else ():
            if path.name in ("gl_functions.c",):
                continue
            lines = macos_lines(path)
            start = 0
            for number, (line, active) in enumerate(lines):
                # (a function's body starts at a "{" in the first column; its end, "}")
                if line.startswith("{"):
                    start = number
                if not active:
                    continue
                for name in names:
                    for call in re.finditer(r"\b%s\s*\(" % name, line):
                        tokens = (name,) + GUARDS.get(name, ())
                        before = [text for text, on in lines[start:number] if on] + [line[:call.start()]]
                        guarded = any(re.search(r"\b%s\b(?!\s*\()" % re.escape(token), text)
                                      for text in before for token in tokens)
                        if not guarded:
                            found.append((f"{path.relative_to(ROOT)}:{number + 1}", name))
    return found


def test_every_desktop_function_is_classified():
    unknown = [name for name in desktop_functions() if name not in NEWER and name not in IN_41]
    assert not unknown, f"gl.h loads functions this test doesn't know the OpenGL version of: {unknown}"


def test_newer_than_41_are_optional():
    optional = optional_functions()
    required = [f"{name} ({NEWER[name]})" for name in desktop_functions() if name in NEWER and name not in optional]
    assert not required, f"required, but newer than macOS's OpenGL 4.1 (add to GL_OPTIONAL_FUNCTION): {required}"


def test_optional_calls_are_guarded_on_macos():
    calls = unguarded_calls()
    assert not calls, f"called with no check on macOS's OpenGL 4.1 (NULL there): {calls}"


if __name__ == "__main__":
    optional = optional_functions()
    print("required newer than 4.1:", [n for n in desktop_functions() if n in NEWER and n not in optional])
    print("unclassified:", [n for n in desktop_functions() if n not in NEWER and n not in IN_41])
    print("unguarded on macOS:", unguarded_calls())
    sys.exit(0)
