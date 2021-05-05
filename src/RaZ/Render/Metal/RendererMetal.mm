#include "RaZ/Render/Metal/RendererMetal.hpp"

#include <cassert>
#include <Foundation/Foundation.h>
#include <iostream>
#include <Metal/Metal.h>
#include <string>

#define GLFW_INCLUDE_NONE
#import <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#import <GLFW/glfw3native.h>

#import <QuartzCore/QuartzCore.h>

namespace Raz {

struct RendererData
{
    id<MTLDevice> device;
    NSWindow* nswin;
    CAMetalLayer* metal_layer;

    id<MTLCommandQueue> command_queue;
    id<MTLCommandBuffer> command_buffer;
    id<MTLRenderCommandEncoder> render_command_encoder;
    id<MTLRenderPipelineState> debug_rps;
    id<CAMetalDrawable> drawable;
};

static inline RendererData s_data {};

void Renderer::initialize(GLFWwindow* windowHandle)
{
    s_windowHandle = windowHandle;

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

    // Fetch cocoa window
    s_data.nswin = glfwGetCocoaWindow(windowHandle);
    assert(s_data.nswin);
    s_data.metal_layer = [CAMetalLayer layer];

    s_data.metal_layer.device = s_data.device;
    s_data.metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    s_data.nswin.contentView.layer = s_data.metal_layer;
    s_data.nswin.contentView.wantsLayer = YES;

    // Command queue
    s_data.command_queue = [s_data.device newCommandQueue];

    // RPS
    MTLCompileOptions* compileOptions = [MTLCompileOptions new];
    compileOptions.languageVersion = MTLLanguageVersion1_1;
    NSError* compileError;
    id<MTLLibrary> lib = [s_data.device newLibraryWithSource:
       @"#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "vertex float4 v_simple(\n"
        "    constant float4* in  [[buffer(0)]],\n"
        "    uint             vid [[vertex_id]])\n"
        "{\n"
        "    return in[vid];\n"
        "}\n"
        "fragment float4 f_simple(\n"
        "    float4 in [[stage_in]])\n"
        "{\n"
        "    return float4(1, 0, 0, 1);\n"
        "}\n"
       options:compileOptions error:&compileError];

    id<MTLFunction> vs = [lib newFunctionWithName:@"v_simple"];
    assert(vs);
    id<MTLFunction> fs = [lib newFunctionWithName:@"f_simple"];
    assert(fs);

    MTLRenderPipelineDescriptor* rpd = [MTLRenderPipelineDescriptor new];
    rpd.colorAttachments[0].pixelFormat = s_data.metal_layer.pixelFormat;
    rpd.vertexFunction = vs;
    rpd.fragmentFunction = fs;
    s_data.debug_rps = [s_data.device newRenderPipelineStateWithDescriptor:rpd error:NULL];
}

void Renderer::clear(MaskType type)
{
    float ratio;
    int width, height;
    glfwGetFramebufferSize(s_windowHandle, &width, &height);

    s_data.metal_layer.drawableSize = CGSizeMake(width, height);

    s_data.drawable = [s_data.metal_layer nextDrawable];
    assert(s_data.drawable);

    s_data.command_buffer = [s_data.command_queue commandBuffer];
}

void Renderer::clearColor(float r, float g, float b, float a)
{
    MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor new];
    MTLRenderPassColorAttachmentDescriptor* cd = rpd.colorAttachments[0];
    cd.texture = s_data.drawable.texture;
    cd.loadAction = MTLLoadActionClear;
    cd.clearColor = MTLClearColorMake(r, g, b, a);
    cd.storeAction = MTLStoreActionStore;

    s_data.render_command_encoder = [s_data.command_buffer renderCommandEncoderWithDescriptor: rpd];
    [s_data.render_command_encoder setRenderPipelineState: s_data.debug_rps];
}

void Renderer::endRendering()
{
    [s_data.render_command_encoder endEncoding];
    [s_data.command_buffer presentDrawable: s_data.drawable];
    [s_data.command_buffer commit];
}

}