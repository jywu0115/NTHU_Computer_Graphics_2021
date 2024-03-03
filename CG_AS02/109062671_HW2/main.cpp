#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

# define M_PI 3.14159265358979323846

using namespace std;

// Default window size
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;

bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;

enum TransMode
{
	Static = 0,
	GeoTranslation = 1,
	GeoRotation = 2,
	GeoScaling = 3,	
	Light_editing_mode = 4,
	Shininess_editing_mode = 5,
};

enum LightMode
{
	Directional_light_mode = 0,
	Point_light_mode = 1,
	Spot_light_mode = 2
};

GLint iLoclight_mode, iLocPer_vertex_or_pixel;
GLint iLocMVP, iLocM, iLocV;
GLint iLocLightPos, iLocViewPos;
GLint iLocmaterial_ka, iLocmaterial_kd, iLocmaterial_ks;

GLint iLocDiffIntensity, iLocAmbIntensity;
GLint iLocAttConstant, iLocAttLinear, iLocAttQuadratic;
GLint iLocShininess, iLocSpotLightDirection,  iLocSpotLightExp, iLocSpotLightcutoff;

vector<string> filenames; // .obj filename list

struct PhongMaterial
{
	Vector3 Ka;
	Vector3 Kd;
	Vector3 Ks;

};

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	PhongMaterial material;
	int indexCount;
	GLuint m_texture;
} Shape;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form

	vector<Shape> shapes;
};
vector<model> models;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

// Light source located at infinite far away such as the sun
struct DirectionalLight 
{ 
	Vector3 position;
};
DirectionalLight Directional_light;

// Light source located at a specific position
struct PointLight
{
	Vector3 position;
};
PointLight Point_light;

// Light source located at a specific position with certain cutoff range
struct SpotLight 
{
	Vector3 position;
	Vector3 direction;
	GLfloat exponent;
	GLfloat cutoff;
};
SpotLight Spot_light;

struct Attenuation {
	GLfloat constant;
	GLfloat linear;
	GLfloat quadratic;
};
Attenuation Attenuation_Point_light;
Attenuation Attenuation_Spot_light;

TransMode cur_trans_mode = Static;
LightMode cur_light_mode = Directional_light_mode;

Matrix4 view_matrix;
Matrix4 project_matrix;

// Intensity
Vector3 Diffuse_intensity;
Vector3 Ambient_intensity;
float Shininess;

Shape quad;
Shape m_shpae;
int cur_idx = 0; // represent which model should be rendered now


static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l;

	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{

	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}


// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, vec.x,
		0, 1, 0, vec.y,
		0, 0, 1, vec.z,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, 0,
		0, cos(val * M_PI / 180), -sin(val * M_PI / 180), 0,
		0, sin(val * M_PI / 180), cos(val * M_PI / 180), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val * M_PI / 180), 0, sin(val * M_PI / 180), 0,
		0, 1, 0, 0,
		-sin(val * M_PI / 180), 0, cos(val * M_PI / 180), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val * M_PI / 180), -sin(val * M_PI / 180), 0, 0,
		sin(val * M_PI / 180), cos(val * M_PI / 180), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
	GLfloat Viewing_direction[3] = { // P_1P_2
	main_camera.center.x - main_camera.position.x,
	main_camera.center.y - main_camera.position.y,
	main_camera.center.z - main_camera.position.z };
	GLfloat Up_direction[3] = { // P_1P_3
		main_camera.up_vector.x,
		main_camera.up_vector.y,
		main_camera.up_vector.z
	};
	Normalize(Viewing_direction);
	GLfloat R_z[3] = { -Viewing_direction[0], -Viewing_direction[1], -Viewing_direction[2] };
	GLfloat R_x[3];
	Cross(Viewing_direction, Up_direction, R_x);
	Normalize(R_x);
	GLfloat R_y[3];
	Cross(R_z, R_x, R_y);
	Matrix4 R = Matrix4( // Find the bases with respect to the new space
		R_x[0], R_x[1], R_x[2], 0,
		R_y[0], R_y[1], R_y[2], 0,
		R_z[0], R_z[1], R_z[2], 0,
		0, 0, 0, 1
	);
	Matrix4 T = Matrix4( // Translate eye position to the origin
		1, 0, 0, -main_camera.position.x,
		0, 1, 0, -main_camera.position.y,
		0, 0, 1, -main_camera.position.z,
		0, 0, 0, 1
	);
	view_matrix = R * T;
}

// [TODO] compute orthogonal projection matrix
void setOrthogonal()
{
	// cur_proj_mode = Orthogonal;
	// project_matrix [...] = ...   referred by glortho
	GLfloat far = proj.farClip;
	GLfloat near = proj.nearClip;
	GLfloat t_x = -(proj.right + proj.left) / (proj.right - proj.left);
	GLfloat t_y = -(proj.top + proj.bottom) / (proj.top - proj.bottom);
	GLfloat t_z = -(far + near) / (far - near);

	project_matrix = Matrix4(
		2 / (proj.right - proj.left), 0, 0, t_x,
		0, 2 / (proj.top - proj.bottom), 0, t_y,
		0, 0, -2 / (far - near), t_z,
		0, 0, 0, 1
	);
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	// cur_proj_mode = Perspective;
	// project_matrix [...] = ...   referred by gluperspective
	GLfloat f = -1 / tan(proj.fovy / 2);
	GLfloat near = proj.nearClip;
	GLfloat far = proj.farClip;
	project_matrix = Matrix4(
		f / proj.aspect, 0, 0, 0,
		0, f, 0, 0,
		0, 0, (far + near) / (near - far), (2 * far * near) / (near - far),
		0, 0, -1, 0
	);
}

// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio in both perspective and orthogonal view
	proj.aspect = (float)width / (float)height;
}

// Render function for display rendering
void RenderScene(void) {	
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S;
	// [TODO] update translation, rotation and scaling
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);

	Matrix4 MVP, M, V;
	GLfloat mvp[16], m[16], v[16];
	
	// [TODO] multiply all the matrix
	M = T * R * S;
	V = view_matrix;  
	MVP = project_matrix * view_matrix * T * R * S;

	// [TODO] row-major ---> column-major
	std::swap(M[1], M[4]);
	std::swap(M[2], M[8]);
	std::swap(M[3], M[12]);
	std::swap(M[6], M[9]);
	std::swap(M[7], M[13]);
	std::swap(M[11], M[14]);

	std::swap(V[1], V[4]);
	std::swap(V[2], V[8]);
	std::swap(V[3], V[12]);
	std::swap(V[6], V[9]);
	std::swap(V[7], V[13]);
	std::swap(V[11], V[14]);

	std::swap(MVP[1], MVP[4]);
	std::swap(MVP[2], MVP[8]);
	std::swap(MVP[3], MVP[12]);
	std::swap(MVP[6], MVP[9]);
	std::swap(MVP[7], MVP[13]);
	std::swap(MVP[11], MVP[14]);

	m[0] = 1;  m[4] = 0;   m[8] = 0;    m[12] = 0;
	m[1] = 0;  m[5] = 1;   m[9] = 0;    m[13] = 0;
	m[2] = 0;  m[6] = 0;   m[10] = 1;   m[14] = 0;
	m[3] = 0;  m[7] = 0;   m[11] = 0;   m[15] = 1;

	v[0] = 1;  v[4] = 0;   v[8] = 0;    v[12] = 0;
	v[1] = 0;  v[5] = 1;   v[9] = 0;    v[13] = 0;
	v[2] = 0;  v[6] = 0;   v[10] = 1;   v[14] = 0;
	v[3] = 0;  v[7] = 0;   v[11] = 0;   v[15] = 1;

	mvp[0] = 1;  mvp[4] = 0;   mvp[8] = 0;    mvp[12] = 0;
	mvp[1] = 0;  mvp[5] = 1;   mvp[9] = 0;    mvp[13] = 0;
	mvp[2] = 0;  mvp[6] = 0;   mvp[10] = 1;   mvp[14] = 0;
	mvp[3] = 0;  mvp[7] = 0;   mvp[11] = 0;   mvp[15] = 1;
	
	for (int i = 0; i < 16; i++)
		m[i] = GLfloat(M[i]);
	for (int i = 0; i < 16; i++)
		v[i] = GLfloat(V[i]);
	for (int i = 0; i < 16; i++)
		mvp[i] = GLfloat(MVP[i]);
	
	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(iLocM, 1, GL_FALSE, m);
	glUniformMatrix4fv(iLocV, 1, GL_FALSE, v);
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);

	GLint light_mode = 0; // default static

	// Shininess, viewpos, Intensity
	glUniform1f(iLocShininess, Shininess);
	glUniform3f(iLocViewPos, main_camera.position.x, main_camera.position.y, main_camera.position.z);

	glUniform3f(iLocDiffIntensity, Diffuse_intensity.x, Diffuse_intensity.y, Diffuse_intensity.z);
	glUniform3f(iLocAmbIntensity, Ambient_intensity.x, Ambient_intensity.y, Ambient_intensity.z);

	if (cur_light_mode == Directional_light_mode) {
		light_mode = 0;
		glUniform1i(iLoclight_mode, light_mode);
		glUniform3f(iLocLightPos, Directional_light.position.x, Directional_light.position.y, Directional_light.position.z);
	}
	else if (cur_light_mode == Point_light_mode) {
		light_mode = 1;
		glUniform1i(iLoclight_mode, light_mode);
		glUniform3f(iLocLightPos, Point_light.position.x, Point_light.position.y, Point_light.position.z);
		glUniform1f(iLocAttConstant, Attenuation_Point_light.constant);
		glUniform1f(iLocAttLinear, Attenuation_Point_light.linear);
		glUniform1f(iLocAttQuadratic, Attenuation_Point_light.quadratic);

	}
	else if (cur_light_mode == Spot_light_mode) {
		light_mode = 2;
		glUniform1i(iLoclight_mode, light_mode);
		glUniform3f(iLocLightPos, Spot_light.position.x, Spot_light.position.y, Spot_light.position.z);
		glUniform1f(iLocSpotLightExp, Spot_light.exponent);
		glUniform1f(iLocSpotLightcutoff, Spot_light.cutoff);
		glUniform3f(iLocSpotLightDirection, Spot_light.direction.x, Spot_light.direction.y, Spot_light.direction.z);
		glUniform1f(iLocAttConstant, Attenuation_Spot_light.constant);
		glUniform1f(iLocAttLinear, Attenuation_Spot_light.linear);
		glUniform1f(iLocAttQuadratic, Attenuation_Spot_light.quadratic);
	}
	
	// Per Vertex Lighting
	int Per_vertex_or_pixel = 0;
	glUniform1i(iLocPer_vertex_or_pixel, Per_vertex_or_pixel);
	glViewport(0, 0, WINDOW_WIDTH / 2, WINDOW_HEIGHT);

	for (int i = 0; i < models[cur_idx].shapes.size(); i++) {
		glUniform3f(iLocmaterial_ka, models[cur_idx].shapes[i].material.Ka.x, models[cur_idx].shapes[i].material.Ka.y, models[cur_idx].shapes[i].material.Ka.z);
		glUniform3f(iLocmaterial_kd, models[cur_idx].shapes[i].material.Kd.x, models[cur_idx].shapes[i].material.Kd.y, models[cur_idx].shapes[i].material.Kd.z);
		glUniform3f(iLocmaterial_ks, models[cur_idx].shapes[i].material.Ks.x, models[cur_idx].shapes[i].material.Ks.y, models[cur_idx].shapes[i].material.Ks.z);
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}

	// Per Pixel Lighting
	Per_vertex_or_pixel = 1;
	glUniform1i(iLocPer_vertex_or_pixel, Per_vertex_or_pixel);
	glViewport(WINDOW_WIDTH / 2, 0, WINDOW_WIDTH / 2, WINDOW_HEIGHT);

	for (int i = 0; i < models[cur_idx].shapes.size(); i++) {
		glUniform3f(iLocmaterial_ka, models[cur_idx].shapes[i].material.Ka.x, models[cur_idx].shapes[i].material.Ka.y, models[cur_idx].shapes[0].material.Ka.z);
		glUniform3f(iLocmaterial_kd, models[cur_idx].shapes[i].material.Kd.x, models[cur_idx].shapes[i].material.Kd.y, models[cur_idx].shapes[0].material.Kd.z);
		glUniform3f(iLocmaterial_ks, models[cur_idx].shapes[i].material.Ks.x, models[cur_idx].shapes[i].material.Ks.y, models[cur_idx].shapes[0].material.Ks.z);
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if (action != GLFW_RELEASE) {
		switch (key) {
		case GLFW_KEY_ESCAPE: //Close the window
			exit(0);
		case GLFW_KEY_Z: // Switch the model and reset some para?
			cur_idx = (cur_idx + 1) % 5;
			break;
		case GLFW_KEY_X: // Switch the model
			cur_idx -= 1;
			if (cur_idx < 0) {
				cur_idx += 5;
				cur_idx = cur_idx % 5;
			}
			break;
		case GLFW_KEY_T: // Switch to translation mode
			cur_trans_mode = GeoTranslation;
			break;
		case GLFW_KEY_S: // Switch to scale mode
			cur_trans_mode = GeoScaling;
			break;
		case GLFW_KEY_R: // Switch to rotation mode
			cur_trans_mode = GeoRotation;
			break;
		case GLFW_KEY_L: // Switch between directional/point/spotlight
			if(cur_light_mode == Directional_light_mode){
				std::cout << "point light" << std::endl;
				cur_light_mode = Point_light_mode;
			}
			else if(cur_light_mode == Point_light_mode){
				std::cout << "spot light" << std::endl;
				cur_light_mode = Spot_light_mode;
			}
			else{
				std::cout << "directional light" << std::endl;
				cur_light_mode = Directional_light_mode;
			}
			break;
		case GLFW_KEY_K: // Switch to light editing mode
			cur_trans_mode = Light_editing_mode;
			break;
		case GLFW_KEY_J: // Switch to shininess editing mode
			cur_trans_mode = Shininess_editing_mode;
			break;
		}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
	if (cur_trans_mode == GeoTranslation) { // Translation mode
	// if scroll up, then object will be more close.
	// if scroll down, then object will be farther.
		models[cur_idx].position.z += yoffset * 0.1f;
	}
	else if (cur_trans_mode == GeoRotation) { // rotation mode
		// if scroll up, then object will counterclockwise.
		// if scroll down, then object will clockwise.
		if (yoffset > 0)
			models[cur_idx].rotation.z += 1.0f;
		else
			models[cur_idx].rotation.z -= 1.0f;
	}
	else if (cur_trans_mode == GeoScaling) { // Scaling mode
		// if scroll up, then object will be bigger.
		// if scroll down, then object will be smaller.		
		models[cur_idx].scale.z += yoffset * 0.01f;
		if (models[cur_idx].scale.z <= 0)
			models[cur_idx].scale.z = 0;
	}
	else if (cur_trans_mode == Light_editing_mode){
		if(cur_light_mode == Directional_light_mode){
			if (yoffset > 0) {
				Diffuse_intensity.x += 0.1f;
				Diffuse_intensity.y += 0.1f;
				Diffuse_intensity.z += 0.1f;
			}
			else {
				Diffuse_intensity.x -= 0.1f;
				Diffuse_intensity.y -= 0.1f;
				Diffuse_intensity.z -= 0.1f;
			}
		}
		else if(cur_light_mode == Point_light_mode){
			if (yoffset > 0) {
				Diffuse_intensity.x += 0.1f;
				Diffuse_intensity.y += 0.1f;
				Diffuse_intensity.z += 0.1f;
			}
			else {
				Diffuse_intensity.x -= 0.1f;
				Diffuse_intensity.y -= 0.1f;
				Diffuse_intensity.z -= 0.1f;
			}
		}
		else if (cur_light_mode == Spot_light_mode){
			if (yoffset > 0)
				Spot_light.cutoff += 1.0f;
			else
				Spot_light.cutoff -= 1.0f;
			std::cout << "spot light cutoff: " << Spot_light.cutoff * M_PI / 180 << std::endl;
		}
	}
	else if (cur_trans_mode == Shininess_editing_mode){
		if (yoffset > 0)
			Shininess += 1.0f;
		else
			Shininess -= 1.0f;

		if (cur_light_mode == Directional_light_mode)
			std::cout << "directional light shininess: " << Shininess << std::endl;
		else if (cur_light_mode == Point_light_mode)
			std::cout << "point light shininess: " << Shininess << std::endl;
		else if (cur_light_mode == Spot_light_mode)
			std::cout << "spot light shininess: " << Shininess << std::endl;
		
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
		
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && mouse_pressed == false)
		mouse_pressed = true;
	else {
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
			mouse_pressed = false;
			return;
		}
		else {
			if (cur_trans_mode == GeoTranslation) { // translation mode
				// if cursor shift right, then object will be translated right.
				// if cursor shift left, then object will be translated left.
				// if cursor shift up, then object will be translated up.
				// if cursor shift down, then object will be translated down.

				if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
					models[cur_idx].position.x += 0.02f;
					models[cur_idx].position.y += 0.02f;
				}
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
					models[cur_idx].position.x += 0.02f;
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
					models[cur_idx].position.x += 0.02f;
					models[cur_idx].position.y += 0.02f;
				}
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
					models[cur_idx].position.y -= 0.02f;
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
					models[cur_idx].position.y += 0.02f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
					models[cur_idx].position.x -= 0.02f;
					models[cur_idx].position.y -= 0.02f;
				}
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
					models[cur_idx].position.x -= 0.02f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
					models[cur_idx].position.x -= 0.02f;
					models[cur_idx].position.y += 0.02f;
				}
				starting_press_x = xpos;
				starting_press_y = ypos;
			}
			else if (cur_trans_mode == GeoRotation) { // rotation mode
				// if cursor shift right, then object will counterclockwise.
				// if cursor shift left, then object will clockwise.
				// if cursor shiht up, then object will fall forward.
				// if cursor shit down, then object will fall back.

				if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
					models[cur_idx].rotation.y += 1.0f;
					models[cur_idx].rotation.x -= 1.0f;
				}
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
					models[cur_idx].rotation.y += 1.0f;
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
					models[cur_idx].rotation.y += 1.0f;
					models[cur_idx].rotation.x -= 1.0f;
				}
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
					models[cur_idx].rotation.x += 1.0f;
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
					models[cur_idx].rotation.x -= 1.0f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
					models[cur_idx].rotation.y -= 1.0f;
					models[cur_idx].rotation.x += 1.0f;
				}
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
					models[cur_idx].rotation.y -= 1.0f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
					models[cur_idx].rotation.y -= 1.0f;
					models[cur_idx].rotation.x -= 1.0f;
				}
				starting_press_x = xpos;
				starting_press_y = ypos;
			}
			else if (cur_trans_mode == GeoScaling) { // scale mode			
				// if cursor shift right, then object will be thinner. // followed by demo.
				// if cursor shift left, then object will be fatter. // followed by demo.
				// if cursor shiht up, then object will be higher.
				// if cursor shit down, then object will be shorter.

				if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
					models[cur_idx].scale.x -= 0.02f;
					models[cur_idx].scale.y += 0.02f;
				}
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
					models[cur_idx].scale.x -= 0.02f;
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
					models[cur_idx].scale.x -= 0.02f;
					models[cur_idx].scale.y += 0.02f;
				}
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
					models[cur_idx].scale.y -= 0.02f;
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
					models[cur_idx].scale.y += 0.02f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
					models[cur_idx].scale.x += 0.02f;
					models[cur_idx].scale.y -= 0.02f;
				}
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
					models[cur_idx].scale.x += 0.02f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
					models[cur_idx].scale.x += 0.02f;
					models[cur_idx].scale.y += 0.02f;
				}
				starting_press_x = xpos;
				starting_press_y = ypos;
			}
			else if (cur_trans_mode == Light_editing_mode) {
				if (cur_light_mode == Directional_light_mode) {
					if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
						Directional_light.position.x += 0.02f;
						Directional_light.position.y += 0.02f;
					}
					else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
						Directional_light.position.x += 0.02f;
					else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
						Directional_light.position.x += 0.02f;
						Directional_light.position.y += 0.02f;
					}
					else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
						Directional_light.position.y -= 0.02f;
					else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
						Directional_light.position.y += 0.02f;
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
						Directional_light.position.x -= 0.02f;
						Directional_light.position.y -= 0.02f;
					}
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
						Directional_light.position.x -= 0.02f;
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
						Directional_light.position.x -= 0.02f;
						Directional_light.position.y += 0.02f;
					}
					starting_press_x = xpos;
					starting_press_y = ypos;
					std::cout << "directional light position = ( " << Directional_light.position.x << ", " << Directional_light.position.y << ", " << Directional_light.position.z << " )" << std::endl;
				}
				else if (cur_light_mode == Point_light_mode) {
					if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
						Point_light.position.x += 0.02f;
						Point_light.position.y += 0.02f;
					}
					else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
						Point_light.position.x += 0.02f;
					else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
						Point_light.position.x += 0.02f;
						Point_light.position.y += 0.02f;
					}
					else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
						Point_light.position.y -= 0.02f;
					else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
						Point_light.position.y += 0.02f;
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
						Point_light.position.x -= 0.02f;
						Point_light.position.y -= 0.02f;
					}
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
						Point_light.position.x -= 0.02f;
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
						Point_light.position.x -= 0.02f;
						Point_light.position.y += 0.02f;
					}
					starting_press_x = xpos;
					starting_press_y = ypos;
					std::cout << "point light position = ( " << Point_light.position.x << ", " << Point_light.position.y << ", " << Point_light.position.z << " )" << std::endl;
				}
				else if (cur_light_mode == Spot_light_mode){
					if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
						Spot_light.position.x += 0.02f;
						Spot_light.position.y += 0.02f;
					}
					else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
						Point_light.position.x += 0.02f;
					else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
						Spot_light.position.x += 0.02f;
						Spot_light.position.y += 0.02f;
					}
					else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
						Spot_light.position.y -= 0.02f;
					else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
						Spot_light.position.y += 0.02f;
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
						Spot_light.position.x -= 0.02f;
						Spot_light.position.y -= 0.02f;
					}
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
						Spot_light.position.x -= 0.02f;
					else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
						Spot_light.position.x -= 0.02f;
						Spot_light.position.y += 0.02f;
					}
					starting_press_x = xpos;
					starting_press_y = ypos;
					std::cout << "spot light position = ( " << Spot_light.position.x << ", " << Spot_light.position.y << ", " << Spot_light.position.z << " )" << std::endl;
				}
			}
		}
	}
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p,f);
	glAttachShader(p,v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);
	
	iLoclight_mode = glGetUniformLocation(p, "light_mode");
	iLocPer_vertex_or_pixel = glGetUniformLocation(p, "Per_vertex_or_pixel");

	iLocM = glGetUniformLocation(p, "m");
	iLocV = glGetUniformLocation(p, "v");
	iLocMVP = glGetUniformLocation(p, "mvp");

	iLocViewPos = glGetUniformLocation(p, "viewpos");
	iLocLightPos = glGetUniformLocation(p, "lightpos");
	
	iLocmaterial_ka = glGetUniformLocation(p, "material_ka");
	iLocmaterial_kd = glGetUniformLocation(p, "material_kd");
	iLocmaterial_ks = glGetUniformLocation(p, "material_ks");
	
	iLocDiffIntensity = glGetUniformLocation(p, "DiffuseIntensity");
	iLocAmbIntensity = glGetUniformLocation(p, "AmbientIntensity");

	iLocSpotLightExp = glGetUniformLocation(p, "SpotLightExp");
	iLocSpotLightcutoff = glGetUniformLocation(p, "SpotLightcutoff");
	iLocSpotLightDirection = glGetUniformLocation(p, "SpotLight_Direction");
	iLocShininess = glGetUniformLocation(p, "Shininess");

	iLocAttConstant = glGetUniformLocation(p, "AttLightConstant");
	iLocAttLinear = glGetUniformLocation(p, "AttLightLinear");
	iLocAttQuadratic = glGetUniformLocation(p, "AttLightQuadratic");


	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, vector<GLfloat>& normals, tinyobj::shape_t* shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++) {
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++) {
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
			// Optional: vertex normals
			if (idx.normal_index >= 0) {
				normals.push_back(attrib->normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 2]);
			}
		}
		index_offset += fv;
	}
}

string GetBaseDir(const string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;
	vector<GLfloat> normals;

	string err;
	string warn;

	string base_dir = GetBaseDir(model_path); // handle .mtl with relative path

#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str(), base_dir.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Material size %d\n", shapes.size(), materials.size());
	model tmp_model;

	vector<PhongMaterial> allMaterial;
	for (int i = 0; i < materials.size(); i++)
	{
		PhongMaterial material;
		material.Ka = Vector3(materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
		material.Kd = Vector3(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
		material.Ks = Vector3(materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
		allMaterial.push_back(material);
	}

	for (int i = 0; i < shapes.size(); i++)
	{

		vertices.clear();
		colors.clear();
		normals.clear();
		normalization(&attrib, vertices, colors, normals, &shapes[i]);
		// printf("Vertices size: %d", vertices.size() / 3);

		Shape tmp_shape;
		glGenVertexArrays(1, &tmp_shape.vao);
		glBindVertexArray(tmp_shape.vao);

		glGenBuffers(1, &tmp_shape.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		tmp_shape.vertex_count = vertices.size() / 3;

		glGenBuffers(1, &tmp_shape.p_color);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &tmp_shape.p_normal);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_normal);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GL_FLOAT), &normals.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		// not support per face material, use material of first face
		if (allMaterial.size() > 0)
			tmp_shape.material = allMaterial[shapes[i].mesh.material_ids[0]];
		tmp_model.shapes.push_back(tmp_shape);
	}
	shapes.clear();
	materials.clear();
	models.push_back(tmp_model);
}

void initParameter()
{
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix

	Diffuse_intensity = Vector3(1, 1, 1);
	Ambient_intensity = Vector3(0.15, 0.15, 0.15);
	Shininess = 64.0f;
					
	// directional light parameter
	Directional_light.position = Vector3(1, 1, 1);
	// point light parameter
	Point_light.position = Vector3(0, 2, 1);
	Attenuation_Point_light.constant = 0.01f;
	Attenuation_Point_light.linear = 0.8f;
	Attenuation_Point_light.quadratic = 0.1f;
	// spot light parameter
	Spot_light.position = Vector3(0, 0, 2);
	Spot_light.direction = Vector3(0, 0, 1);
	Spot_light.exponent = 50.0f;
	Spot_light.cutoff = 30.0f; 
	Attenuation_Spot_light.constant = 0.05f;
	Attenuation_Spot_light.linear = 0.3f;
	Attenuation_Spot_light.quadratic = 0.6f;
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);
	vector<string> model_list{ "../NormalModels/bunny5KN.obj", "../NormalModels/dragon10KN.obj", "../NormalModels/lucy25KN.obj", "../NormalModels/teapot4KN.obj", "../NormalModels/dolphinN.obj"};
	
	// [TODO] Load five model at here
	cout << "[" << cur_idx << "]:" << model_list[cur_idx % 5] << endl;
	LoadModels(model_list[cur_idx % 5]);
	cout << "[" << (cur_idx + 1) << "]:" << model_list[(cur_idx + 1) % 5] << endl;
	LoadModels(model_list[(cur_idx + 1) % 5]);
	cout << "[" << (cur_idx + 2) << "]:" << model_list[(cur_idx + 2) % 5] << endl;
	LoadModels(model_list[(cur_idx + 2) % 5]);
	cout << "[" << (cur_idx + 3) << "]:" << model_list[(cur_idx + 3) % 5] << endl;
	LoadModels(model_list[(cur_idx + 3) % 5]);
	cout << "[" << (cur_idx + 4) << "]:" << model_list[(cur_idx + 4) % 5] << endl;
	LoadModels(model_list[(cur_idx + 4) % 5]);
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char*)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char*)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char*)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char*)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}


int main(int argc, char **argv)
{
    // initial glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif

    
    // create window
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "109062671 HW2", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    
    // load OpenGL function pointer
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
	// register glfw callback functions
    glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
    while (!glfwWindowShouldClose(window))
    {
        // render
        RenderScene();
        
        // swap buffer from back to front
        glfwSwapBuffers(window);
        
        // Poll input event
        glfwPollEvents();
    }
	
	// just for compatibiliy purposes
	return 0;
}
