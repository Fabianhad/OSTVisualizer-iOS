$input v_worldPos

#include <bgfx_shader.sh>

SAMPLER2D(s_accumTex,   0);
SAMPLER2D(s_revealTex,  1);

void main()
{
    vec2 uv = v_worldPos.xy;
    vec4 accum = texture2D(s_accumTex, uv);
    float revealage = texture2D(s_revealTex, uv).r;

    if (accum.a <= 1e-5)
        discard;

    vec3 avgColor = accum.rgb / max(accum.a, 1e-5);
    float coverage = 1.0 - revealage;

    gl_FragColor = vec4(avgColor * coverage, coverage);
}
