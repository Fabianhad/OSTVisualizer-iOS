$input v_worldNormal, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_meshColor;
uniform vec4 u_lightParams;  // x=ambient, y=key, z=fill, w=rim
uniform vec4 u_eyePos;

void main()
{
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(u_eyePos.xyz - v_worldPos);
    if (dot(N, V) < 0.0) N = -N;

    vec3 keyDir  = normalize(vec3( 0.577,  0.577,  0.577));
    vec3 fillDir = normalize(vec3(-0.577,  0.577, -0.577));
    vec3 rimDir  = normalize(vec3( 0.0,   -0.707, -0.707));

    float lighting = u_lightParams.x
        + max(0.0, dot(N, keyDir))  * u_lightParams.y
        + max(0.0, dot(N, fillDir)) * u_lightParams.z
        + max(0.0, dot(N, rimDir))  * u_lightParams.w;

    gl_FragColor = vec4(u_meshColor.rgb * lighting, u_meshColor.a);
}
