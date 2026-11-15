// User-supplied source: https://www.shadertoy.com/view/MdGXWG
/*

    Voxel Flythrough
    ----------------

    The cliche voxel flythrough - Everyone should do at least one. :) I've been meaning to put one
    of these up for ages, ever since admiring Reinder's Minecraft Blocks port. Other inspiration
    came from IQ's Voxel Edges and just about every flythrough voxel scene in every demo. :)

    I had originally intended to make it look like some kind of alien space port, but then realized
    that would require actual work, so went with the abstract, naive version. :) I also wanted the
    to keep the code readable enough for anyone who might want to try one of these themselves.

    The voxel setup is reasonably straight forward, and so is the distance function, which is
    described below.

    Mainly based on the following:

    Voxel Ambient Occlusion - fb39ca4
    https://www.shadertoy.com/view/ldl3DS

    Minecraft Blocks - Reinder
    https://www.shadertoy.com/view/MdlGz4
    Based on: http://jsfiddle.net/uzMPU/ - Markus Persson

    Voxel Edges - IQ
    https://www.shadertoy.com/view/4dfGzs

    Voxel Corridor - Shane
    https://www.shadertoy.com/view/MdVSDh

*/

#define FAR 60.

// 2x2 matrix rotation. Note the absence of "cos." It's there, but in disguise, and comes courtesy
// of Fabrice Neyret's "ouside the box" thinking. :)
mat2 rot2( float a ){ vec2 v = sin(vec2(1.570796, 0) + a); return mat2(v, -v.y, v.x); }

// Tri-Planar blending function. Based on an old Nvidia tutorial by Ryan Geiss.
vec3 tex3D( sampler2D tex, in vec3 p, in vec3 n ){

    n = max((abs(n) - 0.2)*7., 0.001); // n = max(abs(n), 0.001);// etc.
    n /= (n.x + n.y + n.z );
    p = (texture(tex, p.yz)*n.x + texture(tex, p.zx)*n.y + texture(tex, p.xy)*n.z).xyz;
    // Rough sRGB to RGB conversion to account for the gamma correction we're doing before presenting
    // the final value to the screen... or something like that.... but filtering is done in sRGB...
    // I think? Sigh! Why does it always have to be so complicated? :)
    return p*p;
}

// The path is a 2D sinusoid that varies over time, depending upon the frequencies, and amplitudes.
vec2 path(in float z){

    //return vec2(0); // Straight.
    float a = sin(z * 0.11);
    float b = cos(z * 0.14);
    return vec2(a*4. -b*1.5, b*1.7 + a*1.5);
    //return vec2(a*4. -b*1.5, 0.); // Just X.
    //return vec2(0, b*1.7 + a*1.5); // Just Y.
}

// Compact, self-contained version of IQ's 3D value noise function.
float n3D(vec3 p){

    const vec3 s = vec3(7, 157, 113);
    vec3 ip = floor(p); p -= ip;
    vec4 h = vec4(0., s.yz, s.y + s.z) + dot(ip, s);
    p = p*p*(3. - 2.*p); //p *= p*p*(p*(p * 6. - 15.) + 10.);
    h = mix(fract(sin(h)*43758.5453), fract(sin(h + s.x)*43758.5453), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z); // Range: [0, 1].
}

// Random cloud-shaped structures. Since the distance field is voxelized, you don't need real clouds, just
// something that approximates it, so some pseudo-random 3D sinusoidal shapes will do. The sinudoidal
// function has been arranged in such a way as to provide an open area for the camera to fly through.
// In turn, the open area is wrapped around a path. By the way, if you were doing this with something like
// 3d value noise, you could could carve a path through it with a cylinder and no one would be the wiser.
float map(vec3 p){

    p.xy -= path(p.z); // Perturb an object around a path.

    p = cos(p*.1575 + sin(p.zxy*.4375)); // 3D sinusoidal mutation.

    // Spherize. The result is some mutated, spherical blob-like shapes.
    float n = dot(p, p);

    p = sin(p*3. + cos(p.yzx*3.)); // Finer bumps. Subtle, and almost not necessary with voxelization.

    return n - p.x*p.y*p.z*.35 - .9; // Combine, and we're done.

}

// The brick groove pattern. Thrown together too quickly.
// Needs some tidy up, but it's quick enough for now.
//
const float w2h = 1.; // Width to height ratio.
const float mortW = .05; // Morter width.

float brickShade(vec2 p){

    p = fract(p);
    return pow(16.*p.x*p.y*(1.-p.x)*(1.-p.y), 0.25);

}

float brickMorter(vec2 p){

    p.x -= .5;

    p = abs(fract(p + vec2(0, .5)) - .5)*2.;

    // Smooth grooves. Better for bump mapping.
    return smoothstep(0., mortW, p.x)*smoothstep(0., mortW*w2h, p.y);

}

float brick(vec2 p){

    p = fract(p*vec2(0.5/w2h, 0.5))*2.;

    return brickMorter(p)*(brickShade(p)*.5 + .5);
}

// Surface bump function. Cheap, but with decent visual impact.
float bumpSurf( in vec3 p, in vec3 n){

    n = abs(n);

    if (n.x>0.5) p.xy = p.zy;
    else if (n.y>0.5) p.xy = p.zx;

    return brick(p.xy);

}

// Standard function-based bump mapping function.
vec3 doBumpMap(in vec3 p, in vec3 nor, float bumpfactor){

    const vec2 e = vec2(0.001, 0);
    float ref = bumpSurf(p, nor);
    vec3 grad = (vec3(bumpSurf(p - e.xyy, nor),
                      bumpSurf(p - e.yxy, nor),
                      bumpSurf(p - e.yyx, nor) )-ref)/e.x;

    grad -= nor*dot(nor, grad);

    return normalize( nor + grad*bumpfactor );

}

// Texture bump mapping. Four tri-planar lookups, or 12 texture lookups in total. I tried to
// make it as concise as possible. Whether that translates to speed, or not, I couldn't say.
vec3 doBumpMap( sampler2D tx, in vec3 p, in vec3 n, float bf){

    const vec2 e = vec2(0.001, 0);

    // Three gradient vectors rolled into a matrix, constructed with offset greyscale texture values.
    mat3 m = mat3( tex3D(tx, p - e.xyy, n), tex3D(tx, p - e.yxy, n), tex3D(tx, p - e.yyx, n));

    vec3 g = vec3(0.299, 0.587, 0.114)*m; // Converting to greyscale.
    g = (g - dot(tex3D(tx, p, n), vec3(0.299, 0.587, 0.114)) )/e.x; g -= n*dot(n, g);

    return normalize( n + g*bf ); // Bumped normal. "bf" - bump factor.

}

// This is just a slightly modified version of fb39ca4's code, with some
// elements from IQ and Reinder's examples. They all work the same way:
// Obtain the current voxel, then test the distance field for a hit. If
// the ray has moved into the voxelized isosurface, break. Otherwise, move
// to the next voxel. That involves a bit of decision making - due to the
// nature of voxel boundaries - and the "mask," "side," etc, variable are
// an evolution of that. If you're not familiar with the process, it's
// pretty straight forward, and there are a lot of examples on Shadertoy,
// plus a lot more articles online.
//
vec3 voxelTrace(vec3 ro, vec3 rd, out vec3 mask){

    vec3 p = floor(ro) + .5;

    vec3 dRd = 1./abs(rd); // 1./max(abs(rd), vec3(.0001));
    rd = sign(rd);
    vec3 side = dRd*(rd * (p - ro) + 0.5);

    mask = vec3(0);

    for (int i = 0; i < 80; i++) {

        if (map(p)<0.) break;

        // Note that I've put in the messy reverse step to accomodate
        // the "less than or equals" logic, rather than just the "less than."
        // Without it, annoying seam lines can appear... Feel free to correct
        // me on that, if my logic isn't up to par. It often isn't. :)
        mask = step(side, side.yzx)*(1.-step(side.zxy, side));
        side += mask*dRd;
        p += mask * rd;
    }

    return p;
}

// Voxel shadows. They kind of work like regular hard-edged shadows. They
// didn't present too many problems, but it was still nice to have Reinder's
// Minecraft shadow example as a reference. Fantastic example, if you've
// never seen it:
//
// Minecraft - Reinder
// https://www.shadertoy.com/view/4ds3WS
//
float voxShadow(vec3 ro, vec3 rd, float end){

    float shade = 1.0;
    vec3 p = floor(ro) + .5;

    vec3 dRd = 1./abs(rd);//1./max(abs(rd), vec3(.0001));
    rd = sign(rd);
    vec3 side = dRd*(rd * (p - ro) + 0.5);

    vec3 mask = vec3(0);

    float d = 1.;

    for (int i = 0; i < 16; i++) {

        d = map(p);

        if (d<0. || length(p-ro)>end) break;

        mask = step(side, side.yzx)*(1.-step(side.zxy, side));
        side += mask*dRd;
        p += mask * rd;
    }

    // Shadow value. If in shadow, return a dark value.
    return shade = step(0., d)*.7 + .3;

}

///////////
//
// This is a trimmed down version of fb39ca4's voxel ambient occlusion code with some
// minor tweaks and adjustments here and there. The idea behind voxelized AO is simple.
// The execution, not so much. :) So damn fiddly. Thankfully, fb39ca4, IQ, and a few
// others have done all the hard work, so it's just a case of convincing yourself that
// it works and using it.
//
// Refer to: Voxel Ambient Occlusion - fb39ca4
// https://www.shadertoy.com/view/ldl3DS
//
vec4 voxelAO(vec3 p, vec3 d1, vec3 d2) {

    // Take the four side and corner readings... at the correct positions...
    // That's the annoying bit that I'm glad others have worked out. :)
    vec4 side = vec4(map(p + d1), map(p + d2), map(p - d1), map(p - d2));
    vec4 corner = vec4(map(p + d1 + d2), map(p - d1 + d2), map(p - d1 - d2), map(p + d1 - d2));

    // Quantize them. It's either occluded, or it's not, so to speak.
    side = step(side, vec4(0));
    corner = step(corner, vec4(0));

    // Use the side and corner values to produce a more honed in value... kind of.
    return 1. - (side + side.yzwx + max(corner, side*side.yzwx))/3.;

}

float calcVoxAO(vec3 vp, vec3 sp, vec3 rd, vec3 mask) {

    // Obtain four AO values at the appropriate quantized positions.
    vec4 vAO = voxelAO(vp - sign(rd)*mask, mask.zxy, mask.yzx);

    // Use the fractional voxel postion and and the proximate AO values
    // to return the interpolated AO value for the surface position.
    sp = fract(sp);
    vec2 uv = sp.yz*mask.x + sp.zx*mask.y + sp.xy*mask.z;
    return mix(mix(vAO.z, vAO.w, uv.x), mix(vAO.y, vAO.x, uv.x), uv.y);

}
///////////

// XT95's really clever, cheap, SSS function. The way I've used it doesn't do it justice,
// so if you'd like to really see it in action, have a look at the following:
//
// Alien Cocoons - XT95: https://www.shadertoy.com/view/MsdGz2
//
float thickness( in vec3 p, in vec3 n, float maxDist, float falloff )
{
    const float nbIte = 6.0;
    float ao = 0.0;

    for( float i=1.; i< nbIte+.5; i++ ){

        float l = (i*.75 + fract(cos(i)*45758.5453)*.25)/nbIte*maxDist;

        ao += (l + map( p -n*l )) / pow(1. + l, falloff);
    }

    return clamp( 1.-ao/nbIte, 0., 1.);
}

// Simple environment mapping. Pass the reflected vector in and create some
// colored noise with it. The normal is redundant here, but it can be used
// to pass into a 3D texture mapping function to produce some interesting
// environmental reflections.
vec3 envMap(vec3 rd, vec3 sn){

    float c = n3D(rd*3.)*.66 + n3D(rd*6.)*.34;
    c = smoothstep(0.4, 1., c);
    return vec3(min(c*1.5, 1.), pow(c, 3.), pow(c, 16.));
    //vec3 col = tex3D(iChannel1, rd/4., sn);
    //return smoothstep(.0, 1., col*col*2.);//col*col*2.;

}

void mainImage( out vec4 fragColor, in vec2 fragCoord ){

    // Screen coordinates.
    vec2 uv = (fragCoord - iResolution.xy*.5)/iResolution.y;

    // Camera Setup.
    vec3 camPos = vec3(0, 0, iTime*8.); // Camera position, doubling as the ray origin.
    vec3 lookAt = camPos + vec3(0, 0, .25);  // "Look At" position.

    // Light positioning.
    vec3 lightPos = camPos + vec3(0, 1, 5);// Put it a bit in front of the camera.

    // Using the Z-value to perturb the XY-plane.
    // Sending the camera, "look at," and light vector along the path. The "path" function is
    // synchronized with the distance function.
    lookAt.xy += path(lookAt.z);
    camPos.xy += path(camPos.z);
    lightPos.xy += path(lightPos.z);

    // Using the above to produce the unit ray-direction vector.
    float FOV = 3.14159265/3.; // FOV - Field of view.
    vec3 forward = normalize(lookAt-camPos);
    vec3 right = normalize(vec3(forward.z, 0, -forward.x ));
    vec3 up = cross(forward, right);

    // rd - Ray direction.
    vec3 rd = normalize(forward + FOV*uv.x*right + FOV*uv.y*up);

    // Distorted version.
    //vec3 rd = normalize(forward + FOV*uv.x*right + FOV*uv.y*up);
    //rd = normalize(vec3(rd.xy, rd.z - dot(rd.xy, rd.xy)*.25));

    // Swiveling the camera about the XY-plane (from left to right) when turning corners.
    // Naturally, it's synchronized with the path in some kind of way.
    rd.xy = rot2( path(lookAt.z).x/24. )*rd.xy;

    // Raymarch the voxel grid.
    vec3 mask;
    vec3 vPos = voxelTrace(camPos, rd, mask);

    // Using the voxel position to determine the distance from the camera to the hit point.
    // I'm assuming IQ is responsible for this clean piece of logic.
    vec3 tCube = (vPos-camPos - .5*sign(rd))/rd;
    float t = max(max(tCube.x, tCube.y), tCube.z);

    // Initialize the scene color.
    vec3 sceneCol = vec3(0);

    // The ray has effectively hit the surface, so light it up.
    if(t<FAR){

        // Surface position and surface normal.
        vec3 sp = camPos + rd*t;

        // Voxel normal.
        vec3 sn = -(mask * sign( rd ));

        // Sometimes, it's necessary to save a copy of the unbumped normal.
        vec3 snNoBump = sn;

        // I try to avoid it, but it's possible to do a texture bump and a function-based
        // bump in succession. It's also possible to roll them into one, but I wanted
        // the separation... Can't remember why, but it's more readable anyway.
        //
        // Texture scale factor.
        const float tSize0 = 1./4.;
        // Texture-based bump mapping.
        sn = doBumpMap(iChannel0, sp*tSize0, sn, 0.01);

        // Function based bump mapping. Comment it out to see the under layer. It's pretty
        // comparable to regular beveled Voronoi... Close enough, anyway.
        sn = doBumpMap(sp, sn, .1);

        // Ambient occlusion.
        float ao = calcVoxAO(vPos, sp, rd, mask);

        // Light direction vectors.
        vec3 ld = lightPos - sp;

        // Distance from respective lights to the surface point.
        float lDist = max(length(ld), 0.001);

        // Normalize the light direction vectors.
        ld /= lDist;

        // Light attenuation, based on the distances above.
        float atten = 1./(1. + lDist*.2 + lDist*.1); // + distlpsp*distlpsp*0.025

        // Ambient light.
        float ambience = 0.25;

        // Diffuse lighting.
        float diff = max( dot(sn, ld), 0.0);

        // Specular lighting.
        float spec = pow(max( dot( reflect(-ld, sn), -rd ), 0.0 ), 32.);

        // Object texturing.
        //vec3 texCol = vec3(1, .75, .5);//vec3(1, .05, .15);//vec3(1, .5, .15);//vec3(1, .6, .4) + step(abs(snNoBump.y), .5)*vec3(0,.4, .6);
        vec3 texCol = vec3(.55, .7, 1.3);

        //float rnd = fract(sin(dot(vPos, vec3(7, 157, 113)))*43758.5453);
        //texCol *= rnd*.5+.5;
        //if(rnd>.5) texCol = vec3(1);

        // Multiplying by the texture color.
        texCol *= tex3D(iChannel0, sp*tSize0, sn)*4.;

        texCol *= bumpSurf( sp, sn)*.5 + .5; // Enhance the bump.

        /////////
        // Translucency, courtesy of XT95. See the "thickness" function.
        vec3 hf = normalize(ld + sn);
        float th = thickness( sp, sn, 1., 1. );
        float tdiff = pow( clamp( dot(rd, -hf), 0., 1.), 1.);
        float trans = (tdiff + .0)*th;
        trans = pow(trans, 4.);
        ////////

        // Shadows... I was having all sorts of trouble trying the move the ray off off the
        // block. Thanks to Reinder's "Minecraft" example for showing me the ray needs to
        // be bumped off by the normal, not the unit direction ray. :)
        float shading = voxShadow(sp + snNoBump*.01, ld, lDist);

        // Combining the above terms to produce the final color.
        sceneCol = texCol*(diff + ambience) + vec3(.7, .9, 1.)*spec;// + vec3(.5, .8, 1)*spec2;
        sceneCol += vec3(1, 0.05, .15)*trans*2.;
        sceneCol += envMap(reflect(rd, sn), sn);

        //vec3 rfCol = texture(iChannel2, reflect(rd, sn)).xyz; // Forest scene.
        //sceneCol += rfCol*rfCol*.5;

        // Shading.
        sceneCol *= atten*shading*ao;

        // "fb39ca4" did such a good job with the AO, that it's worth a look on its own. :)
        //sceneCol = vec3(ao);

    }

    // Blend in a bit of fog for atmospheric effect.
    vec3 fog = mix(vec3(.96, .48, .36), vec3(.24, .32, .64), -rd.y*.5 + .5); //.zyx
    //vec3 fog = mix(vec3(.32, .28, .16)*3., vec3(.32, .04, .08)*2., -rd.y*.5 + .5);

    // I'll tidy this up later.
    sceneCol = mix(sceneCol, fog*sqrt(fog)*1.2, smoothstep(0., .95, t/FAR)); // exp(-.002*t*t), etc. fog.zxy

    // Clamp and present the roughly gamma corrected pixel to the screen. :)
    fragColor = vec4(sqrt(clamp(sceneCol, 0., 1.)), 1.0);

}
