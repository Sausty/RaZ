#include "RaZ/Render/Texture.hpp"

namespace Raz {

Texture::Texture(uint8_t value) : Texture() {
  const std::array<uint8_t, 3> pixel = { value, value, value };

  bind();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
  unbind();
}

Texture::Texture(unsigned int width, unsigned int height, bool isDepthTexture) : Texture() {
  bind();

  if (!isDepthTexture) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  }

  unbind();
}

void Texture::load(const std::string& fileName) {
  const Image img(fileName);

  bind();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (img.getColorspace() == GL_RED || img.getColorspace() == GL_RG) {
    const std::array<GLint, 4> swizzle = { GL_RED, GL_RED, GL_RED, (img.getColorspace() == GL_RED ? GL_ONE : GL_GREEN) };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle.data());
  }

  glTexImage2D(GL_TEXTURE_2D,
               0,
               img.getColorspace(),
               img.getWidth(),
               img.getHeight(),
               0,
               img.getColorspace(),
               GL_UNSIGNED_BYTE,
               img.getDataPtr());
  glGenerateMipmap(GL_TEXTURE_2D);
  unbind();
}

} // namespace Raz
