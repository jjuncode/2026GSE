#version 330

layout(location=0) out vec4 FragColor;

in vec4 v_Color;

uniform int u_PixelMode;

// 4 x 4 Bayer threshold. One fragment of the low-res buffer is one art pixel
float BayerThreshold(vec2 frag)
{
	int x = int(mod(frag.x, 4.0));
	int y = int(mod(frag.y, 4.0));
	int index = y * 4 + x;

	float table[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

	return (table[index] + 0.5) / 16.0;
}

void main()
{
	if (u_PixelMode == 0)
	{
		FragColor = v_Color;
		return;
	}

	// Thin out pixels instead of blending
	float coverage = v_Color.a;
	if (coverage < BayerThreshold(gl_FragCoord.xy) * 0.9 + 0.04)
	{
		discard;
	}

	// Three bands: dark rim, body, white core
	vec3 color = v_Color.rgb;
	if (coverage > 0.75)
	{
		float peak = max(max(color.r, color.g), color.b);
		color = mix(color, vec3(peak), 0.55);
	}
	else if (coverage < 0.4)
	{
		color = color * 0.55;
	}

	FragColor = vec4(color, 1.0);
}
