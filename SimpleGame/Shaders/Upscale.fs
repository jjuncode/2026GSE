#version 330

layout(location=0) out vec4 FragColor;

uniform sampler2D u_Source;
uniform vec2 u_ScreenSize;

void main()
{
	vec2 uv = gl_FragCoord.xy / u_ScreenSize;

	FragColor = vec4(texture(u_Source, uv).rgb, 1.0);
}
