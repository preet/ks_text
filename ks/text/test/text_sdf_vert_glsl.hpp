std::string const text_sdf_vert_glsl = R"___DELIM___(

// VERTEX SHADER

// notes:
// to maintain compatibility, the version
// preprocessor call needs to be added to the
// beginning of this file by the (cpu) compiler:
//
// "#version 100" for OpenGL ES 2 and
// "#version 120" (or higher) for desktop OpenGL

#ifdef GL_ES
    // vertex shader defaults for types are:
    // precision highp float;
    // precision highp int;
    // precision lowp sampler2D;
    // precision lowp samplerCube;
#else
    // with default (non ES) OpenGL shaders, precision
    // qualifiers aren't used -- we explicitly set them
    // to be defined as 'nothing' so they are ignored
    #define lowp
    #define mediump
    #define highp
#endif

// attributes
attribute vec4 a_v4_position;
attribute vec2 a_v2_tex0;
attribute vec4 a_v4_color;

// varyings
// * lowp is okay for textures
//   up to 128x128
// * mediump is good for textures
//   from 128x128 to ~1024x1024
varying mediump vec2 v_v2_tex0;
varying lowp vec4 v_v4_color;

void main()
{
    v_v2_tex0 = a_v2_tex0;
    v_v4_color = a_v4_color;
    gl_Position = a_v4_position;
}
 
)___DELIM___";


