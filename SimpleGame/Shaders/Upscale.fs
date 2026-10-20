#version 330

layout(location=0) out vec4 FragColor;

uniform sampler2D u_Source;
uniform vec2 u_ScreenSize;
uniform float u_Scale;
uniform vec2 u_SourceSize;

void main()
{
	// Exact integer blocks. 1280 / 3 is not whole, so map by scale instead of by ratio
	vec2 texel = floor(gl_FragCoord.xy / u_Scale) + 0.5;
	vec2 uv = texel / u_SourceSize;

	FragColor = vec4(texture(u_Source, uv).rgb, 1.0);
}
