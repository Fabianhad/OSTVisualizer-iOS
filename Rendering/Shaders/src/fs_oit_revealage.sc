$input v_worldNormal, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_meshColor;

void main()
{
    gl_FragColor = vec4(1.0 - u_meshColor.a, 0.0, 0.0, 0.0);
}
