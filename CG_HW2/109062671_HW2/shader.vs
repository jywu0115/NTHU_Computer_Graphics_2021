#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;

out vec3 vertex_pos;
out vec3 vertex_color;
out vec3 vertex_normal;
out vec4 Per_vertex_color;

// mode determination
uniform int light_mode;
uniform int Per_vertex_or_pixel;
uniform float Shininess;

// mvp matrix
uniform mat4 m;
uniform mat4 v;
uniform mat4 mvp;

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

void main()
{
	// [TODO]
	vertex_pos = aPos;
	vertex_color = aColor;
	vertex_normal = aNormal;
	gl_Position = mvp * vec4(aPos, 1.0);

	if(Per_vertex_or_pixel == 0){
		vec4 frag_pos = v * m * vec4(vertex_pos.xyz, 1.0f);
		vec3 frag_normal = normalize((transpose(inverse(v * m)) * vec4(vertex_normal, 0.0)).xyz);
		vec3 light_direction = (v * vec4(lightpos, 1.0f) - frag_pos).xyz;
		float d = length(light_direction);
		light_direction = normalize(light_direction);
		float attenuation_factor = 1.0 / (AttLightConstant + d*AttLightLinear + AttlightQuadratic * pow(d, 2));
		vec3 ambient_color = vertex_color * AmbientIntensity * material_ka; // Ambient
	
		float cos_angle = max(dot(normalize(vertex_normal), light_direction), 0.0); // Diffuse
		vec3 diffuse_color = vertex_color * DiffuseIntensity * material_kd * cos_angle; 

		vec3 m_vertex_pos = (m * vec4(vertex_pos, 1.0f)).xyz; // specular
		vec3 view_direction = normalize(viewpos - m_vertex_pos); 
		vec3 Halfway_vector = normalize(light_direction.xyz + view_direction);
		float h_cos_angle = max(dot(frag_normal, Halfway_vector), 0.0);
		vec3 specular_color = vertex_color * material_ks * pow(h_cos_angle, Shininess);  
		
		if (light_mode == 0){ //  directional_light_mode
			Per_vertex_color = vec4(ambient_color + diffuse_color + specular_color, 1);
		}
		else if(light_mode == 1){ // point light mode
			Per_vertex_color = vec4( attenuation_factor * (ambient_color + diffuse_color + specular_color), 1);
		}
		else if(light_mode == 2){ // spot light mode
			float SpotLight_Effect = 0.0;
			vec3 light_factor = -(v * vec4(lightpos, 1.0f) - frag_pos).xyz;
			if (dot(normalize(light_factor), normalize(-SpotLight_Direction)) > cos(SpotLightcutoff * 3.14159265358979323846 / 180)){
				float s = max(dot(light_direction, SpotLight_Direction), 0.0);
				SpotLight_Effect = pow(s, SpotLightExp);
			}
			Per_vertex_color = vec4( SpotLight_Effect * attenuation_factor * (ambient_color + diffuse_color + specular_color), 1);
		}
	}
}
	