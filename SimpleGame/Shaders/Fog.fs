#version 330

layout(location=0) out vec4 FragColor;

const vec3 FOG_COLOR = vec3(0.40, 0.45, 0.42);
const float FOG_DENSITY = 0.14;
const float THRESHOLD_LOW = 0.45;
const float THRESHOLD_HIGH = 0.66;
const float CELL_WIDTH = 420.0;
const float BAND_HEIGHT = 260.0;

uniform vec2 u_ScreenSize;
uniform float u_PixelScale;
uniform float u_Scroll;

uint HashInt(int x, int y)
{
	uint hash = uint(x) * 374761393u;
	hash += uint(y) * 668265263u;
	hash = (hash ^ (hash >> 13)) * 1274126177u;

	return hash ^ (hash >> 16);
}

float HashToUnit(uint hash)
{
	return float(hash & 0xFFFFu) / 65535.0;
}

float ValueNoise(vec2 point)
{
	vec2 cell = floor(point);
	vec2 blend = point - cell;
	blend = blend * blend * (3.0 - 2.0 * blend);

	int cell_x = int(cell.x);
	int cell_y = int(cell.y);

	float corner_00 = HashToUnit(HashInt(cell_x, cell_y));
	float corner_10 = HashToUnit(HashInt(cell_x + 1, cell_y));
	float corner_01 = HashToUnit(HashInt(cell_x, cell_y + 1));
	float corner_11 = HashToUnit(HashInt(cell_x + 1, cell_y + 1));

	float lower = mix(corner_00, corner_10, blend.x);
	float upper = mix(corner_01, corner_11, blend.x);

	return mix(lower, upper, blend.y);
}

void main()
{
	vec2 screen = gl_FragCoord.xy * u_PixelScale - u_ScreenSize * 0.5;

	float total = ValueNoise(vec2((screen.x + u_Scroll * 0.35) / CELL_WIDTH,
		screen.y / BAND_HEIGHT)) * 0.5;

	total += ValueNoise(vec2((screen.x + u_Scroll * 0.62) / (CELL_WIDTH * 0.55) + 31.0,
		screen.y / (BAND_HEIGHT * 0.70) + 17.0)) * 0.3;

	total += ValueNoise(vec2((screen.x + u_Scroll * 0.95) / (CELL_WIDTH * 0.30) + 71.0,
		screen.y / (BAND_HEIGHT * 0.45) + 53.0)) * 0.2;

	float amount = smoothstep(THRESHOLD_LOW, THRESHOLD_HIGH, total);

	FragColor = vec4(FOG_COLOR, amount * FOG_DENSITY);
}
