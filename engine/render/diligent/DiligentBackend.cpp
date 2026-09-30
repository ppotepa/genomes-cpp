#include "DiligentBackendImpl.hpp"
#if defined(_WIN32)
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#else
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#endif
#include <iostream>

namespace genomes::render {
using namespace diligent_detail;
using BackendResult=foundation::Result<std::unique_ptr<DiligentBackend>,foundation::Error>;
BackendResult DiligentBackend::create(RenderConfig config) { return create(config,{}); }
BackendResult DiligentBackend::create(RenderConfig config,foundation::NativeWindowHandle native) {
    if (!config.valid()) return BackendResult::failure({foundation::ErrorCode::InvalidArgument,"invalid Diligent configuration"});
#if defined(_WIN32)
    if (config.backend!=RenderBackendKind::D3D12) return BackendResult::failure({foundation::ErrorCode::Unsupported,"Windows Diligent profile requires D3D12"});
#else
    if (config.backend!=RenderBackendKind::Vulkan) return BackendResult::failure({foundation::ErrorCode::Unsupported,"non-Windows Diligent profile requires Vulkan"});
#endif
    if (!config.headless && !native.valid()) return BackendResult::failure({foundation::ErrorCode::InvalidArgument,"Diligent requires a native window"});
    try {
        auto p=std::make_unique<Impl>(config);
#if defined(_WIN32)
        auto* factory=Diligent::GetEngineFactoryD3D12();
        Diligent::EngineD3D12CreateInfo ci{};
#else
        auto* factory=Diligent::GetEngineFactoryVk();
        Diligent::EngineVkCreateInfo ci{};
#endif
        if (!factory) return BackendResult::failure({foundation::ErrorCode::Internal,"Diligent factory unavailable"});
        factory->SetBreakOnError(false);ci.EnableValidation=config.validation;
#if defined(_WIN32)
        factory->CreateDeviceAndContextsD3D12(ci,&p->device,&p->context);
#else
        factory->CreateDeviceAndContextsVk(ci,&p->device,&p->context);
#endif
        if (!p->device || !p->context) return BackendResult::failure({foundation::ErrorCode::Internal,"Diligent device/context creation failed"});
        if (!config.headless) {
            Diligent::NativeWindow w{};
#if PLATFORM_WIN32
            if (native.system!=foundation::NativeWindowSystem::Win32) return BackendResult::failure({foundation::ErrorCode::Unsupported,"D3D12 requires Win32"});
            w.hWnd=native.window;
#elif PLATFORM_LINUX
            if (native.system!=foundation::NativeWindowSystem::X11) return BackendResult::failure({foundation::ErrorCode::Unsupported,"current Vulkan SDL integration requires X11"});
            w.pDisplay=native.display;w.WindowId=static_cast<Diligent::Uint32>(native.window_id);
#else
            return BackendResult::failure({foundation::ErrorCode::Unsupported,"native Diligent window integration is not available on this platform"});
#endif
            Diligent::SwapChainDesc desc{};desc.Width=config.width;desc.Height=config.height;
            desc.BufferCount=config.frames_in_flight;desc.ColorBufferFormat=Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
            desc.DepthBufferFormat=Diligent::TEX_FORMAT_D32_FLOAT;
#if defined(_WIN32)
            factory->CreateSwapChainD3D12(p->device,p->context,desc,Diligent::FullScreenModeDesc{},w,&p->swap);
#else
            factory->CreateSwapChainVk(p->device,p->context,desc,w,&p->swap);
#endif
            if (!p->swap) return BackendResult::failure({foundation::ErrorCode::Internal,"Diligent swap chain creation failed"});
            if (auto r=p->initializeResources();!r) return BackendResult::failure(r.error());
        }
        p->caps.initialized=true;
        std::cout<<"Genomes Diligent renderer: "<<(config.backend==RenderBackendKind::D3D12?"D3D12":"Vulkan")
                 <<"; native meshes + skinning + direct PBR + key shadows; sRGB target\n";
        return BackendResult::success(std::unique_ptr<DiligentBackend>{new DiligentBackend{std::move(p)}});
    } catch (...) { return BackendResult::failure({foundation::ErrorCode::Internal,"Diligent initialization exception"}); }
}
DiligentBackend::DiligentBackend(std::unique_ptr<Impl> p) noexcept:impl_(std::move(p)) {}
DiligentBackend::~DiligentBackend() { shutdown(); }
RenderCapabilities DiligentBackend::capabilities() const noexcept { return impl_?impl_->caps:RenderCapabilities{}; }
RenderUploadTelemetry DiligentBackend::uploadTelemetry() const noexcept { return impl_?impl_->telemetry:RenderUploadTelemetry{}; }
std::pair<std::uint32_t,std::uint32_t> DiligentBackend::framebufferSize() const noexcept {
    if (!impl_) return {0,0};
    if (impl_->swap) return {impl_->swap->GetDesc().Width,impl_->swap->GetDesc().Height};
    return {impl_->config.width,impl_->config.height};
}
void DiligentBackend::set_camera(const RenderCamera& camera) noexcept {
    if (impl_) { impl_->camera_override=camera;impl_->have_camera_override=camera.enabled; }
}
RenderResult DiligentBackend::begin_frame() noexcept {
    if (!impl_ || !impl_->caps.initialized || impl_->open) return error("Diligent begin in invalid state");
    try {
        auto& p=*impl_;
        ++p.telemetry.frame;p.telemetry.mesh_uploads=0;p.telemetry.mesh_upload_bytes=0;p.telemetry.palette_updates=0;
        p.telemetry.draw_calls=0;p.telemetry.ui_draw_calls=0;p.telemetry.ui_texture_uploads=0;
        p.telemetry.ui_texture_upload_bytes=0;p.telemetry.ui_buffer_grows=0;
        p.prepared=p.drawn_meshes=p.drawn_instances=p.have_camera_override=false;p.items.clear();
        if (p.swap) {
            auto* target=p.swap->GetCurrentBackBufferRTV();
            if (!target) return error("missing swap-chain back buffer");
            p.context->SetRenderTargets(1U,&target,p.depth_view,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            const float clear[]{.025F,.035F,.055F,1};
            p.context->ClearRenderTarget(target,clear,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            p.context->ClearDepthStencil(p.depth_view,Diligent::CLEAR_DEPTH_FLAG,1.0F,0,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
        p.open=true;return RenderResult::success();
    } catch (...) { return error("Diligent begin exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::draw_meshes(const PresentationSnapshot& snapshot) noexcept {
    if (!impl_ || !impl_->open) return error("Diligent draw outside frame");
    if (!impl_->swap || impl_->drawn_meshes) return RenderResult::success();
    try {
        if (auto r=impl_->prepare(snapshot);!r) return r;
        // A single scene pass orders direct meshes and instances together, including transparent ranges.
        if (auto r=impl_->renderItems(false,false);!r) return r;
        impl_->drawn_meshes=true;return RenderResult::success();
    } catch (...) { return error("Diligent scene draw exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::draw_instances(const PresentationSnapshot& snapshot) noexcept {
    if (!impl_ || !impl_->open) return error("Diligent draw outside frame");
    if (!impl_->swap || impl_->drawn_instances) return RenderResult::success();
    try {
        if (auto r=draw_meshes(snapshot);!r) return r;
        if (auto r=impl_->renderDebug(snapshot);!r) return r;
        impl_->drawn_instances=true;return RenderResult::success();
    } catch (...) { return error("Diligent debug draw exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::draw_ui(const ui::UiRenderFrame& frame) noexcept {
    if (!impl_ || !impl_->open) return error("Diligent UI outside frame");
    if (!impl_->swap) return RenderResult::success();
    try { return impl_->renderUi(frame); }
    catch (...) { return error("Diligent UI exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::abort_frame() noexcept {
    if (!impl_ || !impl_->open) return error("Diligent abort outside frame");
    impl_->open=false;impl_->pending_capture.reset();impl_->prepared=false;
    try {
        impl_->context->SetRenderTargets(0U,nullptr,nullptr,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->context->Flush();impl_->context->FinishFrame();impl_->items.clear();
        return RenderResult::success();
    } catch (...) { return error("Diligent abort exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::end_frame() noexcept {
    if (!impl_ || !impl_->open) return error("Diligent end outside frame");
    try {
        if (impl_->pending_capture) {
            if (auto r=impl_->writeCapture();!r) { (void)abort_frame();return r; }
        }
        impl_->context->Flush();
        if (impl_->swap) impl_->swap->Present(1U);else impl_->context->FinishFrame();
        impl_->open=false;impl_->prune();return RenderResult::success();
    } catch (...) {
        if (impl_->open) (void)abort_frame();
        return error("Diligent end exception",foundation::ErrorCode::Internal);
    }
}
RenderResult DiligentBackend::resize(std::uint32_t width,std::uint32_t height) noexcept {
    if (!impl_ || impl_->open || width==0 || height==0 || width>16384U || height>16384U) return error("invalid Diligent resize");
    try {
        if (impl_->swap) {
            impl_->context->SetRenderTargets(0,nullptr,nullptr,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            impl_->context->WaitForIdle();impl_->swap->Resize(width,height);
            const auto& d=impl_->swap->GetDesc();
            if (auto r=impl_->resizeDepth(d.Width,d.Height);!r) return r;
        }
        impl_->config.width=width;impl_->config.height=height;return RenderResult::success();
    } catch (...) { return error("Diligent resize exception",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::wait_idle() noexcept {
    if (!impl_ || !impl_->caps.initialized) return error("Diligent is shut down");
    try { impl_->context->WaitForIdle();return RenderResult::success(); }
    catch (...) { return error("Diligent wait exception",foundation::ErrorCode::Internal); }
}
void DiligentBackend::shutdown() noexcept {
    if (!impl_) return;
    try {
        if (impl_->open) (void)abort_frame();
        if (impl_->context) { impl_->context->Flush();impl_->context->WaitForIdle(); }
    } catch (...) {}
    // PImpl member order keeps device/context alive until all resources have been released.
    impl_.reset();
}
} // namespace genomes::render
