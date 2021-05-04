#include "RaZ/Render/Metal/RendererMetal.hpp"

#include <cassert>
#include <Foundation/Foundation.h>
#include <iostream>
#include <Metal/Metal.h>
#include <string>

namespace Raz {

struct RendererData
{
    id<MTLDevice> device;
};

static inline RendererData s_data {};

void Renderer::initialize(GLFWwindow* windowHandle)
{
    if (s_isInitialized)
        return;

    s_data.device = MTLCreateSystemDefaultDevice();
    
    if (s_data.device)
        s_isInitialized = true;
    else
        std::cerr << "Error: Failed to initialize Metal." << std::endl;

    NSString* deviceName = s_data.device.name;
    std::string bar = std::string([deviceName UTF8String]);
    std::cout << "Salut NaN! Voici Metal avec RaZ! GPU utilisé: " << bar << std::endl;
}

}