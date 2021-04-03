#pragma once

#include "../graphics/glfwUtils.h"
#include "../engine/event/mouseEvent.h"
#include "../engine/tool/stateTool.h"
#include "ecs/components.h"
#include <optional>

namespace P3D::Application {

class TranslationTool : public Engine::StateTool {
private:
	enum SelectionToolStatus : Engine::ToolStatus {
		kIdle        = 0,
		kTranslateX  = 1,
		kTranslateY  = 2,
		kTranslateZ  = 3,
		kTranslateC  = 4,
		kTranslateXY = 5,
		kTranslateXZ = 6,
		kTranslateYZ = 7,
	};

	bool active = false;
	
public:
	DEFINE_TOOL("Translate", "Translate entities by clicking and dragging or using the handles", Graphics::GLFW::Cursor::ARROW);
	
	~TranslationTool() = default;

	void onRegister() override;
	void onDeregister() override;
	void onRender() override;
	void onUpdate() override;
	void onEvent(Engine::Event& event) override;

	bool onMousePress(Engine::MousePressEvent& event);
	bool onMouseRelease(Engine::MouseReleaseEvent& event);
	bool onMouseDrag(Engine::MouseDragEvent& event);
	
	static void translateInPlane(const Vec3& normal, bool local = true);
	static void translateAlongLine(const Vec3& direction, bool local = true);
};

};