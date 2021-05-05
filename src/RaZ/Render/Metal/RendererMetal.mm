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
#import <simd/simd.h>

#include <unordered_map>

namespace Raz {

struct CoreRendererData
{
    id<MTLDevice> device;
    NSWindow* nswin;
    CAMetalLayer* metal_layer;

    id<MTLCommandQueue> command_queue;
    id<MTLCommandBuffer> command_buffer;
    id<MTLRenderCommandEncoder> render_command_encoder;
    id<MTLRenderPipelineState> debug_rps;
    id<CAMetalDrawable> drawable;

    id<MTLTexture> depth_texture;
};

struct MetalObjects
{
    std::unordered_map<std::string, id<MTLDepthStencilState>> depth_stencil_states;
};

static inline CoreRendererData s_data {};
static inline MetalObjects s_obj;

void Renderer::createDevice()
{
    s_data.device = MTLCreateSystemDefaultDevice();
    
    if (s_data.device)
        s_isInitialized = true;
    else
        std::cerr << "Error: Failed to initialize Metal." << std::endl;

    NSString* deviceName = s_data.device.name;
    std::string bar = std::string([deviceName UTF8String]);
    std::cout << "Salut NaN! Voici Metal avec RaZ! GPU utilisé: " << bar << std::endl;
}

void Renderer::createSurfaceFromCocoa()
{
    s_data.nswin = glfwGetCocoaWindow(s_windowHandle);
    assert(s_data.nswin);
    s_data.metal_layer = [CAMetalLayer layer];

    s_data.metal_layer.device = s_data.device;
    s_data.metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    s_data.nswin.contentView.layer = s_data.metal_layer;
    s_data.nswin.contentView.wantsLayer = YES;
}

void Renderer::initialiseDepthFunctions()
{
    @autoreleasepool {
        // ALWAYS
        MTLDepthStencilDescriptor* always_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        always_descriptor.depthWriteEnabled = YES;
        always_descriptor.depthCompareFunction = MTLCompareFunctionAlways;
        s_obj.depth_stencil_states["Always"] = [s_data.device newDepthStencilStateWithDescriptor: always_descriptor];

        // EQUAL
        MTLDepthStencilDescriptor* equal_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        equal_descriptor.depthWriteEnabled = YES;
        equal_descriptor.depthCompareFunction = MTLCompareFunctionEqual;
        s_obj.depth_stencil_states["Equal"] = [s_data.device newDepthStencilStateWithDescriptor: equal_descriptor];

        // NOT EQUAL
        MTLDepthStencilDescriptor* not_equal_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        not_equal_descriptor.depthWriteEnabled = YES;
        not_equal_descriptor.depthCompareFunction = MTLCompareFunctionNotEqual;
        s_obj.depth_stencil_states["Not Equal"] = [s_data.device newDepthStencilStateWithDescriptor: not_equal_descriptor];

        // LESS EQUAL
        MTLDepthStencilDescriptor* less_equal_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        less_equal_descriptor.depthWriteEnabled = YES;
        less_equal_descriptor.depthCompareFunction = MTLCompareFunctionLessEqual;
        s_obj.depth_stencil_states["Less Equal"] = [s_data.device newDepthStencilStateWithDescriptor: less_equal_descriptor];

        // LESS
        MTLDepthStencilDescriptor* less_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        less_descriptor.depthWriteEnabled = YES;
        less_descriptor.depthCompareFunction = MTLCompareFunctionLess;
        s_obj.depth_stencil_states["Less"] = [s_data.device newDepthStencilStateWithDescriptor: less_descriptor];

        // GREATER EQUAL
        MTLDepthStencilDescriptor* greater_equal_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        greater_equal_descriptor.depthWriteEnabled = YES;
        greater_equal_descriptor.depthCompareFunction = MTLCompareFunctionGreaterEqual;
        s_obj.depth_stencil_states["Greater Equal"] = [s_data.device newDepthStencilStateWithDescriptor: greater_equal_descriptor];

        // GREATER
        MTLDepthStencilDescriptor* greater_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        greater_descriptor.depthWriteEnabled = YES;
        greater_descriptor.depthCompareFunction = MTLCompareFunctionGreater;
        s_obj.depth_stencil_states["Greater"] = [s_data.device newDepthStencilStateWithDescriptor: greater_descriptor];

        // NEVER
        MTLDepthStencilDescriptor* never_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        always_descriptor.depthWriteEnabled = YES;
        always_descriptor.depthCompareFunction = MTLCompareFunctionNever;
        s_obj.depth_stencil_states["Never"] = [s_data.device newDepthStencilStateWithDescriptor: never_descriptor];
    }

    // test if everything is correct
    assert(s_obj.depth_stencil_states["Always"]);
    assert(s_obj.depth_stencil_states["Equal"]);
    assert(s_obj.depth_stencil_states["Not Equal"]);
    assert(s_obj.depth_stencil_states["Less Equal"]);
    assert(s_obj.depth_stencil_states["Less"]);
    assert(s_obj.depth_stencil_states["Greater Equal"]);
    assert(s_obj.depth_stencil_states["Greater"]);
    assert(s_obj.depth_stencil_states["Never"]);
}

void Renderer::initDepthTexture()
{
    int width;
    int height;
    glfwGetFramebufferSize(s_windowHandle, &width, &height);

    @autoreleasepool {
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8 width:width height:height mipmapped:NO];
        descriptor.storageMode = MTLStorageModePrivate;
        descriptor.usage = MTLTextureUsageRenderTarget;

        s_data.depth_texture = [s_data.device newTextureWithDescriptor: descriptor];
    }

    assert(s_data.depth_texture);
}

void Renderer::initialize(GLFWwindow* windowHandle)
{
    s_windowHandle = windowHandle;

    if (s_isInitialized)
        return;
    
    createDevice();
    createSurfaceFromCocoa();
    initDepthTexture();
    initialiseDepthFunctions();

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
        "    return float4(1, 1, 1, 1);\n"
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
    assert(s_data.debug_rps);
}

void Renderer::setDepthFunction(DepthFunction function)
{
    assert(s_data.render_command_encoder);

    switch (function)
    {
    case DepthFunction::NEVER:
        [s_data.render_command_encoder setDepthStencilState: s_obj.depth_stencil_states["Never"]];
        break;
    case DepthFunction::ALWAYS:
        [s_data.render_command_encoder setDepthStencilState: s_obj.depth_stencil_states["Always"]];
        break;
    case DepthFunction::LESS:
        [s_data.render_command_encoder setDepthStencilState: s_obj.depth_stencil_states["Less"]];
        break;
    case DepthFunction::LESS_EQUAL:
        [s_data.render_command_encoder setDepthStencilState: s_obj.depth_stencil_states["Less Equal"]];
        break;
    case DepthFunction::GREATER:
        [s_data.render_command_encoder setDepthStencilState: s_obj.depth_stencil_states["Greater"]];
        break;
    case DepthFunction::GREATER_EQUAL:
        [s_data.render_command_encoder setDepthStencilState: s_obj.depth_stencil_states["Greater Equal"]];
        break;
    case DepthFunction::EQUAL:
        [s_data.render_command_encoder setDepthStencilState: s_obj.depth_stencil_states["Equal"]];
        break;
    case DepthFunction::NOT_EQUAL:
        [s_data.render_command_encoder setDepthStencilState: s_obj.depth_stencil_states["Not Equal"]];
        break;
    }
}

void Renderer::setFaceCulling(CullingMode mode)
{
    assert(s_data.render_command_encoder);

    switch (mode)
    {
    case CullingMode::FRONT:
        [s_data.render_command_encoder setCullMode: MTLCullModeFront];
        break;
    case CullingMode::BACK:
        [s_data.render_command_encoder setCullMode: MTLCullModeBack];
        break;
    case CullingMode::FRONT_BACK:
        break;
    default:
        [s_data.render_command_encoder setCullMode: MTLCullModeNone];
        break;
    }
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

    // Color attachment 
    MTLRenderPassColorAttachmentDescriptor* cd = rpd.colorAttachments[0];
    cd.texture = s_data.drawable.texture;
    cd.loadAction = MTLLoadActionClear;
    cd.clearColor = MTLClearColorMake(r, g, b, a);
    cd.storeAction = MTLStoreActionStore;

    // Depth attachment
    MTLRenderPassDepthAttachmentDescriptor *depthAttachment = rpd.depthAttachment;
    depthAttachment.texture = s_data.depth_texture;
    depthAttachment.clearDepth = 1.0;
    depthAttachment.storeAction = MTLStoreActionDontCare;
    depthAttachment.loadAction = MTLLoadActionClear;

    s_data.render_command_encoder = [s_data.command_buffer renderCommandEncoderWithDescriptor: rpd];
    [s_data.render_command_encoder setRenderPipelineState: s_data.debug_rps];

    [s_data.render_command_encoder setVertexBytes:(vector_float4[]){
            { -0.5,-0.5, 0, 1 },
            {  0.5,-0.5, 0, 1 },
            {  0.0, 0.5, 0, 1 },
        } length:3 * sizeof(vector_float4) atIndex:0];
    [s_data.render_command_encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
}

void Renderer::endRendering()
{
    [s_data.render_command_encoder endEncoding];
    [s_data.command_buffer presentDrawable: s_data.drawable];
    [s_data.command_buffer commit];
}

}