#version 330

in vec2 texCoord;
in vec3 vertex_pos;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec4 per_vertex_color;

out vec4 fragColor;

// mode determination
uniform int light_mode;
uniform int per_vertex_or_per_pixel;
uniform float Shininess;

// position
uniform vec3 viewpos;
uniform vec3 lightpos;

// material
uniform vec3 material_ka;
uniform vec3 material_kd;
uniform vec3 material_ks;

// Intensity
uniform vec3 DiffuseIntensity;
uniform vec3 AmbientIntensity;

// Attenuation for point_light, spot_light
uniform float AttLightConstant;
uniform float AttLightLinear;
uniform float AttlightQuadratic;

// Spot_light
uniform float SpotLightExp;
uniform float SpotLightcutoff;
uniform vec3 SpotLight_Direction;

// mvp
uniform mat4 um4p;
uniform mat4 um4v;
uniform mat4 um4m;

// [TODO] passing texture from main.cpp, Hint: sampler2D
uniform sampler2D tex;
uniform vec2 texcoordoffset;

//fragColor = vec4(texCoord.xy, 0, 1);

void main() {

	// [TODO] sampleing from texture
	vec4 frag_textrue = texture(tex, (texCoord + texcoordoffset));

	if (per_vertex_or_per_pixel == 0) {
		fragColor = frag_textrue * per_vertex_color;
	} else { // per_vertex_or_per_pixel == 1  // left view
		vec4 frag_pos = um4v * um4m * vec4(vertex_pos.xyz, 1.0f);
		vec3 frag_normal = normalize((transpose(inverse(um4v * um4m)) * vec4(vertex_normal, 0.0f)).xyz);
		vec3 light_direction = (um4v * vec4(lightpos, 1.0f) - frag_pos).xyz;

		float d = length(light_direction);
		light_direction = normalize(light_direction);
		float attenuation_factor = 1.0 / (AttLightConstant + d * AttLightLinear + AttlightQuadratic * pow(d, 2));

		vec3 ambient_color = AmbientIntensity * material_ka; // Ambient

		float cos_angle = max(dot(normalize(vertex_normal), light_direction), 0.0); // Diffuse
		vec3 diffuse_color = DiffuseIntensity * material_kd * cos_angle;

		vec3 m_vertex_pos = (um4m * vec4(vertex_pos, 1.0f)).xyz; // specular
		vec3 view_direction = normalize(viewpos - m_vertex_pos);
		vec3 Halfway_vector = normalize(light_direction + view_direction);
		float h_cos_angle = max(dot(frag_normal, Halfway_vector), 0.0f);
		vec3 specular_color = material_ks * pow(h_cos_angle, Shininess);

		if (light_mode == 0){ //  directional_light_mode
			fragColor = frag_textrue * vec4( diffuse_color + ambient_color + specular_color, 1.0f);
		} else if(light_mode == 1){ // point light mode
			fragColor = frag_textrue * vec4( attenuation_factor * (ambient_color + diffuse_color + specular_color), 1.0f);
		} else if(light_mode == 2){ // spot light mode
			float SpotLight_Effect = 0.0;
			vec3 light_factor = -((um4v * vec4(lightpos, 1.0f) - frag_pos).xyz);
			if (dot(normalize(light_factor), normalize(-SpotLight_Direction)) > cos(SpotLightcutoff * 3.14159265358979323846 / 180)){
				float s = max(dot(light_direction, SpotLight_Direction), 0.0f);
				SpotLight_Effect = pow(s, SpotLightExp);
			}
			fragColor = frag_textrue * vec4( SpotLight_Effect * attenuation_factor * (ambient_color + diffuse_color + specular_color), 1.0f);
		}
	}
}
