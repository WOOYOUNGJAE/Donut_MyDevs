/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include <donut/render/GBufferFillPass.h>
#include <donut/render/DrawStrategy.h>
#include <donut/app/ApplicationBase.h>
#include <donut/app/Camera.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/TextureCache.h>
#include <donut/engine/Scene.h>
#include <donut/engine/DescriptorTableManager.h>
#include <donut/engine/BindingCache.h>
#include <donut/app/DeviceManager.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>
#include <donut/core/math/math.h>
#include <nvrhi/utils.h>

using namespace donut;
using namespace donut::math;

#include <donut/shaders/view_cb.h>

static const char* g_WindowTitle = "My Devs : Geometry Pipeline";

namespace MyDevs
{
    struct RenderingPassBase
    {
        nvrhi::BindingLayoutHandle bindingLayout;
        nvrhi::BindingSetHandle bindingSet;
        nvrhi::ShaderHandle vertexShader;
        nvrhi::ShaderHandle pixelShader;
        nvrhi::GraphicsPipelineHandle renderingPipeline;
    };

    struct ForwardPass : RenderingPassBase
    {
        ForwardPass(nvrhi::IDevice* device, std::shared_ptr<engine::ShaderFactory> shaderFactory, const nvrhi::BindingSetDesc& bindingSetDesc)
        {
            vertexShader = shaderFactory->CreateShader("/shaders/app/shaders.hlsl", "main_vs", nullptr, nvrhi::ShaderType::Vertex);
            pixelShader = shaderFactory->CreateShader("/shaders/app/shaders.hlsl", "main_ps", nullptr, nvrhi::ShaderType::Pixel);

            nvrhi::utils::CreateBindingSetAndLayout(device, nvrhi::ShaderType::All, 0, bindingSetDesc, bindingLayout, bindingSet);
        }		
    };
    struct GeometryPass : RenderingPassBase
    {
        nvrhi::ShaderHandle geometryShader;
        GeometryPass(nvrhi::IDevice* device, std::shared_ptr<engine::ShaderFactory> shaderFactory, const nvrhi::BindingSetDesc& bindingSetDesc)
        {
            vertexShader = shaderFactory->CreateShader("/shaders/app/normal_debug.hlsl", "main_vs", nullptr, nvrhi::ShaderType::Vertex);
            geometryShader = shaderFactory->CreateShader("/shaders/app/normal_debug.hlsl", "main_gs", nullptr, nvrhi::ShaderType::Geometry);
            pixelShader = shaderFactory->CreateShader("/shaders/app/normal_debug.hlsl", "main_ps", nullptr, nvrhi::ShaderType::Pixel);

            nvrhi::utils::CreateBindingSetAndLayout(device, nvrhi::ShaderType::All, 0, bindingSetDesc, bindingLayout, bindingSet);
        }
    };

};
class BindlessRendering : public app::ApplicationBase
{
private:
    enum BINDING_TYPE {PLANER_VIEW_CBV, INSTANCES_PUSHCONSTANT, INSTANCE_DATA_SRV, GEOMETRY_DATA_SRV, MATERIALS_SRV, SAMPLER, BINDING_TYPE_NUM};
    std::shared_ptr<vfs::RootFileSystem> m_RootFS;

    nvrhi::CommandListHandle m_CommandList;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::BindingLayoutHandle m_BindlessLayout;
    nvrhi::BindingSetHandle m_BindingSet;
    nvrhi::BindingSetItem m_BindingSetItems[BINDING_TYPE_NUM]{};
    nvrhi::ShaderHandle m_VertexShader;
    nvrhi::ShaderHandle m_PixelShader;
    nvrhi::GraphicsPipelineHandle m_ForwardPassPipeline;
    nvrhi::GraphicsPipelineHandle m_GeometryPassPipeline;
    std::unique_ptr<MyDevs::ForwardPass> m_ForwardPass;
    std::unique_ptr<MyDevs::GeometryPass> m_GeometryPass;

    nvrhi::BufferHandle m_ViewConstants;

    nvrhi::TextureHandle m_DepthBuffer;
    std::vector<nvrhi::FramebufferHandle> m_Framebuffers;

    std::shared_ptr<engine::ShaderFactory> m_ShaderFactory;
    std::unique_ptr<engine::Scene> m_Scene;
    std::shared_ptr<engine::DescriptorTableManager> m_DescriptorTableManager;
    std::unique_ptr<engine::BindingCache> m_BindingCache;

    app::FirstPersonCamera m_Camera;
    engine::PlanarView m_View;

public:
    using ApplicationBase::ApplicationBase;

    bool Init()
    {
        std::filesystem::path sceneFileName = app::GetDirectoryWithExecutable().parent_path() / "media/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf";
        std::filesystem::path frameworkShaderPath = app::GetDirectoryWithExecutable() / "shaders/framework" / app::GetShaderTypeName(GetDevice()->getGraphicsAPI());
        std::filesystem::path appShaderPath = app::GetDirectoryWithExecutable() / "shaders/geometry_pipeline" / app::GetShaderTypeName(GetDevice()->getGraphicsAPI());

        m_RootFS = std::make_shared<vfs::RootFileSystem>();
        m_RootFS->mount("/shaders/donut", frameworkShaderPath);
        m_RootFS->mount("/shaders/app", appShaderPath);

        m_ShaderFactory = std::make_shared<engine::ShaderFactory>(GetDevice(), m_RootFS, "/shaders");
        m_CommonPasses = std::make_shared<engine::CommonRenderPasses>(GetDevice(), m_ShaderFactory);
        m_BindingCache = std::make_unique<engine::BindingCache>(GetDevice());

        nvrhi::BindlessLayoutDesc bindlessLayoutDesc;
        bindlessLayoutDesc.visibility = nvrhi::ShaderType::All;
        bindlessLayoutDesc.firstSlot = 0;
        bindlessLayoutDesc.maxCapacity = 1024;
        bindlessLayoutDesc.registerSpaces = {
            nvrhi::BindingLayoutItem::RawBuffer_SRV(1),
            nvrhi::BindingLayoutItem::Texture_SRV(2)
        };
        m_BindlessLayout = GetDevice()->createBindlessLayout(bindlessLayoutDesc);

        m_DescriptorTableManager = std::make_shared<engine::DescriptorTableManager>(GetDevice(), m_BindlessLayout);

        auto nativeFS = std::make_shared<vfs::NativeFileSystem>();
        m_TextureCache = std::make_shared<engine::TextureCache>(GetDevice(), nativeFS, m_DescriptorTableManager);

        m_CommandList = GetDevice()->createCommandList();

        SetAsynchronousLoadingEnabled(false);
        BeginLoadingScene(nativeFS, sceneFileName);

        m_Scene->FinishedLoading(GetFrameIndex());

        m_Camera.LookAt(float3(0.f, 1.8f, 0.f), float3(1.f, 1.8f, 0.f));
        m_Camera.SetMoveSpeed(3.f);

        m_ViewConstants = GetDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(PlanarViewConstants), "ViewConstants", engine::c_MaxRenderPassConstantBufferVersions));
        
        GetDevice()->waitForIdle();

        nvrhi::BindingSetDesc bindingSetDesc;
        m_BindingSetItems[BINDING_TYPE::PLANER_VIEW_CBV] = nvrhi::BindingSetItem::ConstantBuffer(0, m_ViewConstants); // PlanarViewConstants
        m_BindingSetItems[BINDING_TYPE::INSTANCES_PUSHCONSTANT] = nvrhi::BindingSetItem::PushConstants(1, sizeof(int2)); // InstanceConstants
        m_BindingSetItems[BINDING_TYPE::INSTANCE_DATA_SRV] = nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_Scene->GetInstanceBuffer()); // InstanceData
        m_BindingSetItems[BINDING_TYPE::GEOMETRY_DATA_SRV] = nvrhi::BindingSetItem::StructuredBuffer_SRV(1, m_Scene->GetGeometryBuffer()); // GeometryData
        m_BindingSetItems[BINDING_TYPE::MATERIALS_SRV] = nvrhi::BindingSetItem::StructuredBuffer_SRV(2, m_Scene->GetMaterialBuffer()); // MaterialConstants
        m_BindingSetItems[BINDING_TYPE::SAMPLER] = nvrhi::BindingSetItem::Sampler(0, m_CommonPasses->m_AnisotropicWrapSampler);
        bindingSetDesc.bindings.assign(m_BindingSetItems, m_BindingSetItems + BINDING_TYPE_NUM);

        m_ForwardPass = std::make_unique<MyDevs::ForwardPass>(GetDevice(), m_ShaderFactory, bindingSetDesc);
        bindingSetDesc.bindings.clear();
        bindingSetDesc.bindings.emplace_back(m_BindingSetItems[BINDING_TYPE::PLANER_VIEW_CBV]);
        bindingSetDesc.bindings.emplace_back(m_BindingSetItems[BINDING_TYPE::INSTANCES_PUSHCONSTANT]);
        bindingSetDesc.bindings.emplace_back(m_BindingSetItems[BINDING_TYPE::INSTANCE_DATA_SRV]);
        bindingSetDesc.bindings.emplace_back(m_BindingSetItems[BINDING_TYPE::GEOMETRY_DATA_SRV]);
        nvrhi::utils::CreateBindingSetAndLayout(GetDevice(), nvrhi::ShaderType::All, 0, bindingSetDesc, m_BindingLayout, m_BindingSet);
        m_GeometryPass = std::make_unique<MyDevs::GeometryPass>(GetDevice(), m_ShaderFactory, bindingSetDesc);
        return true;
    }

    bool LoadScene(std::shared_ptr<vfs::IFileSystem> fs, const std::filesystem::path& sceneFileName) override
    {
        std::unique_ptr<engine::Scene> scene = std::make_unique<engine::Scene>(GetDevice(),
            *m_ShaderFactory, fs, m_TextureCache, m_DescriptorTableManager, nullptr);

        if (scene->Load(sceneFileName))
        {
            m_Scene = std::move(scene);
            return true;
        }

        return false;
    }

    bool KeyboardUpdate(int key, int scancode, int action, int mods) override
    {
        m_Camera.KeyboardUpdate(key, scancode, action, mods);
        return true;
    }

    bool MousePosUpdate(double xpos, double ypos) override
    {
        m_Camera.MousePosUpdate(xpos, ypos);
        return true;
    }

    bool MouseButtonUpdate(int button, int action, int mods) override
    {
        m_Camera.MouseButtonUpdate(button, action, mods);
        return true;
    }

    void Animate(float fElapsedTimeSeconds) override
    {
        m_Camera.Animate(fElapsedTimeSeconds);
        GetDeviceManager()->SetInformativeWindowTitle(g_WindowTitle);
    }

    void BackBufferResizing() override
    {
        m_DepthBuffer = nullptr;
        m_Framebuffers.clear();
        m_ForwardPassPipeline = nullptr;
        m_GeometryPassPipeline = nullptr;
        m_BindingCache->Clear();
    }

    void Render(nvrhi::IFramebuffer* framebuffer) override
    {
        const auto& fbinfo = framebuffer->getFramebufferInfo();

        if (!m_DepthBuffer)
        {
            nvrhi::TextureDesc textureDesc;
            textureDesc.format = nvrhi::Format::D24S8;
            textureDesc.isRenderTarget = true;
            textureDesc.initialState = nvrhi::ResourceStates::DepthWrite;
            textureDesc.keepInitialState = true;
            textureDesc.clearValue = nvrhi::Color(0.f);
            textureDesc.useClearValue = true;
            textureDesc.debugName = "DepthBuffer";
            textureDesc.width = fbinfo.width;
            textureDesc.height = fbinfo.height;
            textureDesc.dimension = nvrhi::TextureDimension::Texture2D;

            m_DepthBuffer = GetDevice()->createTexture(textureDesc);
        }

        m_Framebuffers.resize(GetDeviceManager()->GetBackBufferCount());

        int const fbindex = GetDeviceManager()->GetCurrentBackBufferIndex();
        if (!m_Framebuffers[fbindex])
        {
            nvrhi::FramebufferDesc framebufferDesc;
            framebufferDesc.addColorAttachment(framebuffer->getDesc().colorAttachments[0]);
            framebufferDesc.setDepthAttachment(m_DepthBuffer);
            m_Framebuffers[fbindex] = GetDevice()->createFramebuffer(framebufferDesc);
        }

        if (m_ForwardPass->renderingPipeline == nullptr)
        {
            nvrhi::GraphicsPipelineDesc pipelineDesc;
            pipelineDesc.VS = m_ForwardPass->vertexShader;
            pipelineDesc.PS = m_ForwardPass->pixelShader;
            pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
            pipelineDesc.bindingLayouts = { m_ForwardPass->bindingLayout, m_BindlessLayout };
            pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
            pipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::GreaterOrEqual;
            pipelineDesc.renderState.rasterState.frontCounterClockwise = true;
            pipelineDesc.renderState.rasterState.setCullBack();
            m_ForwardPass->renderingPipeline = GetDevice()->createGraphicsPipeline(pipelineDesc, m_Framebuffers[fbindex]);
        }

        if (m_GeometryPass->renderingPipeline == nullptr)
        {
            nvrhi::GraphicsPipelineDesc pipelineDesc;
            pipelineDesc.VS = m_GeometryPass->vertexShader;
            pipelineDesc.GS = m_GeometryPass->geometryShader;
            pipelineDesc.PS = m_GeometryPass->pixelShader;
            pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
            pipelineDesc.bindingLayouts = { m_GeometryPass->bindingLayout, m_BindlessLayout };
            pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
            pipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::GreaterOrEqual;
            pipelineDesc.renderState.rasterState.frontCounterClockwise = true;
            pipelineDesc.renderState.rasterState.setCullBack();
            m_GeometryPass->renderingPipeline = GetDevice()->createGraphicsPipeline(pipelineDesc, m_Framebuffers[fbindex]);

        }

        nvrhi::Viewport windowViewport(float(fbinfo.width), float(fbinfo.height));
        m_View.SetViewport(windowViewport);
        m_View.SetMatrices(m_Camera.GetWorldToViewMatrix(), perspProjD3DStyleReverse(dm::PI_f * 0.25f, windowViewport.width() / windowViewport.height(), 0.1f));
        m_View.UpdateCache();

        m_CommandList->open();

        nvrhi::TextureHandle colorBuffer = framebuffer->getDesc().colorAttachments[0].texture;
        m_CommandList->clearTextureFloat(colorBuffer, nvrhi::AllSubresources, nvrhi::Color(0.f));
        m_CommandList->clearDepthStencilTexture(m_DepthBuffer, nvrhi::AllSubresources, true, 0.f, true, 0);

        PlanarViewConstants viewConstants;
        m_View.FillPlanarViewConstants(viewConstants);
        m_CommandList->writeBuffer(m_ViewConstants, &viewConstants, sizeof(viewConstants));

        // Forward Pass
        nvrhi::GraphicsState state;
        state.pipeline = m_ForwardPass->renderingPipeline;
        state.framebuffer = m_Framebuffers[fbindex];
        state.bindings = { m_ForwardPass->bindingSet, m_DescriptorTableManager->GetDescriptorTable() };
        state.viewport = m_View.GetViewportState();
        m_CommandList->setGraphicsState(state);

        for (const auto& instance : m_Scene->GetSceneGraph()->GetMeshInstances())
        {
            const auto& mesh = instance->GetMesh();

            for (size_t i = 0; i < mesh->geometries.size(); i++)
            {
                int2 constants = int2(instance->GetInstanceIndex(), int(i));
                m_CommandList->setPushConstants(&constants, sizeof(constants));

                nvrhi::DrawArguments args;
                args.instanceCount = 1;
                args.vertexCount = mesh->geometries[i]->numIndices;
                m_CommandList->draw(args);
            }
        }


        // Geometry Pass
        state.pipeline = m_GeometryPass->renderingPipeline;
        state.bindings = { m_GeometryPass->bindingSet, m_DescriptorTableManager->GetDescriptorTable() };
        m_CommandList->setGraphicsState(state);
        for (const auto& instance : m_Scene->GetSceneGraph()->GetMeshInstances())
        {
            const auto& mesh = instance->GetMesh();

            for (size_t i = 0; i < mesh->geometries.size(); i++)
            {
                int2 constants = int2(instance->GetInstanceIndex(), int(i));
                m_CommandList->setPushConstants(&constants, sizeof(constants));

                nvrhi::DrawArguments args;
                args.instanceCount = 1;
                args.vertexCount = mesh->geometries[i]->numIndices;
                m_CommandList->draw(args);
            }
        }
        m_CommandList->close();
        GetDevice()->executeCommandList(m_CommandList);
    }
};

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(int __argc, const char** __argv)
#endif
{
    nvrhi::GraphicsAPI api = app::GetGraphicsAPIFromCommandLine(__argc, __argv);
    if (api == nvrhi::GraphicsAPI::D3D11)
    {
        log::error("The Bindless Rendering example does not support D3D11.");
        return 1;
    }

    app::DeviceManager* deviceManager = app::DeviceManager::Create(api);

    app::DeviceCreationParameters deviceParams;
#ifdef _DEBUG
    deviceParams.enableDebugRuntime = true;
    deviceParams.enableNvrhiValidationLayer = true;
#endif

    if (!deviceManager->CreateWindowDeviceAndSwapChain(deviceParams, g_WindowTitle))
    {
        log::fatal("Cannot initialize a graphics device with the requested parameters");
        return 1;
    }

    {
        BindlessRendering example(deviceManager);
        if (example.Init())
        {
            deviceManager->AddRenderPassToBack(&example);
            deviceManager->RunMessageLoop();
            deviceManager->RemoveRenderPass(&example);
        }
    }

    deviceManager->Shutdown();

    delete deviceManager;

    return 0;
}
