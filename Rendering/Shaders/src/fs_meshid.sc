$input v_worldNormal, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_meshId;

void main()
{
    gl_FragColor = u_meshId;
}
