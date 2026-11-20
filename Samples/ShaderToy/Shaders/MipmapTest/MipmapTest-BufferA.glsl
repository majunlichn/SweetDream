float checker(vec2 p, float size)
{
    vec2 cell = floor(vec2(abs(p.x), p.y) / size);
    // Mirroring around the center makes the average of each checker zero.
    return sign(p.x) * (2.0 * mod(cell.x + cell.y, 2.0) - 1.0);
}

void mainImage(out vec4 color, in vec2 fragCoord)
{
    vec2 texelCenter = floor(fragCoord) + 0.5;
    vec2 size = iResolution.xy;
    vec2 uv = texelCenter / size;
    vec2 p = vec2(texelCenter.x - 0.5 * size.x, texelCenter.y);
    float fine = checker(p, 2.0);
    float coarse = checker(p, 12.0);
    color = vec4(0.5 + 0.55 * (uv.x - 0.5) + 0.18 * fine,
                 0.5 + 0.55 * (uv.y - 0.5) + 0.18 * coarse,
                 0.5 + 0.30 * fine + 0.16 * coarse,
                 0.25 * float(1 + iFrame % 3));
}
