$input v_worldNormal, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_meshColor;
uniform vec4 u_lightParams;
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

    vec3 color = u_meshColor.rgb * lighting;
    float alpha = u_meshColor.a;

    float z = clamp(gl_FragCoord.z, 0.0, 1.0);
    float d2 = z * 0.2; d2 *= d2;
    float d5 = z * 0.005; d5 = d5 * d5 * d5 * d5 * d5;
    float weight = alpha * clamp(10.0 / (1e-5 + d2 + d5), 0.01, 3000.0);

    gl_FragColor = vec4(color * alpha * weight, alpha * weight);
}
