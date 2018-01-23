#pragma once

#ifndef RAZ_SHADER_HPP
#define RAZ_SHADER_HPP

#include <string>
#include <initializer_list>

#include "glew/include/GL/glew.h"

enum ShaderType { RAZ_SHADER_TYPE_VERTEX = GL_VERTEX_SHADER,
                  RAZ_SHADER_TYPE_FRAGMENT = GL_FRAGMENT_SHADER,
                  RAZ_SHADER_TYPE_COMPUTE = GL_COMPUTE_SHADER,
                  RAZ_SHADER_TYPE_GEOMETRY = GL_GEOMETRY_SHADER };

namespace Raz {

class Shader {
public:
  GLuint getIndex() const { return m_index; }
  const std::string& getContent() const { return m_content; }

  void read(const std::string& fileName);

  ~Shader() { glDeleteShader(m_index); }

protected:
  explicit Shader(ShaderType type) : m_type{ type } {}

  GLuint m_index {};
  std::string m_content;

  ShaderType m_type;
};

class VertexShader : public Shader {
public:
  explicit VertexShader(const std::string& fileName) : VertexShader() { read(fileName); }

private:
  VertexShader() : Shader(RAZ_SHADER_TYPE_VERTEX) {}
};

class FragmentShader : public Shader {
public:
  explicit FragmentShader(const std::string& fileName) : FragmentShader() { read(fileName); }

private:
  FragmentShader() : Shader(RAZ_SHADER_TYPE_FRAGMENT) {}
};

} // namespace Raz

#endif // RAZ_SHADER_HPP
