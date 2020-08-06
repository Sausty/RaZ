#pragma once

#ifndef RAZ_RENDERSYSTEM_HPP
#define RAZ_RENDERSYSTEM_HPP

#include "RaZ/Entity.hpp"
#include "RaZ/Math/Matrix.hpp"
#include "RaZ/Math/Vector.hpp"
#include "RaZ/Render/Cubemap.hpp"
#include "RaZ/Render/Framebuffer.hpp"
#include "RaZ/Render/RenderGraph.hpp"
#include "RaZ/Render/UniformBuffer.hpp"
#include "RaZ/System.hpp"
#include "RaZ/Utils/Window.hpp"

namespace Raz {

/// RenderSystem class, handling the rendering part.
class RenderSystem final : public System {
  friend RenderGraph;

public:
  /// Creates a render system, initializing its inner data.
  RenderSystem() { initialize(); }
  /// Creates a render system with a given scene size.
  /// \param sceneWidth Width of the scene.
  /// \param sceneHeight Height of the scene.
  RenderSystem(unsigned int sceneWidth, unsigned int sceneHeight) : RenderSystem() { resizeViewport(sceneWidth, sceneHeight); }
  /// Creates a render system along with a Window.
  /// \param sceneWidth Width of the scene.
  /// \param sceneHeight Height of the scene.
  /// \param windowTitle Title of the window.
  /// \param antiAliasingSampleCount Number of anti-aliasing samples.
  RenderSystem(unsigned int sceneWidth, unsigned int sceneHeight, const std::string& windowTitle, uint8_t antiAliasingSampleCount = 1)
    : m_window{ Window::create(sceneWidth, sceneHeight, windowTitle, antiAliasingSampleCount) } { initialize(sceneWidth, sceneHeight); }

  bool hasWindow() const { return (m_window != nullptr); }
  const Window& getWindow() const { assert("Error: Window must be set before being accessed." && hasWindow()); return *m_window; }
  Window& getWindow() { return const_cast<Window&>(static_cast<const RenderSystem*>(this)->getWindow()); }
  const RenderPass& getGeometryPass() const { return m_renderGraph.getGeometryPass(); }
  RenderPass& getGeometryPass() { return m_renderGraph.getGeometryPass(); }
  const ShaderProgram& getGeometryProgram() const { return getGeometryPass().getProgram(); }
  ShaderProgram& getGeometryProgram() { return getGeometryPass().getProgram(); }
  const RenderGraph& getRenderGraph() const { return m_renderGraph; }
  RenderGraph& getRenderGraph() { return m_renderGraph; }
  bool hasCubemap() const { return m_cubemap.has_value(); }
  const Cubemap& getCubemap() const { assert("Error: Cubemap must be set before being accessed." && hasCubemap()); return *m_cubemap; }

  void setCubemap(Cubemap cubemap) { m_cubemap = std::move(cubemap); }

  void resizeViewport(unsigned int width, unsigned int height);
  void createWindow(unsigned int width, unsigned int height, const std::string& title = "") { m_window = Window::create(width, height, title); }
  RenderPass& addRenderPass(VertexShader vertShader, FragmentShader fragShader);
  RenderPass& addRenderPass(FragmentShader fragShader);
  bool update(float deltaTime) override;
  void sendViewMatrix(const Mat4f& viewMat) const { m_cameraUbo.sendData(viewMat, 0); }
  void sendInverseViewMatrix(const Mat4f& invViewMat) const { m_cameraUbo.sendData(invViewMat, sizeof(Mat4f)); }
  void sendProjectionMatrix(const Mat4f& projMat) const { m_cameraUbo.sendData(projMat, sizeof(Mat4f) * 2); }
  void sendInverseProjectionMatrix(const Mat4f& invProjMat) const { m_cameraUbo.sendData(invProjMat, sizeof(Mat4f) * 3); }
  void sendViewProjectionMatrix(const Mat4f& viewProjMat) const { m_cameraUbo.sendData(viewProjMat, sizeof(Mat4f) * 4); }
  void sendCameraPosition(const Vec3f& cameraPos) const { m_cameraUbo.sendData(cameraPos, sizeof(Mat4f) * 5); }
  void sendCameraMatrices(const Mat4f& viewProjMat) const;
  void sendCameraMatrices() const;
  void updateLight(const Entity* entity, std::size_t lightIndex) const;
  void updateLights() const;
  void removeCubemap() { m_cubemap.reset(); }
  void updateShaders() const;
  void saveToImage(const FilePath& filePath, TextureFormat format = TextureFormat::RGB) const;
  void destroy() override { if (m_window) m_window->setShouldClose(); }

protected:
  void linkEntity(const EntityPtr& entity) override;

private:
  void initialize();
  void initialize(unsigned int sceneWidth, unsigned int sceneHeight);

  unsigned int m_sceneWidth {};
  unsigned int m_sceneHeight {};

  WindowPtr m_window {};
  Entity* m_cameraEntity {};

  RenderGraph m_renderGraph {};
  UniformBuffer m_cameraUbo = UniformBuffer(sizeof(Mat4f) * 5 + sizeof(Vec4f), 0);

  std::optional<Cubemap> m_cubemap {};
};

} // namespace Raz

#endif // RAZ_RENDERSYSTEM_HPP
