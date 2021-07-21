#pragma once

#include "../eventHandler.h"
#include "../util/properties.h"
#include "../graphics/visualShape.h"
#include "../engine/event/event.h"
#include "../engine/layer/layerStack.h"
#include "../engine/ecs/registry.h"
#include "camera.h"
#include "graphics/buffers/frameBuffer.h"
#include "graphics/batch/instanceBatchManager.h"
#include "graphics/batch/OrderedBatchManager.h"

namespace P3D {

namespace Graphics {
	struct Quad;
	class MainFrameBuffer;
};

namespace Application {

	class StandardInputHandler;
	class PlayerWorld;

	bool initGLEW();
	bool initGLFW();
	void terminateGLFW();

	class Screen {
	public:
		Engine::Registry64 registry;

		PlayerWorld* world;
		Vec2i dimension;

		Camera camera;
		Engine::LayerStack layerStack;
		EventHandler eventHandler;
		Util::Properties properties;

		SRef<Graphics::MainFrameBuffer> screenFrameBuffer;
		URef<Graphics::DefaultInstanceBatchManager> instanceManager;
		URef<Graphics::DefaultOrderedBatchManager> basicManager;

		Engine::Registry64::entity_type intersectedEntity = 0;
		Engine::Registry64::entity_type selectedEntity = 0;
		ExtendedPart* intersectedPart = nullptr;
		ExtendedPart* selectedPart = nullptr;
		Position intersectedPoint;
		Position selectedPoint;

		Screen();
		Screen(int width, int height, PlayerWorld* world);

		void onInit(bool quickBoot);
		void onUpdate();
		void onEvent(Engine::Event& event);
		void onRender();
		void onClose();

		bool shouldClose();
	};

	extern StandardInputHandler* handler;

};

}