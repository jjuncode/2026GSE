#version 330

layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform vec2 u_TargetSize;
uniform float u_Threshold;

void main()
{
	vec2 uv = gl_FragCoord.xy / u_TargetSize;

	vec2 texel = 0.5 / u_TargetSize;
	vec3 color = texture(u_Scene, uv + vec2(-texel.x, -texel.y)).rgb;
	color += texture(u_Scene, uv + vec2(texel.x, -texel.y)).rgb;
	color += texture(u_Scene, uv + vec2(-texel.x, texel.y)).rgb;
	color += texture(u_Scene, uv + vec2(texel.x, texel.y)).rgb;
	color *= 0.25;

	float brightness = max(color.r, max(color.g, color.b));
	if (brightness <= u_Threshold)
	{
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	float excess = brightness - u_Threshold;
	color *= excess / brightness;

	FragColor = vec4(color, 1.0);
}
