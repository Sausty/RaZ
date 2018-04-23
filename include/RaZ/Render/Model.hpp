#pragma once

#ifndef RAZ_MODEL_HPP
#define RAZ_MODEL_HPP

#include "RaZ/Math/Matrix.hpp"
#include "RaZ/Math/Transform.hpp"
#include "RaZ/Render/Mesh.hpp"
#include "RaZ/Render/Material.hpp"

namespace Raz {

class Model : public Transform {
public:
  explicit Model(MeshPtr mesh) : m_mesh{ std::move(mesh) } {}

  const MeshPtr& getMesh() const { return m_mesh; }

  void load(const ShaderProgram& program) const { m_mesh->load(&program); }
  void draw(const ShaderProgram& program) const { m_mesh->draw(&program); }

private:
  MeshPtr m_mesh;
};

using ModelPtr = std::unique_ptr<Model>;

} // namespace Raz

#endif // RAZ_MODEL_HPP
