#version 330

layout(location=0) out vec4 FragColor;

const int MAX_LIGHT_COUNT = 8;
const int DOT_DIVISOR = 8;
const int PATCH_PERIOD = 5;

uniform vec2 u_ScreenSize;
uniform vec2 u_TileSize;
uniform ivec2 u_CameraTile;
uniform vec2 u_CameraFrac;
uniform int u_ChunkTileCount;
uniform int u_ShowChunkGrid;

uniform int u_LightCount;
uniform vec2 u_LightOffset[MAX_LIGHT_COUNT];
uniform float u_LightRadius[MAX_LIGHT_COUNT];
uniform vec3 u_LightColor[MAX_LIGHT_COUNT];
uniform float u_LightStrength[MAX_LIGHT_COUNT];

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

int FloorDiv(int value, int divisor)
{
	int quotient = value / divisor;

	if (value < 0 && quotient * divisor != value)
	{
		quotient -= 1;
	}

	return quotient;
}

int FloorMod(int value, int divisor)
{
	return value - FloorDiv(value, divisor) * divisor;
}

void main()
{
	vec2 screen = gl_FragCoord.xy - u_ScreenSize * 0.5;

	float sum_axis = -screen.y / (u_TileSize.y * 0.5);
	float diff_axis = screen.x / (u_TileSize.x * 0.5);

	vec2 tile_offset = vec2(sum_axis + diff_axis, sum_axis - diff_axis) * 0.5;

	vec2 tile_float = u_CameraFrac + tile_offset;
	ivec2 tile_step = ivec2(floor(tile_float));
	ivec2 tile_index = u_CameraTile + tile_step;
	vec2 tile_fract = tile_float - vec2(tile_step);

	ivec2 dot_local = ivec2(floor(tile_fract * float(DOT_DIVISOR)));
	ivec2 dot_index = tile_index * DOT_DIVISOR + dot_local;

	float grain = HashToUnit(HashInt(dot_index.x, dot_index.y));
	float patch = HashToUnit(HashInt(FloorDiv(dot_index.x, PATCH_PERIOD), FloorDiv(dot_index.y, PATCH_PERIOD)));

	vec3 color = vec3(0.050, 0.068, 0.042);
	color += patch * vec3(0.042, 0.048, 0.026);
	color += grain * vec3(0.016, 0.018, 0.012);

	if (u_ShowChunkGrid != 0)
	{
		if (FloorMod(tile_index.x, u_ChunkTileCount) == 0 || FloorMod(tile_index.y, u_ChunkTileCount) == 0)
		{
			color += vec3(0.16, 0.11, 0.05);
		}
	}

	for (int index = 0; index < u_LightCount; ++index)
	{
		vec2 delta = u_LightOffset[index] - tile_offset;
		float distance_tiles = length(delta);

		if (distance_tiles >= u_LightRadius[index])
		{
			continue;
		}

		float falloff = 1.0 - distance_tiles / u_LightRadius[index];
		falloff = falloff * falloff;

		color += u_LightColor[index] * (falloff * u_LightStrength[index]);
	}

	FragColor = vec4(color, 1.0);
}
