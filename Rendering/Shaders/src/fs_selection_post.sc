$input v_worldPos

#include <bgfx_shader.sh>

SAMPLER2D(s_sceneTex, 0);
SAMPLER2D(s_selMaskTex, 1);

void main()
{
    vec2 uv = v_worldPos.xy;
    vec4 scene = texture2D(s_sceneTex, uv);
    vec2 id = texture2D(s_selMaskTex, uv).rg;

    // Not selected (id == 0)
    if (id.r < 0.001 && id.g < 0.001)
    {
        gl_FragColor = scene;
        return;
    }

    // Interior tint (matches old renderer)
    gl_FragColor = vec4(
        min(1.0, scene.r * 0.75 + 0.25),
        min(1.0, scene.g * 0.75 + 0.20),
        scene.b * 0.55,
        1.0
    );
}
