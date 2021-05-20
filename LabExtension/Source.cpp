#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <glm/glm.hpp>
#include "SDL_helper.h"
#include "TestModel.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;
using glm::vec3;
using glm::ivec2;
using glm::mat3;
using glm::vec2;

// ----------------------------------------------------------------------------
// GLOBAL VARIABLES

const int SCREEN_WIDTH = 500;
const int SCREEN_HEIGHT = 500;
Window screen = Window::Create("Project", SCREEN_WIDTH, SCREEN_HEIGHT);
int t;
vector<Triangle> triangles;
vec3 cameraPos(0, 0, -3.001);
const int focalLength = SCREEN_HEIGHT;
mat3 R;
float yaw = 0;
// Rasterization Part
vec3 currentColor;
float depthBuffer[SCREEN_HEIGHT][SCREEN_WIDTH];
vec3 lightPos(0, -0.5, -0.7);
vec3 lightPower = 1.1f * vec3(1, 1, 1);// The light power is enlarged for 10 times
vec3 indirectLightPowerPerArea = 0.5f * vec3(1, 1, 1);
vec3 currentNormal;
vec3 currentReflectance;
// Ray Tracing Part
vec3 lightColor = 14.f * vec3(1, 1, 1);
vec3 indirectLight = 0.5f * vec3(1, 1, 1);
// Edge Detection
uint8_t grayMap[SCREEN_HEIGHT][SCREEN_HEIGHT];


// ----------------------------------------------------------------------------
// CLASS
struct Pixel
{
	int x;
	int y;
	float zinv;
	//vec3 illumination;
	vec3 pos3d;
};

struct Vertex
{
	vec3 position;
	//vec3 normal;
	//vec3 reflectance;
};

struct Intersection
{
	vec3 position;
	float distance;
	int triangleIndex;
};

// ----------------------------------------------------------------------------
// FUNCTIONS

void Update(float dt);
void Draw_Rasterization();
void Draw_RayTracing();
void VertexShader(const vec3& v, ivec2& p);
void Interpolate(ivec2 a, ivec2 b, vector<ivec2>& result);
void DrawLineSDL(ivec2 a, ivec2 b, vec3 color);
void DrawPolygonEdges(const vector<vec3>& vertices);
void Rotation(float raw);
void DrawPolygon(const vector<Vertex>& vertices);
void DrawPolygonRows(const vector<Pixel>& leftPixels, const vector<Pixel>& rightPixels);
void ComputePolygonRows(const vector<Pixel>& vertexPixels, vector<Pixel>& leftPixels, vector<Pixel>& rightPixels);
void Interpolate(Pixel a, Pixel b, vector<Pixel>& result); // Overloading
void VertexShader(const vec3& v, Pixel& p); // Overloading
void VertexShader(const Vertex& v, Pixel& p); // Overloading
void PixelShader(const Pixel& p);
bool ClosestIntersection(
	vec3 start,
	vec3 dir,
	const vector<Triangle>& triangles,
	Intersection& closestIntersection
);
vec3 CelStyleDirectLight(const Intersection& i);
float MyDet(vec3 a, vec3 b, vec3 c);
//Cel Shader
void CelShader(const Pixel& p);
float AngleTranslation(int tone_size, float product);
void DrawEdgeCanny();


int main(int argc, char* argv[])
{	
	LoadTestModel(triangles);
	bool running = true;
	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				running = false;
			}
		}

		Draw_Rasterization();
		//Draw_RayTracing();
		DrawEdgeCanny();
		
		// NOTE: The pixels are not shown on the screen 
		// until we update the window with this method.
		screen.update();
	}

	screen.screenshot();

	Window::Destroy(&screen);
	return 0;
}

void Update(float dt)
{
	std::cout << "Render time: " << dt << " s." << std::endl;
	const Uint8* keystate = SDL_GetKeyboardState(nullptr);

	if (keystate[SDL_SCANCODE_UP]) {
		cameraPos.y = cameraPos.y + 10 / focalLength;
	}
	if (keystate[SDL_SCANCODE_DOWN]) {
		cameraPos.y = cameraPos.y - 10 / focalLength;
	}
	if (keystate[SDL_SCANCODE_LEFT]) {
		yaw = yaw - 0.5 * 0.1745; //About 5 degree
		Rotation(yaw);
		cameraPos = R * cameraPos;
	}
	if (keystate[SDL_SCANCODE_RIGHT]) {
		yaw = yaw + 0.5 * 0.1745; //About 5 degree
		Rotation(yaw);
		cameraPos = R * cameraPos;
	}
	if (keystate[SDL_SCANCODE_W]) {
		lightPos.z = lightPos.z + 10 / focalLength;
	}
	if (keystate[SDL_SCANCODE_S]) {
		lightPos.z = lightPos.z - 10 / focalLength;
	}
	if (keystate[SDL_SCANCODE_A]) {
		lightPos.x = lightPos.x + 10 / focalLength;
	}
	if (keystate[SDL_SCANCODE_D]) {
		lightPos.x = lightPos.x - 10 / focalLength;
	}
	if (keystate[SDL_SCANCODE_Q]) {
		lightPos.y = lightPos.y + 10 / focalLength;
	}
	if (keystate[SDL_SCANCODE_E]) {
		lightPos.y = lightPos.y - 10 / focalLength;
	}
}


// ----------------------------------------------------------------------------
// Rasterization Part

void Draw_Rasterization()
{
	for (int y = 0; y < SCREEN_HEIGHT; ++y)
		for (int x = 0; x < SCREEN_WIDTH; ++x)
			depthBuffer[y][x] = 0;

	screen.fill(BLACK);

	for (int i = 0; i < triangles.size(); ++i)
	{
		currentColor = triangles[i].color;
		vector<Vertex> vertices(3);
		vertices[0].position = triangles[i].v0;
		vertices[1].position = triangles[i].v1;
		vertices[2].position = triangles[i].v2;

		currentNormal = triangles[i].normal;
		currentReflectance = 10.f * vec3(1, 1, 1);

		DrawPolygon(vertices);
	}
}

void VertexShader(const vec3& v, ivec2& p) {
	p.x = (focalLength * v.x) / (v.z - cameraPos.z) + SCREEN_WIDTH / 2;
	p.y = (focalLength * v.y) / (v.z - cameraPos.z) + SCREEN_WIDTH / 2;
	return;
}

void VertexShader(const vec3& v, Pixel& p)
{
	vec3 pos = R * (v - cameraPos);
	p.zinv = 1 / pos.z;
	p.x = int(focalLength * pos.x * p.zinv) + SCREEN_WIDTH / 2;
	p.y = int(focalLength * pos.y * p.zinv) + SCREEN_HEIGHT / 2;
}

void VertexShader(const Vertex& v, Pixel& p)
{
	vec3 pos = R * (v.position - cameraPos);
	p.zinv = 1 / pos.z;
	p.x = int(focalLength * pos.x * p.zinv) + SCREEN_WIDTH / 2;
	p.y = int(focalLength * pos.y * p.zinv) + SCREEN_HEIGHT / 2;
	p.pos3d = v.position;
}

void Interpolate(ivec2 a, ivec2 b, vector<ivec2>& result)
{
	int N = result.size();
	glm::vec2 step = glm::vec2(b - a) / float(max(N - 1, 1));
	glm::vec2 current(a);
	for (int i = 0; i < N; ++i)
	{
		result[i] = current;
		current += step;
	}
}

void Interpolate(Pixel a, Pixel b, vector<Pixel>& result)
{
	int N = result.size();
	float stepx = float(b.x - a.x) / float(glm::max(N - 1, 1));
	float stepy = float(b.y - a.y) / float(glm::max(N - 1, 1));
	float stepz = float(b.zinv - a.zinv) / float(glm::max(N - 1, 1));
	Pixel current = a;

	vec3 za = a.pos3d * a.zinv;
	vec3 zb = b.pos3d * b.zinv;
	vec3 stepp = vec3(zb - za) / float(max(N - 1, 1));
	vec3 current_p(za);

	for (int i = 0; i < N; ++i)
	{
		result[i] = current;
		current.x = a.x + stepx * i;
		current.y = a.y + stepy * i;
		current.zinv = a.zinv + stepz * i;
		result[i].pos3d = current_p / result[i].zinv;
		current_p += stepp;
	}
}

void DrawLineSDL(ivec2 a, ivec2 b, vec3 color) {
	ivec2 delta = glm::abs(a - b);
	int pixels = glm::max(delta.x, delta.y) + 1;
	vector<ivec2> line(pixels);
	Interpolate(a, b, line);
	for (int i = 0; i < line.size(); i++) {
		screen.set_pixel(line[i].x, line[i].y, color);
	}
}

void DrawPolygonEdges(const vector<vec3>& vertices)
{
	int V = vertices.size();
	// Transform each vertex from 3D world position to 2D image position:
	vector<ivec2> projectedVertices(V);
	for (int i = 0; i < V; ++i)
	{
		VertexShader(vertices[i], projectedVertices[i]);
	}
	// Loop over all vertices and draw the edge from it to the next vertex:
	for (int i = 0; i < V; ++i)
	{
		int j = (i + 1) % V; // The next vertex
		vec3 color(1, 1, 1);
		DrawLineSDL(projectedVertices[i], projectedVertices[j], color);
	}
}

void ComputePolygonRows(const vector<Pixel>& vertexPixels, vector<Pixel>& leftPixels, vector<Pixel>& rightPixels)
{
	// 1. Find max and min y-value of the polygon and compute the number of rows it occupies.
	int temp_min = +numeric_limits<int>::max();
	int temp_max = -numeric_limits<int>::max();
	for (int i = 0; i < vertexPixels.size(); i++) {
		if (temp_min > vertexPixels[i].y) {
			temp_min = vertexPixels[i].y;
		}
		if (temp_max < vertexPixels[i].y) {
			temp_max = vertexPixels[i].y;
		}
	}
	int ROWS = temp_max - temp_min + 1;

	// 2. Resize leftPixels and rightPixels so that they have an element for each row.
	vector<Pixel> left(ROWS);
	vector<Pixel> right(ROWS);
	leftPixels = left;
	rightPixels = right;


	// 3. Initialize the x-coordinates in leftPixels to some really large value and 
	// the x-coordinates in rightPixels to some really small value.
	for (int i = 0; i < ROWS; i++)
	{
		leftPixels[i].x = +numeric_limits<int>::max();
		rightPixels[i].x = -numeric_limits<int>::max();
	}

	// 4. Loop through all edges of the polygon and use linear interpolation to find the x-coordinate for
	// each row it occupies. Update the corresponding values in rightPixels and leftPixels.
	int v_size = vertexPixels.size();
	for (int i = 0; i < v_size; ++i)
	{
		int j = (i + 1) % v_size; // The next vertex
		//ivec2 delta = glm::abs(vertexPixels[i] - vertexPixels[j]);
		//int pixels = delta.y + 1; // BUG!!! (No max between delta.x and delta.y)
		int pixels = glm::abs(vertexPixels[i].y - vertexPixels[j].y) + 1;
		vector<Pixel> line(pixels);
		Interpolate(vertexPixels[i], vertexPixels[j], line);
		for (int k = 0; k < line.size(); k++) {
			int index = line[k].y - temp_min;
			if (leftPixels[index].x > line[k].x) {
				leftPixels[index] = line[k];
			}
			if (rightPixels[index].x < line[k].x) {
				rightPixels[index] = line[k];
			}
		}
	}

}

void DrawPolygonRows(const vector<Pixel>& leftPixels, const vector<Pixel>& rightPixels) {
	for (int i = 0; i < leftPixels.size(); i++) {
		int pixels = glm::abs(leftPixels[i].x - rightPixels[i].x) + 1;
		vector<Pixel> line(pixels);
		Interpolate(leftPixels[i], rightPixels[i], line);
		for (int j = 0; j < line.size(); j++) {
			//PixelShader(line[j]);
			CelShader(line[j]);
		}
	}
}

void DrawPolygon(const vector<Vertex>& vertices)
{
	int V = vertices.size();
	vector<Pixel> vertexPixels(V);
	for (int i = 0; i < V; ++i)
		VertexShader(vertices[i], vertexPixels[i]);
	vector<Pixel> leftPixels;
	vector<Pixel> rightPixels;
	ComputePolygonRows(vertexPixels, leftPixels, rightPixels);
	DrawPolygonRows(leftPixels, rightPixels);
}

void PixelShader(const Pixel& p)
{
	int x = p.x;
	int y = p.y;
	if (p.zinv >= depthBuffer[y][x])
	{
		float r_2 = (lightPos.x - p.pos3d.x) * (lightPos.x - p.pos3d.x) +
			(lightPos.y - p.pos3d.y) * (lightPos.y - p.pos3d.y) +
			(lightPos.z - p.pos3d.z) * (lightPos.z - p.pos3d.z);
		float S = 4 * 3.1415926535 * r_2;
		vec3 ray = glm::normalize(lightPos - p.pos3d);
		float dot_p = ray.x * currentNormal.x + ray.y * currentNormal.y + ray.z * currentNormal.z;
		vec3 D = (lightPower * max(dot_p, 0.f)) / S;
		vec3 R = currentReflectance * D + indirectLightPowerPerArea;
		vec3 color = R * currentColor;
		depthBuffer[y][x] = p.zinv;
		screen.set_pixel(x, y, color);
		// Depth Figure
		/*vec3 white(1, 1, 1);
		float scale = (3.3 * depthBuffer[y][x] - 0.66);
		PutPixelSDL(screen, x, y, scale * white);*/
	}
}

void CelShader(const Pixel& p)
{
	int x = p.x;
	int y = p.y;
	if (p.zinv >= depthBuffer[y][x])
	{
		float r_2 = (lightPos.x - p.pos3d.x) * (lightPos.x - p.pos3d.x) +
			(lightPos.y - p.pos3d.y) * (lightPos.y - p.pos3d.y) +
			(lightPos.z - p.pos3d.z) * (lightPos.z - p.pos3d.z);
		float S = 4 * 3.1415926535 * r_2;
		vec3 ray = glm::normalize(lightPos - p.pos3d);
		
		float dot_p = ray.x * currentNormal.x + ray.y * currentNormal.y + ray.z * currentNormal.z;
		//vec3 D = (lightPower * max(dot_p, 0.f)) / S;
		int tone_size = 4;
		dot_p = AngleTranslation(tone_size, dot_p);
		
		vec3 D = 0.075f * vec3(1, 1, 1)* max(dot_p, 0.f);
		vec3 R = currentReflectance * D + indirectLightPowerPerArea;
		float product = R.x;
		//product = AngleTranslation(3, product);
		
		vec3 color = product * currentColor;
		depthBuffer[y][x] = p.zinv;
		
		// Canny Edge Detection Color Parameter
		float temp = 0.1140 * currentColor.b + 0.5870 * currentColor.g + 0.2989 * currentColor.r;
		grayMap[y][x] = round((temp + 1) * 255 / 2);

		// Draw the pixel on the screen
		screen.set_pixel(x, y, color);
	}
}

float AngleTranslation(int tone_size, float product) {
	switch (tone_size) {
	case 0:
		return product;
	case 1:
		return 1;
	case 2:
		if (product < 0.5) {
			return 0.3;
		}
		else {
			return 0.8;
		}
		break;
	case 3:
		if (product < 0.4) {
			return 0.2;
		}
		else if(product < 0.7) {
			return 0.55;
		}
		else {
			return 0.8;
		}
		break;
	case 4:
		if (product < 0.3) {
			return 0.2;
		}
		else if (product < 0.6) {
			return 0.45;
		}
		else if (product < 0.8) {
			return 0.7;
		}
		else {
			return 0.9;
		}
		break;
	default:
		break;
	}
	return 0;
}


// ----------------------------------------------------------------------------
// Ray Tracing Part
void Draw_RayTracing()
{
	Intersection closetIntersection;
	for (int y = 0; y < SCREEN_HEIGHT; ++y)
	{
		for (int x = 0; x < SCREEN_WIDTH; ++x)
		{
			vec3 direction = vec3(x - SCREEN_WIDTH / 2, y - SCREEN_HEIGHT / 2, focalLength);
			bool flag = ClosestIntersection(cameraPos, direction, triangles, closetIntersection);
			if (flag) {
				vec3 color = triangles[closetIntersection.triangleIndex].color;

				// Canny Edge Detection Color Parameter
				float temp = 0.1140 * color.b + 0.5870 * color.g + 0.2989 * color.r;
				grayMap[y][x] = round((temp + 1) * 255 / 2);

				vec3 D = CelStyleDirectLight(closetIntersection);
				vec3 N = indirectLight;
				color = color * (D + N);
				color.x = min(color.x, 1.f);
				color.y = min(color.y, 1.f);
				color.z = min(color.z, 1.f);

				// Draw the pixel on the screen
				screen.set_pixel(x, y, color);
			}
			else
			{
				vec3 black(0, 0, 0);
				screen.set_pixel(x, y, black);
			}
		}
	}
}

bool ClosestIntersection(vec3 start, vec3 dir, const vector<Triangle>& triangles, Intersection& closestIntersection)
{
	bool result = false;
	const float m = std::numeric_limits<float>::max();
	closestIntersection.distance = m;
	// For each triangle in the model
	for (int i = 0; i < triangles.size(); i++)
	{
		vec3 v0 = triangles[i].v0;
		vec3 v1 = triangles[i].v1;
		vec3 v2 = triangles[i].v2;

		//find the intersection point between the ray of camera to that point and the plane of that triangle
		vec3 e1 = v1 - v0;
		vec3 e2 = v2 - v0;
		vec3 b = start - v0;

		// Cramar's Rule
		float det_A = MyDet(-dir, e1, e2);
		if (det_A == 0) {
			continue;
		}
		else
		{
			float det_T = MyDet(b, e1, e2);
			float tt = det_T / det_A;
			if (tt <= 0) {
				continue;
			}
			float det_U = MyDet(-dir, b, e2);
			float uu = det_U / det_A;
			if (uu < 0) {
				continue;
			}
			float det_V = MyDet(-dir, e1, b);
			float vv = det_V / det_A;
			if (vv < 0) {
				continue;
			}
			if (uu + vv > 1) {
				continue;
			}
			result = true;
			if (closestIntersection.distance > tt) {
				closestIntersection.distance = tt;
				closestIntersection.position = start + tt * dir;
				closestIntersection.triangleIndex = i;
			}
		}
	}
	return result;
}

float MyDet(vec3 a, vec3 b, vec3 c) {
	float ans = 0;
	ans = a.x * (b.y * c.z - b.z * c.y) - a.y * (b.x * c.z - b.z * c.x) + a.z * (b.x * c.y - b.y * c.x);
	return ans;
}


vec3 CelStyleDirectLight(const Intersection& i) {
	triangles[i.triangleIndex].ComputeNormal();
	vec3 N = triangles[i.triangleIndex].normal;
	vec3 R = lightPos - i.position;
	float N_len_2 = N.x * N.x + N.y * N.y + N.z * N.z;
	float R_len_2 = R.x * R.x + R.y * R.y + R.z * R.z;
	float S = 4 * 3.1415926535 * R_len_2;
	vec3 B = lightColor * 0.1f;
	float RN = (N.x * R.x + N.y * R.y + N.z * R.z) / (N_len_2 * R_len_2);
	// Cel Shading the Pixel
	int tone_size = 3;
	RN = AngleTranslation(tone_size, RN);
	vec3 D(0, 0, 0);
	if (RN > 0) {
		D = B * RN;
	}
	Intersection shadowIntersection;
	ClosestIntersection(lightPos, -R, triangles, shadowIntersection);
	vec3 shadowPath = shadowIntersection.position - lightPos;
	float shadowDistance = shadowPath.x * shadowPath.x + shadowPath.y * shadowPath.y + shadowPath.z * shadowPath.z;
	if (shadowDistance + 0.0001 < R_len_2) {
		// Whether to calculate the shadow independently
		//D = B * AngleTranslation(tone_size, 0) * vec3(1, 1, 1);
		D = vec3(0, 0, 0);
	}
	return D;
}

// Else
void Rotation(float yaw) {
	R = mat3(cos(yaw), 0, sin(yaw), 0, 1, 0, -sin(yaw), 0, cos(yaw));
}


// ----------------------------------------------------------------------------
// Edge Detection Canny
void DrawEdgeCanny() {
	
	// Edge detection using the depth buffer (depth information)
	/*uint8_t scale[SCREEN_HEIGHT][SCREEN_WIDTH]; //int is not acceptable here
	for (int x = 0; x < SCREEN_WIDTH; x++) {
		for (int y = 0; y < SCREEN_HEIGHT; y++) {
			float temp = (3.3 * depthBuffer[y][x] - 0.66);
			scale[y][x] = round((temp + 1) * 255 / 2);
		}
	}
	Mat inputMap(SCREEN_HEIGHT, SCREEN_WIDTH, CV_8U, scale);
	Mat detect(SCREEN_HEIGHT, SCREEN_WIDTH, CV_8U, scale);
	Canny(inputMap, detect, 30, 90);*/
	
	// Edge detection using grayscale color conver from averaging RGB
	Mat inputMap(SCREEN_HEIGHT, SCREEN_WIDTH, CV_8U, grayMap);
	Mat detect(SCREEN_HEIGHT, SCREEN_WIDTH, CV_8U, grayMap);
	//Canny(inputMap, detect, 1, 3); // For Rasterization
	Canny(inputMap, detect, 10, 30); // For Ray Tracing

	for (int y = 0; y < detect.rows; y++) {
		for (int x = 0; x < detect.cols; x++) {
			if (detect.at<uint8_t>(y, x) != 0) {
				screen.set_pixel(x, y, vec3(0, 0, 0));
				if (x < 499 && y < 499) {
					screen.set_pixel(x + 1, y, vec3(0, 0, 0));
					screen.set_pixel(x, y + 1, vec3(0, 0, 0));
				}
			}
		}
	}
	//imshow("Original", inputMap);
	//imshow("Result", detect
}

// ----------------------------------------------------------------------------
// Edge Detection Triangle

