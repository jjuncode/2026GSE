#version 330

in vec3 a_Position;

in vec2 a_Origin;
in vec2 a_AxisX;
in vec2 a_AxisY;
in vec4 a_Region;
in vec4 a_Color;

out vec2 v_Uv;
out vec4 v_Color;

void main()
{
	vec2 position = a_Origin + a_AxisX * a_Position.x + a_AxisY * a_Position.y;

	v_Uv = vec2(mix(a_Region.x, a_Region.z, a_Position.x), mix(a_Region.y, a_Region.w, a_Position.y));
	v_Color = a_Color;

	gl_Position = vec4(position, 0.0, 1.0);
}
