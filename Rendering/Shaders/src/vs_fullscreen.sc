$input a_position, a_texcoord0
$output v_worldPos

#include <bgfx_shader.sh>

void main()
{
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    v_worldPos = vec3(a_texcoord0, 0.0);
}
