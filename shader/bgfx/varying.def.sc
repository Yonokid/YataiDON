vec2  v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
vec4  v_color0    : COLOR0    = vec4(1.0, 1.0, 1.0, 1.0);
float v_facing    : TEXCOORD1 = 0.0;
float v_texindex  : TEXCOORD2 = 0.0;
vec4 v_colortransform : TEXCOORD3 = vec4(0.0, 1.0, 1.0, 0.0);

vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;
vec4 a_color0    : COLOR0;
uvec4 a_indices  : BLENDINDICES;
vec4 a_weight    : BLENDWEIGHT;
vec4 i_data0     : TEXCOORD31;
vec4 i_data1     : TEXCOORD30;
vec4 i_data2     : TEXCOORD29;
vec4 i_data3     : TEXCOORD28;
