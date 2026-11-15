#version 460

layout(location = 0) out vec2 outFragCoord;

layout(constant_id = 0) const bool RenderToTexture = false;

void main()
{
    vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outFragCoord = position;
    // Buffer row zero stores bottom-left texels; the screen's row zero is top-left.
    if (!RenderToTexture)
    {
        outFragCoord.y = 1.0 - outFragCoord.y;
    }
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
