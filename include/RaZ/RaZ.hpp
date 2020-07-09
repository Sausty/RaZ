#pragma once

#ifndef RAZ_RAZ_HPP
#define RAZ_RAZ_HPP

#define GLEW_STATIC

#include "Application.hpp"
#include "Component.hpp"
#include "Entity.hpp"
#include "System.hpp"
#include "World.hpp"
#include "Audio/Sound.hpp"
#include "Math/Angle.hpp"
#include "Math/Constants.hpp"
#include "Math/Matrix.hpp"
#include "Math/Quaternion.hpp"
#include "Math/Transform.hpp"
#include "Math/Vector.hpp"
#include "Physics/PhysicsSystem.hpp"
#include "Physics/RigidBody.hpp"
#include "Render/Camera.hpp"
#include "Render/Cubemap.hpp"
#include "Render/Framebuffer.hpp"
#include "Render/GraphicObjects.hpp"
#include "Render/Light.hpp"
#include "Render/Material.hpp"
#include "Render/Mesh.hpp"
#include "Render/Renderer.hpp"
#include "Render/RenderPass.hpp"
#include "Render/RenderSystem.hpp"
#include "Render/Shader.hpp"
#include "Render/ShaderProgram.hpp"
#include "Render/Submesh.hpp"
#include "Render/Texture.hpp"
#include "Render/UniformBuffer.hpp"
#include "Utils/Bitset.hpp"
#include "Utils/CompilerUtils.hpp"
#include "Utils/EnumUtils.hpp"
#include "Utils/FilePath.hpp"
#include "Utils/FloatUtils.hpp"
#include "Utils/Graph.hpp"
#include "Utils/Image.hpp"
#include "Utils/Input.hpp"
#include "Utils/Overlay.hpp"
#include "Utils/Ray.hpp"
#include "Utils/Shape.hpp"
#include "Utils/StrUtils.hpp"
#include "Utils/Threading.hpp"
#include "Utils/TypeUtils.hpp"
#include "Utils/Window.hpp"

using namespace Raz::Literals;

#endif // RAZ_RAZ_HPP
