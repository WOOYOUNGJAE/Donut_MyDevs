#pragma once

#include <donut/engine/ShaderFactory.h>
#include <donut/engine/TextureCache.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/Scene.h>
#include <donut/app/DeviceManager.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>
#include <nvrhi/utils.h>
#include "donut/app/ApplicationBase.h"
#include <donut/app/DeviceManager.h>

using namespace donut;
using namespace donut::math;
using namespace donut::app;
using namespace donut::vfs;
using namespace donut::engine;
//using namespace donut::render;

static const char* g_WindowTitle = "Donut Example: Vertex Buffer";

struct Vertex
{
    math::float3 position;
    math::float2 uv;
};

static const Vertex g_Vertices[] = {
    { {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f} }, // front face
    { { 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f} },
    { {-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f} },
    { { 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f} },

    { { 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f} }, // right side face
    { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f} },
    { { 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f} },
    { { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f} },

    { {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f} }, // left side face
    { {-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f} },
    { {-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f} },
    { {-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f} },

    { { 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f} }, // back face
    { {-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f} },
    { { 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f} },
    { {-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f} },

    { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f} }, // top face
    { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f} },
    { { 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f} },
    { {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f} },

    { { 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f} }, // bottom face
    { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f} },
    { { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f} },
    { {-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f} },
};

static const uint32_t g_Indices[] = {
     0,  1,  2,   0,  3,  1, // front face
     4,  5,  6,   4,  7,  5, // left face
     8,  9, 10,   8, 11,  9, // right face
    12, 13, 14,  12, 15, 13, // back face
    16, 17, 18,  16, 19, 17, // top face
    20, 21, 22,  20, 23, 21, // bottom face
};

constexpr uint32_t c_NumViews = 1;

static const math::float3 g_RotationAxes[c_NumViews] = {
    math::float3(1.f, 0.f, 0.f)
};

class GeometryPipeline : public app::IRenderPass
{
private:
    nvrhi::ShaderHandle m_VertexShader;
    nvrhi::ShaderHandle m_GeometryShader;
    nvrhi::ShaderHandle m_PixelShader;
    nvrhi::BufferHandle m_ConstantBuffer;
    nvrhi::BufferHandle m_VertexBuffer;
    nvrhi::BufferHandle m_IndexBuffer;
    nvrhi::TextureHandle m_Texture;
    nvrhi::InputLayoutHandle m_InputLayout;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::BindingSetHandle m_BindingSets[c_NumViews];
    nvrhi::GraphicsPipelineHandle m_Pipeline;
    nvrhi::CommandListHandle m_CommandList;
    float m_Rotation = 0.f;

public:
    using IRenderPass::IRenderPass;
    explicit GeometryPipeline(DeviceManager* deviceManager, std::shared_ptr<ShaderFactory> shaderFactory): IRenderPass(deviceManager)
    {
        m_VertexShader = shaderFactory->CreateShader("app/shaders.hlsl", "main_vs", nullptr, nvrhi::ShaderType::Vertex);
        //m_GeometryShader = shaderFactory->CreateShader("app/shaders.hlsl", "main_gs", nullptr, nvrhi::ShaderType::Geometry);
        m_PixelShader = shaderFactory->CreateShader("app/shaders.hlsl", "main_ps", nullptr, nvrhi::ShaderType::Pixel);

        if (!m_VertexShader/* || !m_GeometryShader*/ || !m_PixelShader)
        {
            return;
        }

        m_ConstantBuffer = GetDevice()->createBuffer(nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(ConstantBufferEntry) * c_NumViews, "ConstantBuffer")
            .setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(0)
                .setBufferIndex(0)
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("UV")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setOffset(0)
                .setBufferIndex(1)
                .setElementStride(sizeof(Vertex)),
        };
        m_InputLayout = GetDevice()->createInputLayout(attributes, uint32_t(std::size(attributes)), m_VertexShader);


        engine::CommonRenderPasses commonPasses(GetDevice(), shaderFactory);
        engine::TextureCache textureCache(GetDevice(), nativeFS, nullptr);

        m_CommandList = GetDevice()->createCommandList();
        m_CommandList->open();

        nvrhi::BufferDesc vertexBufferDesc;
        vertexBufferDesc.byteSize = sizeof(g_Vertices);
        vertexBufferDesc.isVertexBuffer = true;
        vertexBufferDesc.debugName = "VertexBuffer";
        vertexBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
        m_VertexBuffer = GetDevice()->createBuffer(vertexBufferDesc);

        m_CommandList->beginTrackingBufferState(m_VertexBuffer, nvrhi::ResourceStates::CopyDest);
        m_CommandList->writeBuffer(m_VertexBuffer, g_Vertices, sizeof(g_Vertices));
        m_CommandList->setPermanentBufferState(m_VertexBuffer, nvrhi::ResourceStates::VertexBuffer);

        nvrhi::BufferDesc indexBufferDesc;
        indexBufferDesc.byteSize = sizeof(g_Indices);
        indexBufferDesc.isIndexBuffer = true;
        indexBufferDesc.debugName = "IndexBuffer";
        indexBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
        m_IndexBuffer = GetDevice()->createBuffer(indexBufferDesc);

        m_CommandList->beginTrackingBufferState(m_IndexBuffer, nvrhi::ResourceStates::CopyDest);
        m_CommandList->writeBuffer(m_IndexBuffer, g_Indices, sizeof(g_Indices));
        m_CommandList->setPermanentBufferState(m_IndexBuffer, nvrhi::ResourceStates::IndexBuffer);

        std::filesystem::path textureFileName = app::GetDirectoryWithExecutable().parent_path() / "media/nvidia-logo.png";
        std::shared_ptr<engine::LoadedTexture> texture = textureCache.LoadTextureFromFile(textureFileName, true, nullptr, m_CommandList);
        m_Texture = texture->texture;

        m_CommandList->close();
        GetDevice()->executeCommandList(m_CommandList);

        if (!texture->texture)
        {
            log::error("Couldn't load the texture");
            return;
        }

        // Create a single binding layout and multiple binding sets, one set per view.
        // The different binding sets use different slices of the same constant buffer.
        for (uint32_t viewIndex = 0; viewIndex < c_NumViews; ++viewIndex)
        {
            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.bindings = {
                // Note: using viewIndex to construct a buffer range.
                nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer, nvrhi::BufferRange(sizeof(ConstantBufferEntry) * viewIndex, sizeof(ConstantBufferEntry))),
                // Texutre and sampler are the same for all model views.
                nvrhi::BindingSetItem::Texture_SRV(0, m_Texture),
                nvrhi::BindingSetItem::Sampler(0, commonPasses.m_AnisotropicWrapSampler)
            };

            // Create the binding layout (if it's empty -- so, on the first iteration) and the binding set.
            if (!nvrhi::utils::CreateBindingSetAndLayout(GetDevice(), nvrhi::ShaderType::All, 0, bindingSetDesc, m_BindingLayout, m_BindingSets[viewIndex]))
            {
                log::error("Couldn't create the binding set or layout");
                return;
            }
        }
    }

    // This example uses a single large constant buffer with multiple views to draw multiple versions of the same model.
    // The alignment and size of partially bound constant buffers must be a multiple of 256 bytes,
    // so define a struct that represents one constant buffer entry or slice for one draw call.
    struct ConstantBufferEntry
    {
        dm::float4x4 viewProjMatrix;
        float padding[16 * 3];
    };

    static_assert(sizeof(ConstantBufferEntry) == nvrhi::c_ConstantBufferOffsetSizeAlignment, "sizeof(ConstantBufferEntry) must be 256 bytes");

    bool Init();

    void Animate(float seconds) override
    {
        m_Rotation += seconds * 1.1f;
        GetDeviceManager()->SetInformativeWindowTitle(g_WindowTitle);
    }

    void BackBufferResizing() override
    {
        m_Pipeline = nullptr;
    }

    void Render(nvrhi::IFramebuffer* framebuffer) override;
};

class GeometryPipelineDemo : public donut::app::ApplicationBase
{
private:
    std::shared_ptr<RootFileSystem>     m_RootFs;
    std::shared_ptr<NativeFileSystem>   m_NativeFs;
    std::vector<std::string>            m_SceneFilesAvailable;
    std::string                         m_CurrentSceneName;
    std::filesystem::path               m_SceneDir;
    std::shared_ptr<Scene>				m_Scene;
    std::shared_ptr<ShaderFactory>      m_ShaderFactory;
    std::unique_ptr<GeometryPipeline>   m_RenderPass;

    nvrhi::CommandListHandle            m_CommandList;


public:
    GeometryPipelineDemo(DeviceManager* deviceManager, const std::string& sceneName)
        : ApplicationBase(deviceManager)
    {
        m_RootFs = std::make_shared<RootFileSystem>();

        std::filesystem::path mediaDir = app::GetDirectoryWithExecutable().parent_path() / "media";
        std::filesystem::path frameworkShaderDir = app::GetDirectoryWithExecutable() / "shaders/framework" / app::GetShaderTypeName(GetDevice()->getGraphicsAPI());

        m_RootFs->mount("/media", mediaDir);
        m_RootFs->mount("/shaders/donut", frameworkShaderDir);

        m_NativeFs = std::make_shared<NativeFileSystem>();

        m_SceneDir = mediaDir / "glTF-Sample-Assets/Models/";
        m_SceneFilesAvailable = FindScenes(*m_NativeFs, m_SceneDir);

        if (sceneName.empty() && m_SceneFilesAvailable.empty())
        {
            log::fatal("No scene file found in media folder '%s'\n"
                "Please make sure that folder contains valid scene files.", m_SceneDir.generic_string().c_str());
        }

        m_ShaderFactory = std::make_shared<ShaderFactory>(GetDevice(), m_RootFs, "/shaders");
        m_RenderPass = std::make_unique<GeometryPipeline>(deviceManager);

        m_CommandList = GetDevice()->createCommandList();

    }
};