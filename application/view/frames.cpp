#include "core.h"
#include "frames.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "shader/shaders.h"
#include "extendedPart.h"
#include "application.h"
#include "worlds.h"
#include "screen.h"

#include "../physics/misc/toString.h"

#include "../graphics/debug/visualDebug.h"
#include "../graphics/renderer.h"
#include "../graphics/texture.h"
#include "../graphics/resource/shaderResource.h"
#include "../graphics/resource/textureResource.h"
#include "../graphics/resource/fontResource.h"
#include "../graphics/font.h"
#include "../graphics/shader/shader.h"

#include "../engine/ecs/tree.h"
#include "../engine/ecs/node.h"
#include "../engine/ecs/registry.h"
#include "../application/ecs/components.h"

#include "../util/resource/resource.h"
#include "../util/resource/resourceManager.h"

namespace P3D::Application {
using namespace Graphics;

float BigFrame::hdr = 1.0f;
float BigFrame::gamma = 1.0f;
float BigFrame::exposure = 1.0f;
Color3 BigFrame::sunColor = Color3::full(1);
Vec3f BigFrame::position = Vec3f(0, 0, 0);
bool BigFrame::doEvents;
bool BigFrame::noRender;
bool BigFrame::doUpdate;
bool BigFrame::isDisabled;
Engine::Layer* BigFrame::selectedLayer = nullptr;
Resource* BigFrame::selectedResource = nullptr;

TextureResource* folderIcon;
TextureResource* objectIcon;

bool IconTreeNode(void* ptr, ImTextureID texture, ImGuiTreeNodeFlags flags, const char* label) {
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;

	ImU32 id = window->GetID(ptr);
	ImVec2 pos = window->DC.CursorPos;
	ImRect button(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
	bool opened = ImGui::TreeNodeBehaviorIsOpen(id);
	bool leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;

	bool hovered, held;
	if (ImGui::ButtonBehavior(button, id, &hovered, &held, true))
		if (!leaf)
			window->DC.StateStorage->SetInt(id, opened ? 0 : 1);

	ImU32 color = ImGui::GetColorU32(ImGuiCol_Button);
	if (held)
		color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
	else if (hovered)
		color = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
	
	if (hovered || held)
		window->DrawList->AddRectFilled(button.Min, button.Max, color);

	// Icon
	float arrowWidth = g.FontSize;
	float buttonSize = g.FontSize + g.Style.FramePadding.y * 2;
	window->DrawList->AddImage(texture, ImVec2(pos.x + arrowWidth, pos.y), ImVec2(pos.x + buttonSize + arrowWidth, pos.y + buttonSize));

	// Text
	ImVec2 textPos = ImVec2(pos.x + buttonSize + arrowWidth + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y);
	ImGui::RenderText(textPos, label);

	// Arrow
	ImVec2 padding = ImVec2(g.Style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, g.Style.FramePadding.y));
	float arrowOffset = std::abs(g.FontSize - arrowWidth) / 2.0f;
	if (!leaf)
		ImGui::RenderArrow(window->DrawList, ImVec2(pos.x + arrowOffset, textPos.y), ImGui::GetColorU32(ImGuiCol_Text), opened ? ImGuiDir_Down : ImGuiDir_Right, 1.0f);

	ImGui::ItemSize(button, g.Style.FramePadding.y);
	ImGui::ItemAdd(button, id);

	if (opened)
		ImGui::TreePush(label);

	return opened;
}

void BigFrame::onInit(Engine::Registry64& registry) {
	folderIcon = ResourceManager::add<TextureResource>("folder", "../res/textures/gui/folder.png");
	objectIcon = ResourceManager::add<TextureResource>("object", "../res/textures/gui/object.png");
}

void BigFrame::renderTextureInfo(Texture* texture) {
	ImGui::Text("ID: %d", texture->getID());
	ImGui::Text("Width: %d", texture->getWidth());
	ImGui::Text("Height: %d", texture->getHeight());
	ImGui::Text("Channels: %d", texture->getChannels());
	ImGui::Text("Unit: %d", texture->getUnit());

	if (ImGui::TreeNodeEx("Preview", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen)) {
		float cw = ImGui::GetColumnWidth();
		float ch = ImGui::GetWindowHeight();
		float tw = ch * texture->getAspect();
		float th = cw / texture->getAspect();

		ImVec2 size;
		if (th > ch)
			size = ImVec2(tw, ch);
		else
			size = ImVec2(cw, th);

		Vec4f c = COLOR::ACCENT;
		ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
		ImGui::Image((void*) (intptr_t) texture->getID(), size, ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(c.x, c.y, c.z, c.w));

		ImGui::TreePop();
	}
}

void BigFrame::renderFontInfo(Font* font) {
	if (ImGui::TreeNode("Atlas")) {
		renderTextureInfo(font->getAtlas());
		ImGui::TreePop();
	}
	
	char selectedCharacter = 0;
	if (ImGui::TreeNode("Characters")) {
		for (int i = 0; i < CHARACTER_COUNT; i++) {
			Character& character = font->getCharacter(i);

			float s = float(character.x) / font->getAtlasWidth();
			float t = float(character.y) / font->getAtlasHeight();
			float ds = float(character.width) / font->getAtlasWidth();
			float dt = float(character.height) / font->getAtlasHeight();

			ImGui::ImageButton((void*) (intptr_t) font->getAtlas()->getID(), ImVec2(character.width / 2.0f, character.height / 2.0f), ImVec2(s, t), ImVec2(s + ds, t + dt));
			float last_button_x2 = ImGui::GetItemRectMax().x;
			float next_button_x2 = last_button_x2 + ImGui::GetStyle().ItemSpacing.x + character.width / 2.0f; // Expected position if next button was on same line
			if (i + 1 < CHARACTER_COUNT && next_button_x2 < ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
				ImGui::SameLine();
		}
		if (ImGui::TreeNodeEx("Info", ImGuiTreeNodeFlags_DefaultOpen)) {
			Character& c = font->getCharacter(selectedCharacter);
			ImGui::Text("Character: %s", std::string(1, (char) selectedCharacter).c_str());
			ImGui::Text("ID: %d", c.id);
			ImGui::Text("Origin: (%d, %d)", c.x, c.x);
			ImGui::Text("Size: (%d, %d)", c.width, c.height);
			ImGui::Text("Bearing: (%d, %d)", c.bx, c.by);
			ImGui::Text("Advance: %d", c.advance);
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
}

void renderShaderTypeEditor(GShader* shader, const ShaderUniform& uniform) {
	ShaderVariableType type = ShaderParser::parseVariableType(uniform.variableType);
	switch (type) {
		case ShaderVariableType::NONE:
		case ShaderVariableType::VOID:
		case ShaderVariableType::STRUCT:
		case ShaderVariableType::VS_OUT:
		case ShaderVariableType::MAT2:
		case ShaderVariableType::MAT3:
		case ShaderVariableType::MAT4:
			ImGui::Text("No editor available");
			ImGui::Text("No editor available");
			break;
		case ShaderVariableType::INT: {
			int value = 0;
			if(ImGui::InputInt(uniform.name.c_str(), &value))
				shader->setUniform(uniform.name, value);
		} break; 
		case ShaderVariableType::FLOAT: {
			float value = 0.0f;
			if(ImGui::InputFloat(uniform.name.c_str(), &value))
				shader->setUniform(uniform.name, value);
		} break; 
		case ShaderVariableType::VEC2: {
			Vec2f value = Vec2f(0, 0);
			if(ImGui::InputFloat2(uniform.name.c_str(), value.data))
				shader->setUniform(uniform.name, value);
		} break; 
		case ShaderVariableType::VEC3: {
			Vec3f value = Vec3f(0, 0, 0);
			if(ImGui::InputFloat3(uniform.name.c_str(), value.data))
				shader->setUniform(uniform.name, value);
		} break; 
		case ShaderVariableType::VEC4: {
			Vec4f value = Vec4f(0, 0, 0, 0);
			if(ImGui::InputFloat4(uniform.name.c_str(), value.data))
				shader->setUniform(uniform.name, value);
		} break; 
		case ShaderVariableType::SAMPLER2D: {
			int value = 0;
			if(ImGui::InputInt(uniform.name.c_str(), &value))
				shader->setUniform(uniform.name, value);
		} break;
		case ShaderVariableType::SAMPLER3D: {
			int value = 0;
			if(ImGui::InputInt(uniform.name.c_str(), &value))
				shader->setUniform(uniform.name, value);
		} break;
		default:
			break;
	}
}

void BigFrame::renderShaderStageInfo(ShaderResource* shader, const ShaderStage& stage) {
	if (ImGui::TreeNode("Code")) {
		ImGui::TextWrapped("%s", stage.source.c_str());

		ImGui::TreePop();
	}

	if (stage.info.uniforms.size()) {
		if (ImGui::TreeNode("Uniforms")) {
			ImGui::Columns(5);
			ImGui::Separator();
			ImGui::Text("ID"); ImGui::NextColumn();
			ImGui::Text("Name"); ImGui::NextColumn();
			ImGui::Text("Type"); ImGui::NextColumn();
			ImGui::Text("Array"); ImGui::NextColumn();
			ImGui::Text("Edit"); ImGui::NextColumn();
			ImGui::Separator();

			for (const ShaderUniform& uniform : stage.info.uniforms) {
				ImGui::Text("%d", shader->getUniform(uniform.name)); ImGui::NextColumn();
				ImGui::Text("%s", uniform.name.c_str()); ImGui::NextColumn();
				ImGui::Text("%s", uniform.variableType.c_str()); ImGui::NextColumn();
				if (uniform.array) ImGui::Text("yes: %d", uniform.amount); else ImGui::Text("no"); ImGui::NextColumn();
				ImGui::Text("No editor available"); ImGui::NextColumn();
				//if (!uniform.array) renderShaderTypeEditor(shader, uniform); else ImGui::Text("No editor available"); ImGui::NextColumn();
			}
			ImGui::Columns(1);
			ImGui::Separator();
			ImGui::TreePop();
		}
	}

	if (stage.info.locals.size()) {
		if (ImGui::TreeNode("Locals")) {
			ImGui::Columns(3);
			ImGui::Separator();
			ImGui::Text("Name"); ImGui::NextColumn();
			ImGui::Text("Type"); ImGui::NextColumn();
			ImGui::Text("Array"); ImGui::NextColumn();
			ImGui::Separator();

			for (const ShaderLocal& local : stage.info.locals) {
				ImGui::Text("%s", local.name.c_str()); ImGui::NextColumn();
				ImGui::Text("%s", local.variableType.c_str()); ImGui::NextColumn();
				if (local.array)
					ImGui::Text("yes: %d", local.amount);
				else
					ImGui::Text("no");

				ImGui::NextColumn();
			}
			ImGui::Columns(1);
			ImGui::Separator();
			ImGui::TreePop();
		}
	}

	if (stage.info.globals.size()) {
		if (ImGui::TreeNode("Globals")) {
			ImGui::Columns(4);
			ImGui::Separator();
			ImGui::Text("Name"); ImGui::NextColumn();
			ImGui::Text("IO"); ImGui::NextColumn();
			ImGui::Text("Type"); ImGui::NextColumn();
			ImGui::Text("Array"); ImGui::NextColumn();
			ImGui::Separator();

			for (const ShaderGlobal& global : stage.info.globals) {
				ImGui::Text("%s", global.name.c_str()); ImGui::NextColumn();
				ImGui::Text("%s", global.ioType.c_str()); ImGui::NextColumn();
				ImGui::Text("%s", global.variableType.c_str()); ImGui::NextColumn();
				if (global.array)
					ImGui::Text("yes: %d", global.amount);
				else
					ImGui::Text("no");

				ImGui::NextColumn();
			}
			ImGui::Columns(1);
			ImGui::Separator();
			ImGui::TreePop();
		}
	}

	if (stage.info.structs.size()) {
		if (ImGui::TreeNode("Structs")) {
			for (const auto& strctit : stage.info.structs) {
				const ShaderStruct& strct = strctit.second;

				if (ImGui::TreeNode(strct.name.c_str())) {
					ImGui::Columns(3);
					ImGui::Separator();
					ImGui::Text("Name"); ImGui::NextColumn();
					ImGui::Text("Type"); ImGui::NextColumn();
					ImGui::Text("Array"); ImGui::NextColumn();
					ImGui::Separator();

					for (const ShaderLocal& local : strct.locals) {
						ImGui::Text("%s", local.name.c_str()); ImGui::NextColumn();
						ImGui::Text("%s", local.variableType.c_str()); ImGui::NextColumn();
						if (local.array)
							ImGui::Text("yes: %d", local.amount);
						else
							ImGui::Text("no");

						ImGui::NextColumn();
					}
					ImGui::Columns(1);
					ImGui::Separator();
					ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}
	}

	if (stage.info.defines.size()) {
		if (ImGui::TreeNode("Defines")) {
			ImGui::Columns(2);
			ImGui::Separator();
			ImGui::Text("Name"); ImGui::NextColumn();
			ImGui::Text("Value"); ImGui::NextColumn();
			ImGui::Separator();

			for (const auto& define : stage.info.defines) {
				ImGui::Text("%s", define.first.c_str()); ImGui::NextColumn();
				ImGui::Text("%d", (int) define.second); ImGui::NextColumn();
			}
			ImGui::Columns(1);
			ImGui::Separator();
			ImGui::TreePop();
		}
	}
}

void BigFrame::renderShaderInfo(ShaderResource* shader) {
	ImGui::BeginChild("Shaders");

	if (ImGui::BeginTabBar("##tabs")) {
		if (shader->flags & shader->VERTEX) {
			if (ImGui::BeginTabItem("Vertex")) {
				renderShaderStageInfo(shader, shader->vertexStage);
				ImGui::EndTabItem();
			}
		}
		if (shader->flags & shader->GEOMETRY) {
			if (ImGui::BeginTabItem("Geometry")) {
				renderShaderStageInfo(shader, shader->geometryStage);
				ImGui::EndTabItem();
			}
		}
		if (shader->flags & shader->TESSELATION_CONTROL) {
			if (ImGui::BeginTabItem("Tesselation control")) {
				renderShaderStageInfo(shader, shader->tesselationControlStage);
				ImGui::EndTabItem();
			}
		}
		if (shader->flags & shader->TESSELATION_EVALUATE) {
			if (ImGui::BeginTabItem("Tesselation evaluate")) {
				renderShaderStageInfo(shader, shader->tesselationEvaluationStage);
				ImGui::EndTabItem();
			}
		}
		if (shader->flags & shader->FRAGMENT) {
			if (ImGui::BeginTabItem("Fragment")) {
				renderShaderStageInfo(shader, shader->fragmentStage);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	if (ImGui::Button("Reload")) {
		ShaderSource shaderSource = parseShader(shader->getName(), shader->getPath());
		if (shaderSource.vertexSource.empty() || shaderSource.fragmentSource.empty()) {
			ImGui::EndChild();
			return;
		}
		shader->reload(shaderSource);
	}
	
	ImGui::EndChild();
}

void BigFrame::renderResourceFrame() {
	ImGui::Columns(2, 0, true);
	ImGui::Separator();

	auto map = ResourceManager::getResourceMap();
	for (auto iterator = map.begin(); iterator != map.end(); ++iterator) {
		if (ImGui::TreeNodeEx(iterator->first.c_str(), ImGuiTreeNodeFlags_SpanFullWidth)) {
			for (Resource* resource : iterator->second) {
				if (ImGui::Selectable(resource->getName().c_str(), resource == selectedResource))
					selectedResource = resource;
			}
			ImGui::TreePop();
		}
	}

	ImGui::NextColumn();

	if (selectedResource) {
		ImGui::Text("Name: %s", selectedResource->getName().c_str());
		ImGui::Text("Path: %s", selectedResource->getPath().c_str());
		ImGui::Text("Type: %s", selectedResource->getTypeName().c_str());
		switch (selectedResource->getType()) {
			case ResourceType::Texture: 
				renderTextureInfo(static_cast<TextureResource*>(selectedResource));
				break;
			case ResourceType::Font:
				renderFontInfo(static_cast<FontResource*>(selectedResource));
				break;
			case ResourceType::GShader:
				renderShaderInfo(static_cast<ShaderResource*>(selectedResource));
				break;
			default:
				ImGui::Text("Visual respresentation not supported.");
				break;
		}
	} else {
		ImGui::Text("No resource selected.");
	}

	ImGui::Columns(1);
	ImGui::Separator();
}

void BigFrame::renderECSNode(Engine::Registry64& registry, Engine::Registry64::entity_type entity) {
	void* id = (void*) entity;
	const auto& children = registry.getChildren(entity);
	
	bool leaf = children.begin() == registry.end();
	ImTextureID texture = (void*) (intptr_t) objectIcon->getID();
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | (leaf ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None);

	std::string name = registry.getOr<Comp::Tag>(entity, Comp::Tag(std::to_string(entity))).name;
	bool open = IconTreeNode(id, texture, flags, name.c_str());
		
	// Render popup
	if (ImGui::BeginPopupContextItem()) {
		// Collapse / unfold
		if (!leaf) {
			if (open) {
				if (ImGui::BeginMenu("Collapse")) {
					if (ImGui::MenuItem("Node")) {

					}
					if (ImGui::MenuItem("Recursively")) {

					}
					ImGui::EndMenu();
				}
			} else {
				if (ImGui::MenuItem("Unfold node")) {
					if (ImGui::MenuItem("Node")) {

					}
					if (ImGui::MenuItem("Recursively")) {

					}
					ImGui::EndMenu();
				}
			}
		}

		// Rename
		if (ImGui::BeginMenu("Rename")) {
			char* buffer = new char[name.size() + 1];
			//strcpy(buffer, node->getName().c_str());
			ImGui::Text("Edit name:");
			ImGui::InputText("##edit", buffer, IM_ARRAYSIZE(buffer));
			if (ImGui::Button("Apply")) {
				std::string newName(buffer);
				registry.get<Comp::Tag>(entity)->name = newName;
			}
			ImGui::EndMenu();
				
			delete[] buffer;
		}

		// Add
		if (ImGui::BeginMenu("Add")) {
			if (ImGui::MenuItem("Entity")) {
				//node->getTree()->createEntity(node);
			}
			if (ImGui::MenuItem("Folder")) {
				//node->getTree()->createGroup(node);
			}
			ImGui::EndMenu();
		}

		// Delete
		if (ImGui::MenuItem("Delete")) {
			//node->getTree()->removeNode(node, false);
		}

		ImGui::EndPopup();
	}

	// Render children
	if (!leaf) {
		if (open) {
			for (auto child : children)
				renderECSNode(registry, child);
			ImGui::TreePop();
		}
	}

	if (ImGui::BeginDragDropSource()) {
		// check sizeof later
		ImGui::SetDragDropPayload("ECS_NODE", id, sizeof(id));

		float buttonSize = GImGui->FontSize + GImGui->Style.FramePadding.y * 2;

		ImGui::Image(texture, ImVec2(buttonSize, buttonSize));
		ImGui::SameLine();
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 4));
		ImGui::Text("Move %s", name.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ECS_NODE")) {
			Engine::Node* source = static_cast<Engine::Node*>(payload->Data);
		}
		ImGui::EndDragDropTarget();
	}
}

void BigFrame::renderECSTree(Engine::Registry64& registry) {
	using namespace Engine;

	for (auto entity : registry) {
		if (parent(entity) == registry.null_entity)
			renderECSNode(registry, entity);
	}
}

void BigFrame::renderPropertiesFrame(Engine::Registry64& registry) {
	ExtendedPart* sp = screen.selectedPart;

	ImGui::SetNextTreeNodeOpen(true);
	if (ImGui::TreeNode("General")) {
		std::string name = registry.getOr<Comp::Tag>((sp)? sp->entity : 0, (sp)? std::to_string(sp->entity) : "-").name;
		ImGui::Text("Name: %s", name.c_str());
		//ImGui::Text("Mesh ID: %s", (sp) ? str(sp->visualData.id).c_str() : "-");

		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true);
	if (ImGui::TreeNode("Physical")) {
		if (sp) {
			// Position
			position = castPositionToVec3(sp->getPosition());
			if (ImGui::InputFloat3("Position: ", position.data, 3)) {
				GlobalCFrame frame = sp->getCFrame();
				frame.position = castVec3fToPosition(position);
				sp->setCFrame(frame);
			}
		} else {
			// Position
			position = Vec3f(0, 0, 0);
			ImGui::InputFloat3("Position", position.data, 3, ImGuiInputTextFlags_ReadOnly);
		}

		ImGui::Text("Velocity: %s", (sp) ? str(sp->getMotion().getVelocity()).c_str() : "-");
		ImGui::Text("Acceleration: %s", (sp) ? str(sp->getMotion().getAcceleration()).c_str() : "-");
		ImGui::Text("Angular velocity: %s", (sp) ? str(sp->getMotion().getAngularVelocity()).c_str() : "-");
		ImGui::Text("Angular acceleration: %s", (sp) ? str(sp->getMotion().getAngularAcceleration()).c_str() : "-");
		ImGui::Text("Mass: %s", (sp) ? str(sp->getMass()).c_str() : "-");
		ImGui::Text("Friction: %s", (sp) ? str(sp->properties.friction).c_str() : "-");
		ImGui::Text("Density: %s", (sp) ? str(sp->properties.density).c_str() : "-");
		ImGui::Text("Bouncyness: %s", (sp) ? str(sp->properties.bouncyness).c_str() : "-");
		ImGui::Text("Conveyor: %s", (sp) ? str(sp->properties.conveyorEffect).c_str() : "-");
		ImGui::Text("Inertia: %s", (sp) ? str(sp->getInertia()).c_str() : "-");


		if(sp != nullptr && sp->parent != nullptr) {
			const MotorizedPhysical* phys = sp->parent->mainPhysical;
			ImGui::Text("Physical Info");

			ImGui::Text("Total impulse: %s", str(phys->getTotalImpulse()).c_str());
			ImGui::Text("Total angular momentum: %s", str(phys->getTotalAngularMomentum()).c_str());
			ImGui::Text("motion of COM: %s", str(phys->getMotionOfCenterOfMass()).c_str());
		}

		static volatile ExtendedPart* selectedPart = nullptr;
		if(sp != nullptr) {
			selectedPart = sp;
		}

		if(ImGui::Button("Debug This Part")) {
			Log::debug("Debugging part %d", reinterpret_cast<uint64_t>(selectedPart));
			#ifdef _MSC_VER
			__debugbreak();
			#else
			Log::warn("Debug breaking is not supported on non-linux platforms");
			#endif
		}

		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true);
	if (ImGui::TreeNode("Material")) {
		if (sp) {
			Ref<Comp::Material> material = registry.getOrAdd<Comp::Material>(sp->entity);
			ImGui::ColorEdit4("Albedo", material->albedo.data);
			ImGui::SliderFloat("Metalness", &material->metalness, 0, 1);
			ImGui::SliderFloat("Roughness", &material->roughness, 0, 1);
			ImGui::SliderFloat("Ambient occlusion", &material->ao, 0, 1);
			//if (ImGui::Button(sp ? (sp->renderMode == Renderer::FILL ? "Render mode: fill" : "Render mode: wireframe") : "l"))
			//	if (sp) sp->renderMode = sp->renderMode == Renderer::FILL ? Renderer::WIREFRAME : Renderer::FILL;
		} else {
			ImGui::Text("No part selected");
		}

		ImGui::TreePop();
	}
}

void BigFrame::renderDebugFrame() {
	using namespace ::Debug;
	using namespace Graphics::Debug;
	ImGui::SetNextTreeNodeOpen(true);
	if (ImGui::TreeNode("Vectors")) {
		ImGui::Checkbox("Info", &vectorDebugEnabled[INFO_VEC]);
		ImGui::Checkbox("Position", &vectorDebugEnabled[POSITION]);
		ImGui::Checkbox("Velocity", &vectorDebugEnabled[VELOCITY]);
		ImGui::Checkbox("Acceleration", &vectorDebugEnabled[ACCELERATION]);
		ImGui::Checkbox("Moment", &vectorDebugEnabled[MOMENT]);
		ImGui::Checkbox("Force", &vectorDebugEnabled[FORCE]);
		ImGui::Checkbox("Angular impulse", &vectorDebugEnabled[ANGULAR_IMPULSE]);
		ImGui::Checkbox("Impulse", &vectorDebugEnabled[IMPULSE]);

		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true);
	if (ImGui::TreeNode("Points")) {
		ImGui::Checkbox("Info", &pointDebugEnabled[INFO_POINT]);
		ImGui::Checkbox("Center of mass", &pointDebugEnabled[CENTER_OF_MASS]);
		ImGui::Checkbox("Intersections", &pointDebugEnabled[INTERSECTION]);

		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true);
	if (ImGui::TreeNode("Render")) {
		ImGui::Checkbox("Render pies", &renderPiesEnabled);
		if (ImGui::Button("Switch collision sphere render mode")) colissionSpheresMode = static_cast<SphereColissionRenderMode>((static_cast<int>(colissionSpheresMode) + 1) % 3);

		ImGui::TreePop();
	}
}

void BigFrame::renderEnvironmentFrame() {
	if (ImGui::SliderFloat("HDR", &hdr, 0, 1)) {
		Shaders::basicShader.updateHDR(hdr);
		Shaders::instanceShader.updateHDR(hdr);
	}

	if (ImGui::SliderFloat("Gamma", &gamma, 0, 3)) {
		Shaders::basicShader.updateGamma(gamma);
		Shaders::instanceShader.updateGamma(gamma);
	}

	if (ImGui::SliderFloat("Exposure", &exposure, 0, 2)) {
		Shaders::basicShader.updateExposure(exposure);
		Shaders::instanceShader.updateExposure(exposure);
	}

	if (ImGui::ColorEdit3("Sun color", sunColor.data)) {
		Shaders::basicShader.updateSunColor(sunColor);
		Shaders::instanceShader.updateSunColor(sunColor);
	}
}

void BigFrame::renderLayerFrame() {
	using namespace Engine;

	ImGui::Columns(2, 0, true);
	ImGui::Separator();
	for (Layer* layer : screen.layerStack) {
		if (ImGui::Selectable(layer->name.c_str(), selectedLayer == layer))
			selectedLayer = layer;
	}

	ImGui::NextColumn();

	ImGui::Text("Name: %s", (selectedLayer) ? selectedLayer->name.c_str() : "-");
	if (selectedLayer) {
		ImGui::SetNextTreeNodeOpen(true);
		if (ImGui::TreeNode("Flags")) {
			char& flags = selectedLayer->flags;

			doEvents = !(flags & Layer::NoEvents);
			doUpdate = !(flags & Layer::NoUpdate);
			noRender = !(flags & Layer::NoRender);
			isDisabled = flags & Layer::Disabled;

			ImGui::Checkbox("Disabled", &isDisabled);
			ImGui::Checkbox("Events", &doEvents);
			ImGui::Checkbox("Update", &doUpdate);
			ImGui::Checkbox("Render", &noRender);

			flags = (isDisabled) ? flags | Layer::Disabled : flags & ~Layer::Disabled;
			flags = (doEvents) ? flags & ~Layer::NoEvents : flags | Layer::NoEvents;
			flags = (doUpdate) ? flags & ~Layer::NoUpdate : flags | Layer::NoUpdate;
			flags = (noRender) ? flags & ~Layer::NoRender : flags | Layer::NoRender;

			ImGui::TreePop();
		}
	}

	ImGui::Columns(1);
	ImGui::Separator();
}

void BigFrame::render(Engine::Registry64& registry) {
	//ImGui::ShowDemoWindow();
	
	ImGui::Begin("Tree");
	renderECSTree(registry);
	ImGui::End();
	
	ImGui::Begin("Properties");
	renderPropertiesFrame(registry);
	ImGui::End();
	ImGui::Begin("Layers");
	renderLayerFrame();
	ImGui::End();
	ImGui::Begin("Resources");
	renderResourceFrame();
	ImGui::End();
	ImGui::Begin("Environment");
	renderEnvironmentFrame();
	ImGui::End();
	ImGui::Begin("Debug");
	renderDebugFrame();
	ImGui::End();
}

}
