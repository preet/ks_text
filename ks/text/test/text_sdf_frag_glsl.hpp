std::string const text_sdf_frag_glsl = R"___DELIM___(

// FRAGMENT SHADER

// notes:
// to maintain compatibility, the version
// preprocessor call needs to be added to the
// beginning of this file by the (cpu) compiler:
//
// "#version 100" for OpenGL ES 2 and
// "#version 120" (or higher) for desktop OpenGL

#ifdef GL_ES
    // the fragment shader in ES 2 doesn't have a
    // default precision qualifier for floats so
    // it needs to be explicitly specified
    precision mediump float;

    // note: highp may not be available for float types in
    // the fragment shader -- use the following to set it:
    // #ifdef GL_FRAGMENT_PRECISION_HIGH
    // precision highp float;
    // #else
    // precision mediump float;
    // #endif

    // fragment shader defaults for other types are:
    // precision mediump int;
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

// varyings
varying mediump vec2 v_v2_tex0;
varying lowp vec4 v_v4_color;

// uniforms
// * lowp because lower precision
//   for color data is acceptable
uniform lowp sampler2D u_s_tex0;

void main(void)
{
    // distf
    // The distance field value for this fragment:
    // (distf == 0.5): on the shape's edge
    // (distf < 0.5): moving away from the edge outwards
    // (distf > 0.5): moving away from the edge inwards
    float distf = texture2D(u_s_tex0, v_v2_tex0).r;

    float glyph_center = 0.5;
    float glyph_fuzz = 0.02;
    vec4 color = v_v4_color;

    // NOTE glyph_fuzz should be scaled wrt how
    // many pixels the font takes up

    float alpha = smoothstep(glyph_center-glyph_fuzz,
                             glyph_center+glyph_fuzz,
                             distf);

    //color.a = min(color.a,alpha);
    color.a *= alpha;   // I think this looks nicer than min(...)
                        // but either should be fine

    gl_FragColor = color;

//    vec4 color = v_v4_color*texture2D(u_s_tex0,v_v2_tex0).r;
//    color.a = 1.0;
//    gl_FragColor = color;
}
 
)___DELIM___";
