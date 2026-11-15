#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_GOOGLE_cpp_style_line_directive : require

layout(location = 0) in vec2 inFragCoord;
layout(location = 0) out vec4 outFragColor;

layout(std140, set = 0, binding = 0) uniform ShaderToyUniforms
{
    vec3 iResolution;
    float iTime;
    float iTimeDelta;
    int iFrame;
    float iFrameRate;
    float iSampleRate;
    vec4 iMouse;
    vec4 iDate;
    float iChannelTime[4];
    vec3 iChannelResolution[4];
};
layout(set = 0, binding = 1) uniform sampler2D iChannel0;
layout(set = 0, binding = 2) uniform sampler2D iChannel1;
layout(set = 0, binding = 3) uniform sampler2D iChannel2;
layout(set = 0, binding = 4) uniform sampler2D iChannel3;
const int ColorEncodingLinear = 0;
const int ColorEncodingSrgb = 1;
layout(constant_id = 0) const int ColorAttachmentEncoding = ColorEncodingLinear;

vec4 GetFragCoord()
{
    return vec4(inFragCoord * iResolution.xy, gl_FragCoord.zw);
}
#define gl_FragCoord GetFragCoord()
#if !RENDER_TO_TEXTURE
#define dFdy(p) (-dFdy(p))
#define dFdyFine(p) (-dFdyFine(p))
#define dFdyCoarse(p) (-dFdyCoarse(p))
#endif

#define texture2D texture
#define textureCube texture

// Shadertoy Image shaders already encode display color; undo an sRGB attachment's encoding.
vec3 SrgbToLinear(vec3 color)
{
    return mix(color / 12.92, pow((max(color, 0.0) + 0.055) / 1.055, vec3(2.4)),
               greaterThan(color, vec3(0.04045)));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord);

void main()
{
    mainImage(outFragColor, inFragCoord.xy * iResolution.xy);
    if (ColorAttachmentEncoding == ColorEncodingSrgb)
    {
        outFragColor.rgb = SrgbToLinear(outFragColor.rgb);
    }
#if !RENDER_TO_TEXTURE
    outFragColor.a = 1.0;
#endif
}
