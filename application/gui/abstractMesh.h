#pragma once

#include "GL\glew.h"
#include "vertexArray.h"
#include "bufferLayout.h"
#include "../../util/log.h"

enum class RenderMode {
	TRIANGLES = GL_TRIANGLES,
	LINES = GL_LINES,
	POINTS = GL_POINTS
};

class AbstractMesh {
public:
	VertexArray* vertexArray = nullptr;
	BufferLayout bufferLayout;
	RenderMode renderMode;

	AbstractMesh(RenderMode rendermode) : renderMode(renderMode) {
		Log::debug("Created new AbstractMesh with renderMode %d", (int) renderMode);
		vertexArray = new VertexArray();
	};

	AbstractMesh() : renderMode(RenderMode::TRIANGLES) {
		Log::debug("Created new AbstractMesh with renderMode %d", (int) renderMode);
		vertexArray = new VertexArray();
	};

	virtual void render() = 0;
	virtual void close() = 0;
};

