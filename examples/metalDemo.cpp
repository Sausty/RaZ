#include "RaZ/RaZ.hpp"

using namespace std::literals;

int main() {
  Raz::Application app;
  Raz::World& world = app.addWorld(2);

  auto& render = world.addSystem<Raz::RenderSystem>(1280, 720, "RaZ");

  Raz::Entity& camera = world.addEntityWithComponent<Raz::Transform>(Raz::Vec3f(0.f, 0.f, -5.f));
  camera.addComponent<Raz::Camera>(render.getWindow().getWidth(), render.getWindow().getHeight());

  render.getWindow().addKeyCallback(Raz::Keyboard::ESCAPE, [&app] (float /* deltaTime */) { app.quit(); });

  app.run();

  return EXIT_SUCCESS;
}
