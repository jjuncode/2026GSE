#version 330

layout(location=0) out vec4 FragColor;

uniform sampler2D u_Source;
uniform vec2 u_TargetSize;
uniform vec2 u_Direction;

void main()
{
	vec2 uv = gl_FragCoord.xy / u_TargetSize;
	vec2 offset_step = u_Direction / u_TargetSize;

	vec3 color = texture(u_Source, uv).rgb * 0.227027;
	color += texture(u_Source, uv + offset_step).rgb * 0.194594;
	color += texture(u_Source, uv - offset_step).rgb * 0.194594;
	color += texture(u_Source, uv + offset_step * 2.0).rgb * 0.121621;
	color += texture(u_Source, uv - offset_step * 2.0).rgb * 0.121621;
	color += texture(u_Source, uv + offset_step * 3.0).rgb * 0.054054;
	color += texture(u_Source, uv - offset_step * 3.0).rgb * 0.054054;
	color += texture(u_Source, uv + offset_step * 4.0).rgb * 0.016216;
	color += texture(u_Source, uv - offset_step * 4.0).rgb * 0.016216;

	FragColor = vec4(color, 1.0);
}
