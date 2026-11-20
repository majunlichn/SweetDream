void mainImage(out vec4 color, in vec2 fragCoord)
{
    ivec2 baseSize = ivec2(iChannelResolution[0].xy);
    int expectedLevels = findMSB(max(1, max(baseSize.x, baseSize.y))) + 1;
    const int maxColumns = 4;
    ivec2 grid = ivec2(min(expectedLevels, maxColumns),
                       (expectedLevels + maxColumns - 1) / maxColumns);
    vec2 tileSize = iResolution.xy / vec2(grid);
    ivec2 tile = ivec2(fragCoord / tileSize);
    int level = (grid.y - 1 - tile.y) * grid.x + tile.x;
    vec2 local = fragCoord - vec2(tile) * tileSize;
    float margin = min(8.0, 0.08 * min(tileSize.x, tileSize.y));
    vec2 panelSize = tileSize - 2.0 * margin;
    vec2 panel = local - margin;
    color = vec4(0.035, 0.04, 0.05, 1.0);
    if (level >= expectedLevels ||
        any(lessThan(panel, vec2(0.0))) || any(greaterThanEqual(panel, panelSize)))
    {
        return;
    }

    ivec2 expectedSize = max(ivec2(1), baseSize >> level);
    int levels = textureQueryLevels(iChannel0);
    bool available = level < levels;
    ivec2 size = available ? textureSize(iChannel0, level) : expectedSize;
    vec2 previewArea = panelSize - 2.0 * margin;
    // Give each level the same space but show its texels without interpolation.
    float scale = min(previewArea.x / float(size.x), previewArea.y / float(size.y));
    vec2 previewSize = scale * vec2(size);
    vec2 preview = panel - 0.5 * (panelSize - previewSize);
    if (available && all(greaterThanEqual(preview, vec2(0.0))) &&
        all(lessThan(preview, previewSize)))
    {
        color.rgb = texelFetch(iChannel0, min(ivec2(preview / scale), size - 1), level).rgb;
        return;
    }

    vec4 expected = vec4(vec3(0.5), 0.25 * float(1 + iFrame % 3));
    bool passed = levels == expectedLevels && available && size == expectedSize &&
                  all(lessThan(abs(textureLod(iChannel0, vec2(0.5), float(level)) - expected),
                               vec4(0.001)));
    if (any(lessThan(min(panel, panelSize - panel), vec2(min(2.0, margin)))))
    {
        color.rgb = passed ? vec3(0.24, 0.77, 0.42) : vec3(0.96, 0.24, 0.24);
    }
    else
    {
        color.rgb = vec3(0.09, 0.10, 0.12);
    }
}
