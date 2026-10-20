#version 330

layout(location=0) out vec4 FragColor;

in vec2 v_Uv;
in vec4 v_Color;
in float v_Flash;

uniform sampler2D u_Texture;
uniform int u_UndoToneMap;

// 4 x 4 Bayer threshold. One fragment of the low-res buffer is one art pixel
float BayerThreshold(vec2 frag)
{
	int x = int(mod(frag.x, 4.0));
	int y = int(mod(frag.y, 4.0));
	float table[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

	return (table[y * 4 + x] + 0.5) / 16.0;
}

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

	// Silhouette (flash above 1.5): the shape in one color, thinned with Bayer instead of blending
	if (v_Flash > 1.5)
	{
		if (v_Color.a < BayerThreshold(gl_FragCoord.xy))
		{
			discard;
		}

		FragColor = vec4(v_Color.rgb, 1.0);
		return;
	}

	vec3 color = texel.rgb;

	if (u_UndoToneMap != 0)
	{
		color = UndoToneMapACES(color);
	}

	vec3 lit = mix(color * v_Color.rgb, vec3(1.6), clamp(v_Flash, 0.0, 1.0));

	FragColor = vec4(lit, v_Color.a);
}
