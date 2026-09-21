#version 330

in vec3 a_Position;

uniform vec2 u_Origin;
uniform vec2 u_AxisX;
uniform vec2 u_AxisY;
uniform vec4 u_Region;

out vec2 v_Uv;

void main()
{
	vec2 position = u_Origin + u_AxisX * a_Position.x + u_AxisY * a_Position.y;
	v_Uv = vec2(mix(u_Region.x, u_Region.z, a_Position.x), mix(u_Region.y, u_Region.w, a_Position.y));

	gl_Position = vec4(position, 0.0, 1.0);
}
