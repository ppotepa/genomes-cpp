#include "DiligentBackend.hpp"
#include "DiligentBackendImpl.hpp"
#include "DiligentScenePasses.hpp"

#if defined(_WIN32)
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#else
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#endif

namespace genomes::render {
namespace {
using BackendResult=foundation::Result<std::unique_ptr<DiligentBackend>,foundation::Error>;
BackendResult createError(const char* message,foundation::ErrorCode code=foundation::ErrorCode::Internal) {
    return BackendResult::failure({code,message});
}
}

BackendResult DiligentBackend::create(RenderConfig config) { return create(config,{}); }

BackendResult DiligentBackend::create(RenderConfig config,foundation::NativeWindowHandle native_window) {
    if (!config.valid()) return createError("invalid render configuration",foundation::ErrorCode::InvalidArgument);
#if defined(_WIN32)
    if (config.backend!=RenderBackendKind::D3D12)
#else
    if (config.backend!=RenderBackendKind::Vulkan)
#endif
        return createError("render backend not supported on this platform",foundation::ErrorCode::Unsupported);
    if (!config.headless&&!native_window.valid()) return createError("windowed renderer requires a native window");
    try {
        auto impl=std::make_unique<Impl>(config);
#if defined(_WIN32)
        auto* factory=Diligent::GetEngineFactoryD3D12();
        Diligent::EngineD3D12CreateInfo device_info{};
#else
        auto* factory=Diligent::GetEngineFactoryVk();
        Diligent::EngineVkCreateInfo device_info{};
#endif
        if (!factory) return createError("Diligent factory unavailable");
        factory->SetBreakOnError(false);device_info.EnableValidation=config.validation;
        Diligent::IRenderDevice* device=nullptr;Diligent::IDeviceContext* context=nullptr;
#if defined(_WIN32)
        factory->CreateDeviceAndContextsD3D12(device_info,&device,&context);
#else
        factory->CreateDeviceAndContextsVk(device_info,&device,&context);
#endif
        impl->device.Attach(device);impl->context.Attach(context);
        if (!device||!context) return createError("Diligent device/context creation failed");
        if (!config.headless) {
            Diligent::NativeWindow window{};
#if PLATFORM_WIN32
            if (native_window.system!=foundation::NativeWindowSystem::Win32)
                return createError("Win32 native window required",foundation::ErrorCode::Unsupported);
            window.hWnd=native_window.window;
#elif PLATFORM_LINUX
            if (native_window.system!=foundation::NativeWindowSystem::X11)
                return createError("X11 native window required",foundation::ErrorCode::Unsupported);
            window.pDisplay=native_window.display;
            window.WindowId=static_cast<Diligent::Uint32>(native_window.window_id);
#else
            return createError("unsupported native window platform",foundation::ErrorCode::Unsupported);
#endif
            Diligent::SwapChainDesc desc{};
            desc.Width=config.width;desc.Height=config.height;desc.BufferCount=config.frames_in_flight;
            desc.ColorBufferFormat=Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
            Diligent::ISwapChain* chain=nullptr;
#if defined(_WIN32)
            Diligent::FullScreenModeDesc fullscreen{};
            factory->CreateSwapChainD3D12(device,context,desc,fullscreen,window,&chain);
#else
            factory->CreateSwapChainVk(device,context,desc,window,&chain);
#endif
            impl->swap_chain.Attach(chain);
            if (!chain) return createError("Diligent swap-chain creation failed");
            if (!impl->initializePipelines()) return createError("Diligent shader/pipeline/binding creation failed");
        }
        impl->capabilities.initialized=true;
        auto backend=std::unique_ptr<DiligentBackend>(new DiligentBackend(std::move(impl)));
        if (const auto result=backend->recreate_depth_buffer();!result) return BackendResult::failure(result.error());
        return BackendResult::success(std::move(backend));
    } catch (const std::exception&) { return createError("exception while creating Diligent renderer"); }
}

DiligentBackend::DiligentBackend(std::unique_ptr<Impl> impl) noexcept:impl_(std::move(impl)) {}
DiligentBackend::~DiligentBackend() { shutdown(); }
RenderCapabilities DiligentBackend::capabilities() const noexcept { return impl_?impl_->capabilities:RenderCapabilities{}; }
RenderUploadTelemetry DiligentBackend::uploadTelemetry() const noexcept { return impl_?impl_->telemetry:RenderUploadTelemetry{}; }

RenderResult DiligentBackend::recreate_depth_buffer() noexcept {
    if (!impl_||!impl_->swap_chain) return RenderResult::success();
    try {
        const auto swap=impl_->swap_chain->GetDesc();
        if (swap.Width==0||swap.Height==0) return diligent_detail::error("invalid depth target dimensions");
        Diligent::TextureDesc desc{};desc.Name="Genomes depth";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width=swap.Width;desc.Height=swap.Height;desc.MipLevels=1;desc.SampleCount=1;
        desc.Format=Diligent::TEX_FORMAT_D32_FLOAT;desc.BindFlags=Diligent::BIND_DEPTH_STENCIL;
        Diligent::ITexture* raw=nullptr;impl_->device->CreateTexture(desc,nullptr,&raw);
        diligent_detail::Ptr<Diligent::ITexture> candidate;candidate.Attach(raw);
        if (!candidate) return diligent_detail::error("depth allocation failed",foundation::ErrorCode::Internal);
        diligent_detail::Ptr<Diligent::ITextureView> view=candidate->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
        if (!view) return diligent_detail::error("depth view creation failed",foundation::ErrorCode::Internal);
        impl_->depth_view=std::move(view);impl_->depth=std::move(candidate);
        return RenderResult::success();
    } catch (const std::exception&) { return diligent_detail::error("depth creation exception",foundation::ErrorCode::Internal); }
}

RenderResult DiligentBackend::begin_frame() noexcept {
    if (!impl_||!impl_->capabilities.initialized) return diligent_detail::error("renderer is shut down");
    if (impl_->frame_open) return diligent_detail::error("frame already open");
    try {
        auto& t=impl_->telemetry;++t.frame;t.mesh_uploads=0;t.mesh_upload_bytes=0;t.palette_updates=0;t.draw_calls=0;
        if (impl_->swap_chain) {
            auto* target=impl_->swap_chain->GetCurrentBackBufferRTV();
            if (!target) return diligent_detail::error("no current back buffer");
            Diligent::ITextureView* targets[]{target};
            impl_->context->SetRenderTargets(1,targets,impl_->depth_view,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            constexpr float clear[]{0.035F,0.055F,0.085F,1.0F};
            impl_->context->ClearRenderTarget(target,clear,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            impl_->context->ClearDepthStencil(impl_->depth_view,Diligent::CLEAR_DEPTH_FLAG,1.0F,0,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            impl_->viewport(RenderCamera{});
        }
        impl_->frame_open=true;return RenderResult::success();
    } catch (const std::exception&) { return diligent_detail::error("begin frame exception",foundation::ErrorCode::Internal); }
}

RenderResult DiligentBackend::draw_meshes(const PresentationSnapshot& snapshot) noexcept {
    if (!impl_||!impl_->frame_open) return diligent_detail::error("no open renderer frame");
    if (!impl_->swap_chain) return RenderResult::success();
    try { return impl_->drawMeshes(snapshot); }
    catch (const std::exception&) { return diligent_detail::error("mesh submission exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::draw_instances(const PresentationSnapshot& snapshot) noexcept {
    if (!impl_||!impl_->frame_open) return diligent_detail::error("no open renderer frame");
    if (!impl_->swap_chain) return RenderResult::success();
    try { return impl_->drawInstances(snapshot); }
    catch (const std::exception&) { return diligent_detail::error("instance submission exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::draw_ui(const ui::UiDocument& document) noexcept {
    if (!impl_||!impl_->frame_open) return diligent_detail::error("no open renderer frame");
    if (!impl_->swap_chain) return RenderResult::success();
    try { return impl_->drawUi(document); }
    catch (const std::exception&) { return diligent_detail::error("UI submission exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::end_frame() noexcept {
    if (!impl_||!impl_->frame_open) return diligent_detail::error("no open renderer frame");
    // Clear the logical state before submission/present, including failure paths.
    impl_->frame_open=false;
    try {
        impl_->context->Flush();
        if (impl_->swap_chain) impl_->swap_chain->Present(1);
        else impl_->context->FinishFrame();
        const auto frame=impl_->telemetry.frame;
        const auto prune=[frame](auto& cache) {
            for (auto it=cache.begin();it!=cache.end();) {
                if (frame-it->second.last_seen>240U) it=cache.erase(it);else ++it;
            }
        };
        prune(impl_->mesh_cache);prune(impl_->skin_cache);
        return RenderResult::success();
    } catch (const std::exception&) { return diligent_detail::error("end frame exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::resize(std::uint32_t width,std::uint32_t height) noexcept {
    if (!impl_) return diligent_detail::error("renderer is shut down");
    auto config=impl_->config;config.width=width;config.height=height;
    if (!config.valid()) return diligent_detail::error("invalid render dimensions",foundation::ErrorCode::InvalidArgument);
    if (impl_->frame_open) return diligent_detail::error("resize inside an open frame");
    try {
        if (impl_->swap_chain) {
            impl_->context->WaitForIdle();impl_->swap_chain->Resize(width,height);
            if (auto result=recreate_depth_buffer();!result) return result;
        }
        impl_->config=config;return RenderResult::success();
    } catch (const std::exception&) { return diligent_detail::error("resize exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::wait_idle() noexcept {
    if (!impl_) return diligent_detail::error("renderer is shut down");
    try { impl_->context->WaitForIdle();return RenderResult::success(); }
    catch (const std::exception&) { return diligent_detail::error("wait idle exception",foundation::ErrorCode::Internal); }
}
void DiligentBackend::shutdown() noexcept {
    if (!impl_) return;
    try { if (impl_->context) impl_->context->WaitForIdle(); } catch (const std::exception&) {}
    // Impl field order releases buffers/SRBs/pipelines before context/device.
    impl_.reset();
}
} // namespace genomes::render
