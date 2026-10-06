#version 330

layout(location=0) out vec4 FragColor;

in vec2 v_Uv;
in vec4 v_Color;

uniform sampler2D u_Texture;
uniform int u_UndoToneMap;

vec3 UndoToneMapACES(vec3 display)
{
	vec3 target = clamp(display, 0.0, 0.92);
	vec3 quad_a = 2.51 - 2.43 * target;
	vec3 quad_b = 0.03 - 0.59 * target;
	vec3 quad_c = -0.14 * target;

	return (-quad_b + sqrt(quad_b * quad_b - 4.0 * quad_a * quad_c)) / (2.0 * quad_a);
}

void main()
{
	vec4 texel = texture(u_Texture, v_Uv);

	if (texel.a < 0.5)
	{
		discard;
	}

	vec3 color = texel.rgb;

	if (u_UndoToneMap != 0)
	{
		color = UndoToneMapACES(color);
	}

	FragColor = vec4(color * v_Color.rgb, v_Color.a);
}
