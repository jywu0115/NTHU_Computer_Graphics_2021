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

using namespace std;

// Default window size
const int WINDOW_WIDTH = 600;
const int WINDOW_HEIGHT = 600;

bool isDrawWireframe = false;
bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	ViewCenter = 3,
	ViewEye = 4,
	ViewUp = 5,
};

GLint iLocMVP;

vector<string> filenames; // .obj filename list

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form
};
vector <model> models;

struct camera
{
vector<model> models;
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

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};
ProjMode cur_proj_mode = Orthogonal;
TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix = Matrix4(
	0, 0, 0, 0,
	0, 0, 0, 0, 
	0, 0, 0, 0, 
	0, 0, 0, 0);
Matrix4 project_matrix = Matrix4(
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0);


typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	int materialId;
	int indexCount;
	GLuint m_texture;
} Shape;
Shape quad;
Shape m_shpae;
vector<Shape> m_shape_list;
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
		0, cos(val), -sin(val), 0,
		0, sin(val), cos(val), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), 0, sin(val), 0,
		0, 1, 0, 0,
		-sin(val), 0, cos(val), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), -sin(val), 0, 0,
		sin(val), cos(val), 0, 0,
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
		main_camera.center.z - main_camera.position.z};
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
	cur_proj_mode = Orthogonal;
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
	cur_proj_mode = Perspective;
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

void drawPlane()
{
	// EBO/IBO
	GLfloat vertices[18]{ 1.0, -0.9, -1.0,
		1.0, -0.9,  1.0,
		-1.0, -0.9, -1.0, // first triangle
		1.0, -0.9,  1.0,
		-1.0, -0.9,  1.0,
		-1.0, -0.9, -1.0 }; // second triangle

	GLfloat colors[18]{ 0.0,1.0,0.0,
		0.0,0.5,0.8,
		0.0,1.0,0.0, // first triangle
		0.0,0.5,0.8,
		0.0,0.5,0.8,
		0.0,1.0,0.0 }; // second triangle

	// [TODO] draw the plane with above vertices and color
	Matrix4 MVP;
	GLfloat mvp[16];
	// [TODO] multiply all the matrix
	MVP = project_matrix * view_matrix;
	// [TODO] row-major ---> column-major
	std::swap(MVP[1], MVP[4]);
	std::swap(MVP[2], MVP[8]);
	std::swap(MVP[3], MVP[12]);
	std::swap(MVP[6], MVP[9]);
	std::swap(MVP[7], MVP[13]);
	std::swap(MVP[11], MVP[14]);

	mvp[0] = 1;  mvp[4] = 0;   mvp[8] = 0;    mvp[12] = 0;
	mvp[1] = 0;  mvp[5] = 1;   mvp[9] = 0;    mvp[13] = 0;
	mvp[2] = 0;  mvp[6] = 0;   mvp[10] = 1;   mvp[14] = 0;
	mvp[3] = 0;  mvp[7] = 0;   mvp[11] = 0;   mvp[15] = 1;

	//please refer to LoadModels function
	//glGenVertexArrays..., glBindVertexArray...
	//glGenBuffers..., glBindBuffer..., glBufferData...
	for (int i = 0; i < 16; i++)
		mvp[i] = MVP[i];
	
	glGenVertexArrays(1, &quad.vao);
	glBindVertexArray(quad.vao);

	glGenBuffers(1, &quad.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, quad.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glGenBuffers(1, &quad.p_color);
	glBindBuffer(GL_ARRAY_BUFFER, quad.p_color);
	glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	quad.vertex_count = 6;
	
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(quad.vao); // tie vao
	glDrawArrays(GL_TRIANGLES, 0, quad.vertex_count);
	glBindVertexArray(0);

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

	Matrix4 MVP;
	GLfloat mvp[16];

	// [TODO] multiply all the matrix  
	MVP = project_matrix * view_matrix * T * R * S;
	// [TODO] row-major ---> column-major
	std::swap(MVP[1], MVP[4]);
	std::swap(MVP[2], MVP[8]);
	std::swap(MVP[3], MVP[12]);
	std::swap(MVP[6], MVP[9]);
	std::swap(MVP[7], MVP[13]);
	std::swap(MVP[11], MVP[14]);

	mvp[0] = 1;  mvp[4] = 0;   mvp[8] = 0;    mvp[12] = 0;
	mvp[1] = 0;  mvp[5] = 1;   mvp[9] = 0;    mvp[13] = 0;
	mvp[2] = 0;  mvp[6] = 0;   mvp[10] = 1;   mvp[14] = 0;
	mvp[3] = 0;  mvp[7] = 0;   mvp[11] = 0;   mvp[15] = 1;

	// use uniform to send mvp to vertex shader
	for (int i = 0; i < 16; i++)
		mvp[i] = MVP[i];
	
	// [TODO] draw 3D model in solid or in wireframe mode here, and draw plane
	if (isDrawWireframe == false) // draw 3D model in solid mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else // draw 3D model wireframe mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(m_shape_list[cur_idx].vao); // tie vao
	glDrawArrays(GL_TRIANGLES, 0, m_shape_list[cur_idx].vertex_count);	
	glBindVertexArray(0);

	if (isDrawWireframe == true) // draw plane in solid mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	drawPlane();
}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if (action != GLFW_RELEASE){ // avoid double updates
		switch (key) {
		case GLFW_KEY_ESCAPE:  // close the window
			exit(0);	
		case GLFW_KEY_W: // switch between solid and wireframe mode
			if (isDrawWireframe == false) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				isDrawWireframe = true;
			}
			else {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				isDrawWireframe = false;
			}
			break;
		case GLFW_KEY_Z: // switch the model
			cur_idx = (cur_idx + 1) % 5;
			break;
		case GLFW_KEY_X: // switch the model
			cur_idx -= 1;
			if (cur_idx < 0) {
				cur_idx += 5;
				cur_idx = cur_idx % 5;
			}
			break;
		case GLFW_KEY_O: // switch to Orthogonal projection
			cur_proj_mode = Orthogonal;
			setOrthogonal();
			break;
		case GLFW_KEY_P: // switch to NDC Perspective projection
			cur_proj_mode = Perspective;
			setPerspective();
			break;
		case GLFW_KEY_T: // switch to translation mode
			cur_trans_mode = GeoTranslation;
			break;
		case GLFW_KEY_S: // switch to scale mode
			cur_trans_mode = GeoScaling;
			break;
		case GLFW_KEY_R: // switch to rotation mode
			cur_trans_mode = GeoRotation;
			break;
		case GLFW_KEY_E: // switch to translate eye position mode
			cur_trans_mode = ViewEye;
			break;
		case GLFW_KEY_C: // switch to translate viewing center position mode
			cur_trans_mode = ViewCenter;
			break;
		case GLFW_KEY_U: // switch to translate camera up vector position mode
			cur_trans_mode = ViewUp;
			break;
		case GLFW_KEY_I: // print  information
			Matrix4 V, P, T, R, S;
			V = view_matrix;
			P = project_matrix;
			T = translate(models[cur_idx].position); // translation matrix
			R = rotate(models[cur_idx].rotation); // rotation matrix
			S = scaling(models[cur_idx].scale); // scaling matrix

			std::cout << "Matrix Value:" << std::endl;
			std::cout << "View Matrix:" << std::endl;
			std::cout << "(" << V[0] << ", " << V[1] << ", " << V[2] << ", " << V[3] << ")" << std::endl;
			std::cout << "(" << V[4] << ", " << V[5] << ", " << V[6] << ", " << V[7] << ")" << std::endl;
			std::cout << "(" << V[8] << ", " << V[9] << ", " << V[10] << ", " << V[11] << ")" << std::endl;
			std::cout << "(" << V[12] << ", " << V[13] << ", " << V[14] << ", " << V[15] << ")" << std::endl;
			
			std::cout << "Project Matrix:" << std::endl;
			std::cout << "(" << P[0] << ", " << P[1] << ", " << P[2] << ", " << P[3] << ")" << std::endl;
			std::cout << "(" << P[4] << ", " << P[5] << ", " << P[6] << ", " << P[7] << ")" << std::endl;
			std::cout << "(" << P[8] << ", " << P[9] << ", " << P[10] << ", " << P[11] << ")" << std::endl;
			std::cout << "(" << P[12] << ", " << P[13] << ", " << P[14] << ", " << P[15] << ")" << std::endl;
			
			std::cout << "Translation Matrix:" << std::endl;
			std::cout << "(" << T[0] << ", " << T[1] << ", " << T[2] << ", " << T[3] << ")" << std::endl;
			std::cout << "(" << T[4] << ", " << T[5] << ", " << T[6] << ", " << T[7] << ")" << std::endl;
			std::cout << "(" << T[8] << ", " << T[9] << ", " << T[10] << ", " << T[11] << ")" << std::endl;
			std::cout << "(" << T[12] << ", " << T[13] << ", " << T[14] << ", " << T[15] << ")" << std::endl;
			
			std::cout << "Rotation Matrix" << std::endl;
			std::cout << "(" << R[0] << ", " << R[1] << ", " << R[2] << ", " << R[3] << ")" << std::endl;
			std::cout << "(" << R[4] << ", " << R[5] << ", " << R[6] << ", " << R[7] << ")" << std::endl;
			std::cout << "(" << R[8] << ", " << R[9] << ", " << R[10] << ", " << R[11] << ")" << std::endl;
			std::cout << "(" << R[12] << ", " << R[13] << ", " << R[14] << ", " << R[15] << ")" << std::endl;
			
			std::cout << "Scaling Matrix:" << std::endl;
			std::cout << "(" << S[0] << ", " << S[1] << ", " << S[2] << ", " << S[3] << ")" << std::endl;
			std::cout << "(" << S[4] << ", " << S[5] << ", " << S[6] << ", " << S[7] << ")" << std::endl;
			std::cout << "(" << S[8] << ", " << S[9] << ", " << S[10] << ", " << S[11] << ")" << std::endl;
			std::cout << "(" << S[12] << ", " << S[13] << ", " << S[14] << ", " << S[15] << ")" << std::endl;
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
		models[cur_idx].rotation.z += yoffset * 0.1f;
	}
	else if (cur_trans_mode == GeoScaling){ // Scaling mode
		// if scroll up, then object will be bigger.
		// if scroll down, then object will be smaller.		
		models[cur_idx].scale.z += yoffset * 0.01f;
		if (models[cur_idx].scale.z <= 0)
			models[cur_idx].scale.z = 0;
	} 
	else if (cur_trans_mode == ViewEye) { // translation eye position mode
		// if scroll up, then object and quad will be more close.
		// if scroll down, then object and quad will be more farther.
		if (yoffset > 0)
			main_camera.position.z -= 0.025f;
		else
			main_camera.position.z += 0.025f;
		setViewingMatrix(); // update the viewing matrix
		std::cout << "Camera Viewing Position = ( " << main_camera.position.x << ", " << main_camera.position.y << ", " << main_camera.position.z << " )" << std::endl;
	} 
	else if (cur_trans_mode == ViewCenter) { // translate viewing center position mode
		// if scroll up, then object and quad will vanish at main_camera.center = (0, 0, 0).
		// if scroll down, then object and quad  will not change.
		if (yoffset > 0)
			main_camera.center.z += 0.025f;
		else
			main_camera.center.z -= 0.025f;	
		setViewingMatrix(); // update the viewing matrix
		std::cout << "Camera Viewing Direction = ( " << main_camera.center.x - main_camera.position.x << ", " << main_camera.center.y - main_camera.position.y << ", " << main_camera.center.z - main_camera.position.z << " )" << std::endl;
	}
	else if (cur_trans_mode == ViewUp) { // translate camera up vector position mode
		// if scroll up, then object and quad will not change.
		// if scroll down, then object and quad will not change.		
		if (yoffset > 0)
			main_camera.up_vector.z += 0.025f;
		else
			main_camera.up_vector.z -= 0.025f;
		setViewingMatrix(); // update the viewing matrix
		std::cout << "Camera Up Vector = ( " << main_camera.up_vector.x << ", " << main_camera.up_vector.y << ", " << main_camera.up_vector.z << " )" << std::endl;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] Call back function for mouse
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] Call back function for cursor position
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
					models[cur_idx].rotation.y -= 0.05f;
					models[cur_idx].rotation.x += 0.05f;
				}
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
					models[cur_idx].rotation.y -= 0.05f;
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
					models[cur_idx].rotation.y -= 0.05f;
					models[cur_idx].rotation.x += 0.05f;
				}
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
					models[cur_idx].rotation.x -= 0.05f;
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
					models[cur_idx].rotation.x += 0.05f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
					models[cur_idx].rotation.y += 0.05f;
					models[cur_idx].rotation.x -= 0.05f;
				}
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
					models[cur_idx].rotation.y += 0.05f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
					models[cur_idx].rotation.y += 0.05f;
					models[cur_idx].rotation.x += 0.05f;
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
			else if (cur_trans_mode == ViewEye) { // translate eye position mode
				// if cursor shift right, then object and quad will counterclockwise right
				// if cursor shift left, then object and quad will clockwise left.
				// if cursor shiht up, then object and quad will lean back.
				// if cursor shit down, then object and quad will fall back.

				if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
					main_camera.position.x -= 0.01f;
					main_camera.position.y -= 0.01f;
				}
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
					main_camera.position.x -= 0.01f;
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
					main_camera.position.x -= 0.01f;
					main_camera.position.y += 0.01f;
				}
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
					main_camera.position.y -= 0.01f;
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
					main_camera.position.y += 0.01f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
					main_camera.position.x += 0.01f;
					main_camera.position.y -= 0.01f;
				}
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
					main_camera.position.x += 0.01f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
					main_camera.position.x += 0.01f;
					main_camera.position.y += 0.01f;
				}
				starting_press_x = xpos;
				starting_press_y = ypos;
				setViewingMatrix(); // update the viewing matrix
				std::cout << "Camera Viewing Position = ( " << main_camera.position.x << ", " << main_camera.position.y << ", " << main_camera.position.z << " )" << std::endl;
			}
			else if (cur_trans_mode == ViewCenter) { // translate viewing center position mode
				// if cursor shift right, then object and quad will be translated right.
				// if cursor shift left, then object and quad will be translated left.
				// if cursor shiht up, then object and quad will be translated up.
				// if cursor shit down, then object and quad will be translated down.

				if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
					main_camera.center.x -= 0.03f;
					main_camera.center.y += 0.03f;
				}
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
					main_camera.center.x -= 0.03f;
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
					main_camera.center.x -= 0.03f;
					main_camera.center.y -= 0.03f;
				}
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
					main_camera.center.y += 0.03f;
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
					main_camera.center.y -= 0.03f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
					main_camera.center.x += 0.03f;
					main_camera.center.y += 0.03f;
				}
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
					main_camera.center.x += 0.03f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
					main_camera.center.x += 0.03f;
					main_camera.center.y -= 0.03f;
				}
				starting_press_x = xpos;
				starting_press_y = ypos;
				setViewingMatrix(); // update the viewing matrix
				std::cout << "Camera Viewing Direction = ( " << main_camera.center.x - main_camera.position.x << ", " << main_camera.center.y - main_camera.position.y << ", " << main_camera.center.z - main_camera.position.z << " )" << std::endl;
			}
			else if (cur_trans_mode == ViewUp) { // translate camera up vector position mode
				// if cursor shift right, then object and quad will clockwise.
				// if cursor shift left, then object and quad will counterclockwise.
				// if cursor shiht up, then object and quad will not change.
				// if cursor shit down, then object and quad will not change.
				if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) > 0) {
					main_camera.up_vector.x += 0.03f;
					main_camera.up_vector.y += 0.03f;
				}
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) == 0)
					main_camera.up_vector.x += 0.03f;
				else if ((xpos - starting_press_x) > 0 && (ypos - starting_press_y) < 0) {
					main_camera.up_vector.x += 0.03f;
					main_camera.up_vector.y -= 0.03f;
				}
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) > 0)
					main_camera.up_vector.y += 0.03f;
				else if ((xpos - starting_press_x) == 0 && (ypos - starting_press_y) < 0)
					main_camera.up_vector.y -= 0.03f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) > 0) {
					main_camera.up_vector.x -= 0.03f;
					main_camera.up_vector.y += 0.03f;
				}
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) == 0)
					main_camera.up_vector.x -= 0.03f;
				else if ((xpos - starting_press_x) < 0 && (ypos - starting_press_y) < 0) {
					main_camera.up_vector.x -= 0.03f;
					main_camera.up_vector.y -= 0.03f;
				}
				starting_press_x = xpos;
				starting_press_y = ypos;
				setViewingMatrix(); // update the viewing matrix
				std::cout << "Camera Up Vector = ( " << main_camera.up_vector.x << ", " << main_camera.up_vector.y << ", " << main_camera.up_vector.z << " )" << std::endl;
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

	iLocMVP = glGetUniformLocation(p, "mvp");

	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, tinyobj::shape_t* shape)
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
		attrib->vertices.at(i) = attrib->vertices.at(i)/ scale;
	}
	size_t index_offset = 0;
	vertices.reserve(shape->mesh.num_face_vertices.size() * 3);
	colors.reserve(shape->mesh.num_face_vertices.size() * 3);
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
		}
		index_offset += fv;
	}
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;

	string err;
	string warn;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Maerial size %d\n", shapes.size(), materials.size());
	
	normalization(&attrib, vertices, colors, &shapes[0]);

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

	m_shape_list.push_back(tmp_shape);
	model tmp_model;
	models.push_back(tmp_model);


	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	shapes.clear();
	materials.clear();
}

void initParameter()
{
	
	proj.left = -1; // X_min = proj.left
	proj.right = 1; // X_max = proj.right
	proj.top = 1; // Y_max = proj.top
	proj.bottom = -1; // Y_min = proj.bottom
	proj.nearClip = 0.001; // Z_near = - nearClip, distance from eyes to near plane.
	proj.farClip = 100.0; // Z_far = - farClip, distance from eyes to far plane.
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f); // viewpoint
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f); // center position
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f); // up direction

	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);

	vector<string> model_list{ "../ColorModels/bunny5KC.obj", "../ColorModels/dragon10KC.obj", "../ColorModels/lucy25KC.obj", "../ColorModels/teapot4KC.obj", "../ColorModels/dolphinC.obj"};
	// [TODO] Load five model at here
	LoadModels(model_list[cur_idx % 5]);
	LoadModels(model_list[(cur_idx + 1) % 5]);
	LoadModels(model_list[(cur_idx + 2) % 5]);
	LoadModels(model_list[(cur_idx + 3) % 5]);
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
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "109062671 HW1", NULL, NULL);
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
	glPrintContextInfo(false);
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
