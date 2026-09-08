#version 330

layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform vec2 u_ScreenSize;
uniform float u_BloomStrength;
uniform float u_Exposure;
uniform float u_BlurStart;
uniform float u_BlurRadius;
uniform float u_VignetteEdge;

vec3 ToneMapACES(vec3 x)
{
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;

	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / u_ScreenSize;

	vec2 centered = uv * 2.0 - 1.0;
	float edge_distance = length(centered);

	float blur_amount = clamp((edge_distance - u_BlurStart) / (1.0 - u_BlurStart), 0.0, 1.0);
	float radius = blur_amount * blur_amount * u_BlurRadius;
	vec2 texel = radius / u_ScreenSize;

	vec3 color = texture(u_Scene, uv).rgb * 0.25;
	color += texture(u_Scene, uv + vec2(texel.x, 0.0)).rgb * 0.125;
	color += texture(u_Scene, uv - vec2(texel.x, 0.0)).rgb * 0.125;
	color += texture(u_Scene, uv + vec2(0.0, texel.y)).rgb * 0.125;
	color += texture(u_Scene, uv - vec2(0.0, texel.y)).rgb * 0.125;
	color += texture(u_Scene, uv + texel).rgb * 0.0625;
	color += texture(u_Scene, uv - texel).rgb * 0.0625;
	color += texture(u_Scene, uv + vec2(texel.x, -texel.y)).rgb * 0.0625;
	color += texture(u_Scene, uv + vec2(-texel.x, texel.y)).rgb * 0.0625;

	color += texture(u_Bloom, uv).rgb * u_BloomStrength;

	color = ToneMapACES(color * u_Exposure);

	float vignette = mix(1.0, u_VignetteEdge, clamp(edge_distance * edge_distance * 0.62, 0.0, 1.0));
	color *= vignette;

	FragColor = vec4(color, 1.0);
}
