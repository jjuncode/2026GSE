#version 330

in vec3 a_Position;

in vec4 a_Trans;
in vec4 a_Color;

out vec4 v_Color;

void main()
{
	vec2 position = a_Position.xy * a_Trans.zw + a_Trans.xy;

	v_Color = a_Color;

	gl_Position = vec4(position, 0.0, 1.0);
}
