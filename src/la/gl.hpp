#ifndef __LA_GL_HEADER_GUARD
#define __LA_GL_HEADER_GUARD

#include <assert.h>

// -------------------- C++ feature detection ---------------------------------
// MSVC
#if defined(_MSC_VER)
#   define COMPILER_MSVC _MSC_VER

#   if defined(_MSVC_LANG)
#       define CPP_VERSION _MSVC_LANG
#   else
#       define CPP_VERSION __cplusplus
#   endif

#   if CPP_VERSION >= 202002L
#       define __LA_CXX_20
#       define __LA_CXX_17
#   elif CPP_VERSION >= 201703L
#       define __LA_CXX_17
#   endif

// Clang
#elif defined(__clang__)
#   if __cplusplus >= 202002L
#       define __LA_CXX_20
#       define __LA_CXX_17
#   elif __cplusplus >= 201703L
#       define __LA_CXX_17
#   endif

// GCC
#elif defined(__GNUC__)
#   if __cplusplus >= 202002L
#       define __LA_CXX_20
#       define __LA_CXX_17
#   elif __cplusplus >= 201703L
#       define __LA_CXX_17
#   endif
#endif


// -------------------- Attribute/constexpr defines ---------------------------

#ifdef __LA_CXX_20
#   define __LA_NO_DISCARD [[nodiscard]]
#   define __LA_CONSTEXPR constexpr
#   define __LA_CONSTEXPR_VAR constexpr
#   define __LA_CONSTEVAL consteval

#elif defined(__LA_CXX_17)
#   define __LA_NO_DISCARD [[nodiscard]]
#   define __LA_CONSTEXPR constexpr
#   define __LA_CONSTEXPR_VAR constexpr
#   define __LA_CONSTEVAL constexpr // no consteval in C++17

#else
#   define __LA_NO_DISCARD
#   define __LA_CONSTEXPR inline
#   define __LA_CONSTEXPR_VAR const
#   define __LA_CONSTEVAL __LA_CONSTEXPR
#endif

// ======================= OpenGL Macros ======================================

#pragma region OpenGL_Macros

#if defined(_WIN32) && !defined(LA_GL_APIENTRY) && !defined(__CYGWIN__) &&    \
                                                     !defined(__SCITECH_SNAP__)
#   define LA_GL_APIENTRY __stdcall
#endif

#ifndef LA_GL_APIENTRY
#   define LA_GL_APIENTRY
#endif
#ifndef LA_GL_APIENTRY_P
#   define LA_GL_APIENTRY_P LA_GL_APIENTRY *
#endif

#ifdef _MSC_VER
#   ifdef __has_include
#       if __has_include(<winapifamily.h>)
#           define LA_GL_HAVE_WINAPIFAMILY 1
#       endif
#   elif _MSC_VER >= 1700 && !_USING_V110_SDK71_
#       define LA_GL_HAVE_WINAPIFAMILY 1
#   endif
#endif

#ifdef LA_GL_HAVE_WINAPIFAMILY
#   include <winapifamily.h>
#   if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) &&                  \
        WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#       define IS_UWP 1
#   endif
#endif

#pragma endregion OpenGL_Macros

// ========================== opengl ==========================================

namespace la {
namespace gl {
using LoadProc = void* (*)(const char* name);
extern LoadProc loader;
    
using Enum = unsigned int;
using Boolean = unsigned char;

__LA_CONSTEXPR_VAR struct /* BufferBit */ {
    __LA_CONSTEXPR_VAR static int DEPTH = 0x00000100;
    __LA_CONSTEXPR_VAR static int STENCIL = 0x00000400;
    __LA_CONSTEXPR_VAR static int COLOR = 0x00004000;
} BUFFER_BIT;

enum class Face : unsigned int {
    Front = 0x404,
    Back = 0x405,
    FrontAndBack = 0x408,
};

enum class PolygonMode : unsigned int {
    Point = 0x1B00,
    Line = 0x1B01,
    Fill = 0x1B02,
};

enum class Capability : unsigned int {
    Blend = 0x0BE2,
    CullFace = 0x0B44,
    DepthTest = 0x0B71,
    // others
};

enum class BlendFactor : unsigned int {
    Zero = 0,
    One = 1,
    SrcAlpha = 0x0302,
    OneMinusSrcAlpha = 0x0303,
    // others
};

enum class TextureTarget : unsigned int {
    Texture2D = 0xDE1,
    // others
};

enum class TextureParam : unsigned int {
    MinFilter = 0x2801,
    MagFilter = 0x2800,
    WrapS = 0x2802,
    WrapT = 0x2803,
    // others
};

enum class TextureFormat : unsigned int {
    RGBA = 0x1908,
    RGB = 0x1907,
};

enum class DataType : unsigned int {
    UnsignedByte = 0x1401,
};

enum class GetParam : unsigned int {
    Viewport = 0x0BA2,
    // others
};

enum class PrimitiveMode : unsigned int {
    Triangles = 0x0004,
    TriangleStrip = 0x0005,
    Lines = 0x0001,
    LineStrip = 0x0003,
    Points = 0x0000,
    // others
};

enum class DrawElementsType : unsigned int {
    UnsignedByte = 0x1401,
    UnsignedShort = 0x1403,
    UnsignedInt = 0x1405
};

enum class BufferTarget : unsigned int {
    ArrayBuffer = 0x8892,
    ElementArrayBuffer = 0x8893,
    UniformBuffer = 0x8A11,
    // others
};

enum class BufferUsage : unsigned int {
    StaticDraw = 0x88E4,
    DynamicDraw = 0x88E8,
    StreamDraw = 0x88E0
};

enum class ShaderType : unsigned int {
    VertexShader = 0x8B31,
    FragmentShader = 0x8B30,
    // others
};

enum class ProgramProperty : unsigned int {
    LinkStatus = 0x8B82,
    InfoLogLength = 0x8B84
};

enum class ShaderProperty : unsigned int {
    CompileStatus = 0x8B81,
    InfoLogLength = 0x8B84
};

template <typename T>
inline T default_return_impl() { return T{}; }

template <>
inline void default_return_impl<void>() {}

#define GL_CALL_CUSTOM(name, rettype, proctype, cast_args_decl, cast_args_pass, gl_name) \
static inline rettype name cast_args_decl noexcept {                                     \
    assert(::la::gl::loader && "gl::" #name "failed: context not initialized");          \
    using ProcType = proctype;                                                           \
    static ProcType proc = nullptr;                                                      \
    if (!proc) {                                                                         \
        proc = reinterpret_cast<ProcType>(::la::gl::loader(gl_name));                    \
        assert(proc && "gl::" #name " failed: proc == NULL");                            \
        if (!proc) {                                                                     \
            return default_return_impl<rettype>();                                       \
        }                                                                                \
    }                                                                                    \
    return proc cast_args_pass;                                                          \
}


GL_CALL_CUSTOM(
    /*           name */ cull_face,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (Face face),
    /* cast_args_pass */ (static_cast<unsigned int>(face)),
    "glCullFace"
)

GL_CALL_CUSTOM(
    /*           name */ polygon_mode,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int),
    /* cast_args_decl */ (Face face, PolygonMode mode),
    /* cast_args_pass */ (static_cast<unsigned int>(face), static_cast<unsigned int>(mode)),
    "glPolygonMode"
)

GL_CALL_CUSTOM(
    /*           name */ tex_parameterf,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int, float),
    /* cast_args_decl */ (TextureTarget target, TextureParam pname, float param),
    /* cast_args_pass */ (static_cast<unsigned int>(target), static_cast<unsigned int>(pname), param),
    "glTexParameterf"
)

GL_CALL_CUSTOM(
    /*           name */ tex_paramenteri,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int, int),
    /* cast_args_decl */ (TextureTarget target, TextureParam pname, int param),
    /* cast_args_pass */ (static_cast<unsigned int>(target), static_cast<unsigned int>(pname), param),
    "glTexParameteri"
)

GL_CALL_CUSTOM(
    /*           name */ tex_image_2d,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*),
    /* cast_args_decl */ (TextureTarget target, int level, int internalformat, int width, int height, int border, TextureFormat format, DataType type, const void* pixels),
    /* cast_args_pass */ (
        static_cast<unsigned int>(target),
        level,
        internalformat,
        width,
        height,
        border,
        static_cast<unsigned int>(format),
        static_cast<unsigned int>(type),
        pixels
        ),
    "glTexImage2D"
)

GL_CALL_CUSTOM(
    /*           name */ clear,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int mask),
    /* cast_args_pass */ (mask),
    "glClear"
)

GL_CALL_CUSTOM(
    /*           name */ clear_color,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(float, float, float, float),
    /* cast_args_decl */ (float r, float g, float b, float a),
    /* cast_args_pass */ (r, g, b, a),
    "glClearColor"
)

GL_CALL_CUSTOM(
    /*           name */ disable,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (Capability cap),
    /* cast_args_pass */ (static_cast<unsigned int>(cap)),
    "glDisable"
)


// ----------------------------------------------------------------------------
// Core state
// ----------------------------------------------------------------------------

GL_CALL_CUSTOM(
    /*           name */ enable,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (Capability cap),
    /* cast_args_pass */ (static_cast<unsigned int>(cap)),
    "glEnable"
)

GL_CALL_CUSTOM(
    /*           name */ blend_func,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int),
    /* cast_args_decl */ (BlendFactor sfactor, BlendFactor dfactor),
    /* cast_args_pass */ (static_cast<unsigned int>(sfactor), static_cast<unsigned int>(dfactor)),
    "glBlendFunc"
)

GL_CALL_CUSTOM(
    /*           name */ get_floatv,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, float*),
    /* cast_args_decl */ (GetParam pname, float* data),
    /* cast_args_pass */ (static_cast<unsigned int>(pname), data),
    "glGetFloatv"
)

GL_CALL_CUSTOM(
    /*           name */ get_string,
    /*        rettype */ const unsigned char*,
    /*       proctype */ const unsigned char* (LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int name),
    /* cast_args_pass */ (name),
    "glGetString"
)

GL_CALL_CUSTOM(
    /*           name */ viewport,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, int, int, int),
    /* cast_args_decl */ (int x, int y, int width, int height),
    /* cast_args_pass */ (x, y, width, height),
    "glViewport"
)


// ----------------------------------------------------------------------------
// GL_VERSION_1_1
// ----------------------------------------------------------------------------

GL_CALL_CUSTOM(
    /*           name */ draw_arrays,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, int, int),
    /* cast_args_decl */ (PrimitiveMode mode, int first, int count),
    /* cast_args_pass */ (static_cast<unsigned int>(mode), first, count),
    "glDrawArrays"
)

GL_CALL_CUSTOM(
    /*           name */ draw_elements,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, int, unsigned int, const void*),
    /* cast_args_decl */ (PrimitiveMode mode, int count, DrawElementsType type, const void* indices),
    /* cast_args_pass */ (
        static_cast<unsigned int>(mode),
        count,
        static_cast<unsigned int>(type),
        indices
        ),
    "glDrawElements"
)

GL_CALL_CUSTOM(
    /*           name */ bind_texture,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int),
    /* cast_args_decl */ (TextureTarget target, unsigned int texture),
    /* cast_args_pass */ (static_cast<unsigned int>(target), texture),
    "glBindTexture"
)

GL_CALL_CUSTOM(
    /*           name */ delete_textures,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, const unsigned int*),
    /* cast_args_decl */ (int n, const unsigned int* textures),
    /* cast_args_pass */ (n, textures),
    "glDeleteTextures"
)

GL_CALL_CUSTOM(
    /*           name */ gen_textures,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, unsigned int*),
    /* cast_args_decl */ (int n, unsigned int* textures),
    /* cast_args_pass */ (n, textures),
    "glGenTextures"
)


// ----------------------------------------------------------------------------
// GL_VERSION_1_3
// ----------------------------------------------------------------------------

GL_CALL_CUSTOM(
    /*           name */ active_texture,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (TextureTarget texture),
    /* cast_args_pass */ (static_cast<unsigned int>(texture)),
    "glActiveTexture"
)


// ----------------------------------------------------------------------------
// GL_VERSION_1_5
// ----------------------------------------------------------------------------

GL_CALL_CUSTOM(
    /*           name */ bind_buffer,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int),
    /* cast_args_decl */ (BufferTarget target, unsigned int buffer),
    /* cast_args_pass */ (static_cast<unsigned int>(target), buffer),
    "glBindBuffer"
)

GL_CALL_CUSTOM(
    /*           name */ delete_buffers,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, const unsigned int*),
    /* cast_args_decl */ (int n, const unsigned int* buffers),
    /* cast_args_pass */ (n, buffers),
    "glDeleteBuffers"
)

GL_CALL_CUSTOM(
    /*           name */ gen_buffers,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, unsigned int*),
    /* cast_args_decl */ (int n, unsigned int* buffers),
    /* cast_args_pass */ (n, buffers),
    "glGenBuffers"
)

GL_CALL_CUSTOM(
    /*           name */ buffer_data,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, intptr_t, const void*, unsigned int),
    /* cast_args_decl */ (BufferTarget target, intptr_t size, const void* data, BufferUsage usage),
    /* cast_args_pass */ (
        static_cast<unsigned int>(target),
        size,
        data,
        static_cast<unsigned int>(usage)
        ),
    "glBufferData"
)

GL_CALL_CUSTOM(
    /*           name */ buffer_sub_data,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, intptr_t, intptr_t, const void*),
    /* cast_args_decl */ (BufferTarget target, intptr_t offset, intptr_t size, const void* data),
    /* cast_args_pass */ (
        static_cast<unsigned int>(target),
        offset,
        size,
        data
        ),
    "glBufferSubData"
)


// ----------------------------------------------------------------------------
// GL_VERSION_2_0
// ----------------------------------------------------------------------------

GL_CALL_CUSTOM(
    /*           name */ attach_shader,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int),
    /* cast_args_decl */ (unsigned int program, unsigned int shader),
    /* cast_args_pass */ (program, shader),
    "glAttachShader"
)

GL_CALL_CUSTOM(
    /*           name */ compile_shader,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int shader),
    /* cast_args_pass */ (shader),
    "glCompileShader"
)

GL_CALL_CUSTOM(
    /*           name */ create_program,
    /*        rettype */ unsigned int,
    /*       proctype */ unsigned int(LA_GL_APIENTRY_P)(void),
    /* cast_args_decl */ (),
    /* cast_args_pass */ (),
    "glCreateProgram"
)

GL_CALL_CUSTOM(
    /*           name */ create_shader,
    /*        rettype */ unsigned int,
    /*       proctype */ unsigned int(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (ShaderType type),
    /* cast_args_pass */ (static_cast<unsigned int>(type)),
    "glCreateShader"
)

GL_CALL_CUSTOM(
    /*           name */ delete_program,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int program),
    /* cast_args_pass */ (program),
    "glDeleteProgram"
)

GL_CALL_CUSTOM(
    /*           name */ delete_shader,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int shader),
    /* cast_args_pass */ (shader),
    "glDeleteShader"
)

GL_CALL_CUSTOM(
    /*           name */ enable_vertex_attrib_array,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int index),
    /* cast_args_pass */ (index),
    "glEnableVertexAttribArray"
)

GL_CALL_CUSTOM(
    /*           name */ get_program_iv,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int, int*),
    /* cast_args_decl */ (unsigned int program, ProgramProperty pname, int* params),
    /* cast_args_pass */ (program, static_cast<unsigned int>(pname), params),
    "glGetProgramiv"
)

GL_CALL_CUSTOM(
    /*           name */ get_program_info_log,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, int, int*, char*),
    /* cast_args_decl */ (unsigned int program, int bufSize, int* length, char* infoLog),
    /* cast_args_pass */ (program, bufSize, length, infoLog),
    "glGetProgramInfoLog"
)

GL_CALL_CUSTOM(
    /*           name */ get_shader_iv,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int, int*),
    /* cast_args_decl */ (unsigned int shader, ShaderProperty pname, int* params),
    /* cast_args_pass */ (shader, static_cast<unsigned int>(pname), params),
    "glGetShaderiv"
)

GL_CALL_CUSTOM(
    /*           name */ get_shader_info_log,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, int, int*, char*),
    /* cast_args_decl */ (unsigned int shader, int bufSize, int* length, char* infoLog),
    /* cast_args_pass */ (shader, bufSize, length, infoLog),
    "glGetShaderInfoLog"
)

GL_CALL_CUSTOM(
    /*           name */ get_uniform_location,
    /*        rettype */ int,
    /*       proctype */ int(LA_GL_APIENTRY_P)(unsigned int, const char*),
    /* cast_args_decl */ (unsigned int program, const char* name),
    /* cast_args_pass */ (program, name),
    "glGetUniformLocation"
)

GL_CALL_CUSTOM(
    /*           name */ link_program,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int program),
    /* cast_args_pass */ (program),
    "glLinkProgram"
)

GL_CALL_CUSTOM(
    /*           name */ shader_source,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, int, const char* const*, const int*),
    /* cast_args_decl */ (unsigned int shader, int count, const char* const* strings, const int* lengths),
    /* cast_args_pass */ (shader, count, strings, lengths),
    "glShaderSource"
)

GL_CALL_CUSTOM(
    /*           name */ use_program,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int program),
    /* cast_args_pass */ (program),
    "glUseProgram"
)

GL_CALL_CUSTOM(
    /*           name */ uniform1i,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, int),
    /* cast_args_decl */ (int location, int v0),
    /* cast_args_pass */ (location, v0),
    "glUniform1i"
)

GL_CALL_CUSTOM(
    /*           name */ uniform_matrix4fv,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, int, unsigned char, const float*),
    /* cast_args_decl */ (int location, int count, bool transpose, const float* value),
    /* cast_args_pass */ (location, count, Boolean{ transpose }, value),
    "glUniformMatrix4fv"
)

GL_CALL_CUSTOM(
    /*           name */ vertex_attrib_pointer,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, int, unsigned int, unsigned char, int, const void*),
    /* cast_args_decl */ (unsigned int index, int size, DrawElementsType type, bool normalized, int stride, const void* pointer),
    /* cast_args_pass */ (index, size, static_cast<unsigned int>(type), Boolean{ normalized }, stride, pointer),
    "glVertexAttribPointer"
)


// ----------------------------------------------------------------------------
// GL_VERSION_3_0
// ----------------------------------------------------------------------------

GL_CALL_CUSTOM(
    /*           name */ bind_buffer_base,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, unsigned int, unsigned int),
    /* cast_args_decl */ (BufferTarget target, unsigned int index, unsigned int buffer),
    /* cast_args_pass */ (static_cast<unsigned int>(target), index, buffer),
    "glBindBufferBase"
)

GL_CALL_CUSTOM(
    /*           name */ vertex_attrib_i_pointer,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int, int, unsigned int, int, const void*),
    /* cast_args_decl */ (unsigned int index, int size, DrawElementsType type, int stride, const void* pointer),
    /* cast_args_pass */ (index, size, static_cast<unsigned int>(type), stride, pointer),
    "glVertexAttribIPointer"
)

GL_CALL_CUSTOM(
    /*           name */ bind_vertex_array,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(unsigned int),
    /* cast_args_decl */ (unsigned int array),
    /* cast_args_pass */ (array),
    "glBindVertexArray"
)

GL_CALL_CUSTOM(
    /*           name */ delete_vertex_arrays,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, const unsigned int*),
    /* cast_args_decl */ (int n, const unsigned int* arrays),
    /* cast_args_pass */ (n, arrays),
    "glDeleteVertexArrays"
)

GL_CALL_CUSTOM(
    /*           name */ gen_vertex_arrays,
    /*        rettype */ void,
    /*       proctype */ void(LA_GL_APIENTRY_P)(int, unsigned int*),
    /* cast_args_decl */ (int n, unsigned int* arrays),
    /* cast_args_pass */ (n, arrays),
    "glGenVertexArrays"
)
} // namespace gl
} // namespace la

#endif // __LA_GL_HEADER_GUARD