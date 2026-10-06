#version 330

layout(location=0) out vec4 FragColor;

const int MAX_LIGHT_COUNT = 8;
const int DOT_DIVISOR = 8;
const int PATCH_PERIOD = 5;

const int CODE_OPEN = 0;
const int CODE_CLIFF = 1;
const int CODE_WATER = 2;
const int CODE_ROAD = 3;
const int CODE_PLAZA = 4;
const int CODE_FIELD = 5;
const int CODE_FIELD_DEAD = 6;
const int CODE_BLIGHT = 7;
const int CODE_YARD = 8;

const float CLIFF_DEPTH = 1.75;
const float BANK_DEPTH = 0.14;
const float BAND_UNITS = 6.0;
const float EDGE_JITTER = 0.7;

const vec3 SOIL_LIGHT = vec3(0.31, 0.29, 0.26);
const vec3 SOIL_MID = vec3(0.25, 0.23, 0.21);
const vec3 SOIL_DARK = vec3(0.17, 0.16, 0.15);
const vec3 MOSS = vec3(0.21, 0.25, 0.17);
const vec3 DEAD_GRASS = vec3(0.34, 0.31, 0.21);
const vec3 ASH_SPECK = vec3(0.42, 0.41, 0.40);

const vec3 ROAD_LIGHT = vec3(0.36, 0.31, 0.24);
const vec3 ROAD_DARK = vec3(0.26, 0.22, 0.17);

const vec3 YARD_LIGHT = vec3(0.33, 0.29, 0.22);
const vec3 YARD_GRASS = vec3(0.24, 0.30, 0.18);

const vec3 STONE_LIGHT = vec3(0.42, 0.40, 0.38);
const vec3 STONE_DARK = vec3(0.31, 0.30, 0.29);
const vec3 MORTAR = vec3(0.17, 0.16, 0.16);

const vec3 FURROW_SOIL = vec3(0.29, 0.22, 0.15);
const vec3 FURROW_DARK = vec3(0.19, 0.14, 0.10);
const vec3 SPROUT = vec3(0.28, 0.38, 0.18);
const vec3 DEAD_SOIL = vec3(0.25, 0.22, 0.18);
const vec3 DEAD_FURROW = vec3(0.17, 0.15, 0.13);
const vec3 DEAD_STALK = vec3(0.38, 0.34, 0.24);

const vec3 BLIGHT_SOIL = vec3(0.17, 0.13, 0.18);
const vec3 BLIGHT_DARK = vec3(0.10, 0.08, 0.11);
const vec3 BLIGHT_VEIN = vec3(0.62, 0.20, 0.56);

const vec3 RIM_LIGHT = vec3(0.42, 0.40, 0.37);
const vec3 RIM_SHADE = vec3(0.13, 0.12, 0.12);
const vec3 FACE_LIT = vec3(0.35, 0.33, 0.31);
const vec3 FACE_SHADE = vec3(0.20, 0.19, 0.19);
const vec3 ABYSS = vec3(0.02, 0.02, 0.03);
const vec3 WATER_DEEP = vec3(0.03, 0.05, 0.04);
const vec3 WATER_GLINT = vec3(0.17, 0.23, 0.19);
const vec3 WET_BANK = vec3(0.12, 0.11, 0.09);

uniform vec2 u_ScreenSize;
uniform float u_PixelScale;
uniform vec2 u_TileSize;
uniform ivec2 u_CameraTile;
uniform vec2 u_CameraFrac;
uniform int u_ChunkTileCount;
uniform int u_ShowChunkGrid;

uniform sampler2D u_ZoneTiles;
uniform ivec2 u_ZoneOrigin;
uniform int u_ZoneSize;

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

vec3 UndoToneMapACES(vec3 display)
{
	vec3 target = clamp(display, 0.0, 0.92);
	vec3 quad_a = 2.51 - 2.43 * target;
	vec3 quad_b = 0.03 - 0.59 * target;
	vec3 quad_c = -0.14 * target;

	return (-quad_b + sqrt(quad_b * quad_b - 4.0 * quad_a * quad_c)) / (2.0 * quad_a);
}

int GetCode(ivec2 tile)
{
	ivec2 local = tile - u_ZoneOrigin;

	if (local.x < 0 || local.y < 0 || local.x >= u_ZoneSize || local.y >= u_ZoneSize)
	{
		return CODE_CLIFF;
	}

	return int(texelFetch(u_ZoneTiles, local, 0).r * 255.0 + 0.5);
}

bool IsSurface(int code)
{
	return code != CODE_CLIFF && code != CODE_WATER;
}

float TileNoise(ivec2 tile, vec2 inner, int period, int salt)
{
	ivec2 cell = ivec2(FloorDiv(tile.x, period), FloorDiv(tile.y, period));
	vec2 local = (vec2(float(FloorMod(tile.x, period)), float(FloorMod(tile.y, period))) + inner) / float(period);
	vec2 blend = local * local * (3.0 - 2.0 * local);

	float corner_00 = HashToUnit(HashInt(cell.x + salt, cell.y));
	float corner_10 = HashToUnit(HashInt(cell.x + 1 + salt, cell.y));
	float corner_01 = HashToUnit(HashInt(cell.x + salt, cell.y + 1));
	float corner_11 = HashToUnit(HashInt(cell.x + 1 + salt, cell.y + 1));

	return mix(mix(corner_00, corner_10, blend.x), mix(corner_01, corner_11, blend.x), blend.y);
}

vec3 SoilColor(ivec2 tile, vec2 dot_center, ivec2 dot_index, float grain)
{
	float speckle = HashToUnit(HashInt(FloorDiv(dot_index.x, PATCH_PERIOD), FloorDiv(dot_index.y, PATCH_PERIOD)));
	float broad = TileNoise(tile, dot_center, 9, 0) * 0.65 + TileNoise(tile, dot_center, 4, 1301) * 0.35;
	float dirt = TileNoise(tile, dot_center, 6, 2711);
	float moss = TileNoise(tile, dot_center, 5, 4127);

	vec3 color = mix(SOIL_MID, SOIL_LIGHT, smoothstep(0.35, 0.75, broad));
	color = mix(color, SOIL_DARK, smoothstep(0.60, 0.80, dirt) * 0.8);

	if (moss > 0.64 && grain > 0.45)
	{
		color = mix(color, MOSS, 0.7);
	}
	else if (moss < 0.22 && grain > 0.7)
	{
		color = mix(color, DEAD_GRASS, 0.6);
	}

	if (grain > 0.975)
	{
		color = ASH_SPECK;
	}

	return color * (0.93 + speckle * 0.05 + grain * 0.04);
}

vec3 RoadColor(ivec2 tile, vec2 dot_center, float grain)
{
	float rut = TileNoise(tile, dot_center, 3, 5153);
	vec3 color = mix(ROAD_DARK, ROAD_LIGHT, smoothstep(0.25, 0.7, rut));

	if (grain > 0.93)
	{
		color *= 0.8;
	}

	return color * (0.95 + grain * 0.06);
}

vec3 YardColor(ivec2 tile, vec2 dot_center, float grain)
{
	float patch_noise = TileNoise(tile, dot_center, 4, 6203);
	vec3 color = mix(ROAD_DARK, YARD_LIGHT, smoothstep(0.2, 0.8, patch_noise));

	if (patch_noise > 0.62 && grain > 0.4)
	{
		color = mix(color, YARD_GRASS, 0.75);
	}

	return color * (0.94 + grain * 0.06);
}

vec3 PlazaColor(ivec2 dot_index)
{
	int stone_row = FloorDiv(dot_index.y, 4);
	int shift = FloorMod(stone_row, 2) * 2;
	int stone_col = FloorDiv(dot_index.x + shift, 4);

	if (FloorMod(dot_index.y, 4) == 3 || FloorMod(dot_index.x + shift, 4) == 3)
	{
		return MORTAR;
	}

	float tone = HashToUnit(HashInt(stone_col * 7 + 11, stone_row * 13 + 5));
	vec3 color = mix(STONE_DARK, STONE_LIGHT, tone);

	float wear = HashToUnit(HashInt(dot_index.x, dot_index.y + 3001));
	if (wear > 0.9)
	{
		color *= 0.85;
	}

	return color;
}

vec3 FieldColor(ivec2 dot_index, float grain, bool alive)
{
	bool furrow = FloorMod(dot_index.y, 3) == 0;

	if (alive)
	{
		if (furrow)
		{
			return FURROW_DARK;
		}

		if (grain > 0.78)
		{
			return SPROUT;
		}

		return FURROW_SOIL * (0.94 + grain * 0.08);
	}

	if (furrow)
	{
		return DEAD_FURROW;
	}

	if (grain > 0.88)
	{
		return DEAD_STALK;
	}

	return DEAD_SOIL * (0.94 + grain * 0.08);
}

vec3 BlightColor(ivec2 tile, vec2 dot_center, float grain)
{
	float broad = TileNoise(tile, dot_center, 5, 7331);
	float vein_noise = TileNoise(tile, dot_center, 3, 8123);

	vec3 color = mix(BLIGHT_DARK, BLIGHT_SOIL, smoothstep(0.2, 0.8, broad));

	if (abs(vein_noise - 0.5) < 0.035)
	{
		color = mix(color, BLIGHT_VEIN, 0.65);
	}

	return color * (0.92 + grain * 0.1);
}

vec3 SurfaceColor(int code, ivec2 tile, vec2 tile_fract, ivec2 dot_index, float grain)
{
	ivec2 dot_local = ivec2(floor(tile_fract * float(DOT_DIVISOR)));
	vec2 dot_center = (vec2(dot_local) + 0.5) / float(DOT_DIVISOR);

	if (code == CODE_ROAD)
	{
		return RoadColor(tile, dot_center, grain);
	}

	if (code == CODE_PLAZA)
	{
		return PlazaColor(dot_index);
	}

	if (code == CODE_FIELD)
	{
		return FieldColor(dot_index, grain, true);
	}

	if (code == CODE_FIELD_DEAD)
	{
		return FieldColor(dot_index, grain, false);
	}

	if (code == CODE_BLIGHT)
	{
		return BlightColor(tile, dot_center, grain);
	}

	if (code == CODE_YARD)
	{
		return YardColor(tile, dot_center, grain);
	}

	return SoilColor(tile, dot_center, dot_index, grain);
}

vec3 AddRim(vec3 color, ivec2 tile, vec2 tile_fract)
{
	if (tile_fract.x > 0.88 && GetCode(tile + ivec2(1, 0)) == CODE_CLIFF)
	{
		return RIM_LIGHT;
	}

	if (tile_fract.y > 0.88 && GetCode(tile + ivec2(0, 1)) == CODE_CLIFF)
	{
		return mix(RIM_LIGHT, SOIL_DARK, 0.35);
	}

	if (tile_fract.x < 0.06 && GetCode(tile - ivec2(1, 0)) == CODE_CLIFF)
	{
		return RIM_SHADE;
	}

	if (tile_fract.y < 0.06 && GetCode(tile - ivec2(0, 1)) == CODE_CLIFF)
	{
		return RIM_SHADE;
	}

	return color;
}

vec3 FaceColor(ivec2 rim_tile, bool faces_x, float depth_units, float along)
{
	float band = floor(depth_units / BAND_UNITS);
	int segment = int(floor(along * 4.0));
	float jitter = HashToUnit(HashInt(rim_tile.x * 31 + segment, rim_tile.y * 57 + int(band)));
	float layer = mod(band, 2.0);

	vec3 color = FACE_SHADE;
	if (faces_x)
	{
		color = FACE_LIT;
	}

	color *= 0.84 + layer * 0.12 + jitter * 0.10;

	float band_position = depth_units - band * BAND_UNITS;
	if (band_position < 2.0 && jitter > 0.55)
	{
		color *= 0.72;
	}

	float depth_ratio = clamp(depth_units / (CLIFF_DEPTH * u_TileSize.y), 0.0, 1.0);

	return mix(color, ABYSS, depth_ratio * 0.75);
}

vec3 CliffColor(ivec2 tile, vec2 tile_fract)
{
	ivec2 walk_tile = tile;
	vec2 point = tile_fract;
	float travelled = 0.0;

	for (int iteration = 0; iteration < 8; ++iteration)
	{
		bool cross_x = point.x <= point.y;
		float advance = min(point.x, point.y);

		travelled += advance;
		if (travelled > CLIFF_DEPTH)
		{
			break;
		}

		point -= vec2(advance);

		float along = point.x;
		if (cross_x)
		{
			walk_tile.x -= 1;
			point.x = 1.0;
			along = point.y;
		}
		else
		{
			walk_tile.y -= 1;
			point.y = 1.0;
		}

		if (GetCode(walk_tile) != CODE_CLIFF)
		{
			return FaceColor(walk_tile, cross_x, travelled * u_TileSize.y, along);
		}
	}

	ivec2 dot_local = ivec2(floor(tile_fract * float(DOT_DIVISOR)));
	float speck = HashToUnit(HashInt(tile.x * DOT_DIVISOR + dot_local.x, tile.y * DOT_DIVISOR + dot_local.y + 7919));

	return ABYSS * (0.7 + speck * 0.6);
}

vec3 WaterColor(ivec2 tile, vec2 tile_fract, float grain)
{
	ivec2 walk_tile = tile;
	vec2 point = tile_fract;
	float travelled = 0.0;

	for (int iteration = 0; iteration < 4; ++iteration)
	{
		bool cross_x = point.x <= point.y;
		float advance = min(point.x, point.y);

		travelled += advance;
		if (travelled > BANK_DEPTH)
		{
			break;
		}

		point -= vec2(advance);

		if (cross_x)
		{
			walk_tile.x -= 1;
			point.x = 1.0;
		}
		else
		{
			walk_tile.y -= 1;
			point.y = 1.0;
		}

		if (GetCode(walk_tile) != CODE_WATER)
		{
			return WET_BANK;
		}
	}

	if (grain > 0.985)
	{
		return WATER_GLINT;
	}

	return WATER_DEEP;
}

void main()
{
	vec2 screen = gl_FragCoord.xy * u_PixelScale - u_ScreenSize * 0.5;

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

	int code = GetCode(tile_index);
	vec3 color = vec3(0.0);

	if (code == CODE_CLIFF)
	{
		color = CliffColor(tile_index, tile_fract);
	}
	else if (code == CODE_WATER)
	{
		color = WaterColor(tile_index, tile_fract, grain);
	}
	else
	{
		vec2 shake = vec2(HashToUnit(HashInt(dot_index.x + 131, dot_index.y)), HashToUnit(HashInt(dot_index.x, dot_index.y + 977))) - 0.5;
		vec2 dot_center = (vec2(dot_local) + 0.5) / float(DOT_DIVISOR);
		ivec2 shaken_tile = u_CameraTile + ivec2(floor(vec2(tile_step) + dot_center + shake * EDGE_JITTER));
		int surface_code = GetCode(shaken_tile);

		if (IsSurface(surface_code) == false)
		{
			surface_code = code;
		}

		color = AddRim(SurfaceColor(surface_code, tile_index, tile_fract, dot_index, grain), tile_index, tile_fract);
	}

	color = UndoToneMapACES(color);

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
