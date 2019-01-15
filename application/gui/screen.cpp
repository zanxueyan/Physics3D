#include "screen.h"

#include "shader.h"
#include "indexedMesh.h"
#include "arrayMesh.h"
#include "vectorMesh.h"
#include "picker.h"

#include "../debug.h"
#include "../standardInputHandler.h"
#include "../resourceManager.h"

#include "../../util/log.h"

#include "../engine/math/vec2.h"
#include "../engine/math/mat4.h"
#include "../engine/math/mathUtil.h"
#include "../engine/geometry/shape.h"
#include "../engine/geometry/shape.h"
#include "../engine/geometry/boundingBox.h"
#include "../engine/debug.h"

#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <math.h>
#include <map>

World* curWorld = NULL;

bool initGLFW() {
	/* Initialize the library */
	if (!glfwInit()) {
		Log::error("GLFW Failed to initialize!");
		return false;
	}

	return true;
}

bool initGLEW() {
	/* Init GLEW after creating a valid rendering context */
	if (glewInit() != GLEW_OK) {
		glfwTerminate();

		Log::error("GLEW Failed to initialize!");

		return false;
	}

	return true;
}

void terminateGL() {
	glfwTerminate();
}

Screen::Screen(int width, int height, World* world) {
	setWorld(world);

	/* Create a windowed mode window and its OpenGL context */
	this->window = glfwCreateWindow(width, height, "Physics3D", NULL, NULL);
	if (!this->window) {
		glfwTerminate();
		exit(-1);
	}

	/* Make the window's context current */
	makeCurrent();

	Log::info("OpenGL vendor: (%s)", glGetString(GL_VENDOR));
	Log::info("OpenGL renderer: (%s)", glGetString(GL_RENDERER));
	Log::info("OpenGL version: (%s)", glGetString(GL_VERSION));
	Log::info("OpenGL shader version: (%s)", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

IndexedMesh* boxMesh = nullptr;
VectorMesh* vectorMesh = nullptr;
ArrayMesh* originMesh = nullptr;
IndexedMesh* transMesh = nullptr;

Shader basicShader;
Shader vectorShader;
Shader originShader;
Shader outlineShader;

BoundingBox* box = nullptr;
StandardInputHandler* handler = nullptr;

void Screen::init() {
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	screenSize = Vec2(width, height);

	ShaderSource basicShaderSource = parseShader((std::istream&) std::istringstream(getResourceAsString(BASIC_SHADER)), "basic.shader");
	ShaderSource vectorShaderSource = parseShader((std::istream&) std::istringstream(getResourceAsString(VECTOR_SHADER)), "vector.shader");
	ShaderSource originShaderSource = parseShader((std::istream&) std::istringstream(getResourceAsString(ORIGIN_SHADER)), "origin.shader");
	ShaderSource outlineShaderSource = parseShader((std::istream&) std::istringstream(getResourceAsString(OUTLINE_SHADER)), "outline.shader");

	basicShader = Shader(basicShaderSource);
	basicShader.createUniform("modelMatrix");
	basicShader.createUniform("viewMatrix");
	basicShader.createUniform("color");
	basicShader.createUniform("projectionMatrix");
	basicShader.createUniform("viewPosition");

	vectorShader = Shader(vectorShaderSource);
	vectorShader.createUniform("viewMatrix");
	vectorShader.createUniform("projectionMatrix");
	vectorShader.createUniform("viewPosition");

	originShader = Shader(originShaderSource);
	originShader.createUniform("viewMatrix");
	originShader.createUniform("viewPosition");
	originShader.createUniform("projectionMatrix");
	originShader.createUniform("orthoMatrix");
	originShader.createUniform("rotatedViewMatrix");

	outlineShader = Shader(outlineShaderSource);
	outlineShader.createUniform("modelMatrix");
	outlineShader.createUniform("viewMatrix");
	outlineShader.createUniform("color");
	outlineShader.createUniform("projectionMatrix");
	outlineShader.createUniform("viewPosition");

	camera.setPosition(Vec3(1, 1, -2));
	camera.setRotation(Vec3(0, 3.1415, 0.0));

	handler = new StandardInputHandler(window, this);

	box = new BoundingBox{-0.5, -0.5, -0.5, 0.5, 0.5, 0.5};
	Shape shape = box->toShape(new Vec3[8]);// .rotated(fromEulerAngles(0.5, 0.1, 0.2), new Vec3[8]);
	boxMesh = new IndexedMesh(shape);
	Shape trans = shape.translated(Vec3(1.0, -1.0, 1.0), new Vec3[8]);
	transMesh = new IndexedMesh(trans);
	double originVertices[3] = { 0, 0, 5 };
	originMesh = new ArrayMesh(originVertices, 1, 3, RenderMode::POINTS);
	double * vecs = new double[140];
	vecs[0] = 0.2;
	vecs[1] = 0.3;
	vecs[2] = 0.7;
	vecs[3] = 0.8;
	vecs[4] = 0.6;
	vecs[5] = 0.7;
	vecs[6] = 0.5;
	vectorMesh = new VectorMesh(vecs, 20);

	eventHandler.setPhysicalRayIntersectCallback([] (Screen* screen, Physical* physical, Vec3 point) {
		screen->intersectedPhysical = physical;
		screen->intersectedPoint = point;
	});

	eventHandler.setPhysicalClickCallback([] (Screen* screen, Physical* physical, Vec3 point) {
		screen->selectedPhysical = physical;
		screen->selectedPoint = point;
	});
}

void Screen::makeCurrent() {
	glfwMakeContextCurrent(this->window);
}

// To be moved elsewhere
Mat4f projectionMatrix;
Mat4f orthoMatrix;
Mat4f rotatedViewMatrix;
Mat4f viewMatrix;
Vec3f viewPosition;

void Screen::update() {

	// IO events
	static double speed = 0.02;
	if (handler->anyKey) {
		if (handler->getKey(GLFW_KEY_1)) {
			speed = 0.02;
		}
		if (handler->getKey(GLFW_KEY_2)) {
			speed = 0.1;
		}
		if (handler->getKey(GLFW_KEY_W)) {
			camera.move(Vec3(0, 0, -speed));
			if (handler->leftDragging) 
				moveGrabbedPhysicalTransversal(this, speed * camera.speed);
		}
		if (handler->getKey(GLFW_KEY_S)) {
			camera.move(Vec3(0, 0, speed));
			if (handler->leftDragging) 
				moveGrabbedPhysicalTransversal(this, -speed * camera.speed);
		}
		if (handler->getKey(GLFW_KEY_D)) {
			camera.move(Vec3(speed, 0, 0));
			if (handler->leftDragging) 
				moveGrabbedPhysicalLateral(this);
		}
		if (handler->getKey(GLFW_KEY_A)) {
			camera.move(Vec3(-speed, 0, 0));
			if (handler->leftDragging) 
				moveGrabbedPhysicalLateral(this);
		}
		if (handler->getKey(GLFW_KEY_SPACE)) {
			camera.move(Vec3(0, speed, 0));
			if (handler->leftDragging) 
				moveGrabbedPhysicalLateral(this);
		}
		if (handler->getKey(GLFW_KEY_LEFT_SHIFT)) {
			camera.move(Vec3(0, -speed, 0));
			if (handler->leftDragging) 
				moveGrabbedPhysicalLateral(this);
		}
		if (handler->getKey(GLFW_KEY_LEFT)) {
			camera.rotate(Vec3(0, -speed, 0));
			if (handler->leftDragging) 
				moveGrabbedPhysicalLateral(this);
		}
		if (handler->getKey(GLFW_KEY_RIGHT)) {
			camera.rotate(Vec3(0, speed, 0));
			if (handler->leftDragging) 
				moveGrabbedPhysicalLateral(this);
		}
		if (handler->getKey(GLFW_KEY_UP)) {
			camera.rotate(Vec3(-speed, 0, 0));
			if (handler->leftDragging) 
				moveGrabbedPhysicalLateral(this);
		}
		if (handler->getKey(GLFW_KEY_DOWN)) {
			camera.rotate(Vec3(speed, 0, 0));
			if (handler->leftDragging) 
				moveGrabbedPhysicalLateral(this);
		}
	}

	// Matrix calculations
	projectionMatrix = Mat4f().perspective(1.0, screenSize.x / screenSize.y, 0.01, 100000.0);
	orthoMatrix = Mat4f().ortho(-1, 1, -screenSize.x / screenSize.y, screenSize.x / screenSize.y, 0.1, 100);
	rotatedViewMatrix = camera.cframe.asMat4f().getRotation();
	viewMatrix = rotatedViewMatrix.translate(-camera.cframe.position.x, -camera.cframe.position.y, -camera.cframe.position.z);
	viewPosition = Vec3f(camera.cframe.position.x, camera.cframe.position.y, camera.cframe.position.z);

	updateIntersectedPhysical(this, world->physicals, handler->curPos, screenSize, viewMatrix, projectionMatrix);
}

void Screen::refresh() {
	// Clear GL buffer bits
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// Initialize vector log buffer
	AddableBuffer<AppDebug::ColoredVec> vecLog = AppDebug::getVecBuffer();

	// Bind basic uniforms
	basicShader.bind();
	basicShader.setUniform("color", Vec3f(1, 1, 1));
	basicShader.setUniform("projectionMatrix", projectionMatrix);
	basicShader.setUniform("viewMatrix", viewMatrix);
	basicShader.setUniform("viewPosition", viewPosition);
	
	// Render world objects
	for (Physical& physical : world->physicals) {
		int meshId = physical.part.drawMeshId;

		// Picker code
		if (&physical == selectedPhysical)
			basicShader.setUniform("color", Vec3f(0.6, 0.8, 0.4));
		else if (&physical == intersectedPhysical)
			basicShader.setUniform("color", Vec3f(0.5, 0.5, 0.5));
		else
			basicShader.setUniform("color", Vec3f(0.3, 0.4, 0.2));

		// Render each physical
		Mat4f transformation = physical.cframe.asMat4f();
		basicShader.setUniform("modelMatrix", transformation);
		meshes[meshId]->render();    
	}

	// Update vector mesh
	vectorMesh->update((double*) vecLog.data, vecLog.index);

	// Reset model matrix
	basicShader.setUniform("modelMatrix", Mat4f());

	// Render vector mesh
	vectorShader.bind();
	vectorShader.setUniform("projectionMatrix", projectionMatrix);
	vectorShader.setUniform("viewMatrix", viewMatrix);
	vectorShader.setUniform("viewPosition", viewPosition);
	vectorMesh->render();

	// Render origin mesh
 	originShader.bind();
	originShader.setUniform("projectionMatrix", projectionMatrix);
	originShader.setUniform("viewPosition", viewPosition);
	originShader.setUniform("orthoMatrix", orthoMatrix);
	originShader.setUniform("viewMatrix", viewMatrix);
	originShader.setUniform("rotatedViewMatrix", rotatedViewMatrix);
	originMesh->render();

	glfwSwapBuffers(this->window);
	glfwPollEvents();
}

void Screen::close() {
	basicShader.close();
	vectorShader.close();
	originShader.close();

	terminateGL();
}

bool Screen::shouldClose() {
	return glfwWindowShouldClose(window) != 0;
}

int Screen::addMeshShape(Shape s) {
	int size = meshes.size();
	meshes.push_back(new IndexedMesh(s));
	return size;
}

