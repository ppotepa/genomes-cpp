#include "DiligentBackendImpl.hpp"
#if !defined(_WIN32)
#error "The Genomes Diligent production backend is Windows/D3D12 only"
#endif
#include <d3d12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <utility>

namespace genomes::render {
using namespace diligent_detail;
using BackendResult=foundation::Result<std::unique_ptr<DiligentBackend>,foundation::Error>;
namespace {
[[nodiscard]] bool d3d12DredRequested() noexcept {
    // Breadcrumb tracking is opt-in outside validation builds to avoid runtime overhead.
    char* value = nullptr;
    std::size_t value_size = 0U;
    if (_dupenv_s(&value, &value_size, "GENOMES_D3D12_DRED") != 0) {
        return false;
    }
    const bool requested = value != nullptr && value[0] == '1' && value[1] == '\0';
    std::free(value);
    return requested;
}

void enableD3D12Dred() noexcept {
    ID3D12DeviceRemovedExtendedDataSettings* settings = nullptr;
    if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&settings)))) {
        std::cerr << "D3D12 DRED requested, but debug settings are unavailable\n";
        return;
    }
    settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    settings->Release();
    std::cerr << "D3D12 DRED breadcrumbs and page-fault tracking enabled\n";
}

[[nodiscard]] HRESULT d3d12DeviceRemovedReason(Diligent::IRenderDevice* device) noexcept {
    Diligent::RefCntAutoPtr<Diligent::IRenderDeviceD3D12> device_d3d12{
        device, Diligent::IID_RenderDeviceD3D12};
    if (!device_d3d12 || device_d3d12->GetD3D12Device() == nullptr) return E_NOINTERFACE;
    return device_d3d12->GetD3D12Device()->GetDeviceRemovedReason();
}

void logD3D12DeviceRemoved(const char* operation, HRESULT reason,
                           Diligent::IRenderDevice* device) noexcept {
    std::cerr << "D3D12 device removed during " << operation
              << "; GetDeviceRemovedReason=0x" << std::hex
              << static_cast<unsigned long>(reason) << std::dec << '\n';

    Diligent::RefCntAutoPtr<Diligent::IRenderDeviceD3D12> device_d3d12{
        device, Diligent::IID_RenderDeviceD3D12};
    ID3D12Device* native_device = device_d3d12 ? device_d3d12->GetD3D12Device() : nullptr;
    if (native_device == nullptr) return;

    const LUID adapter_luid = native_device->GetAdapterLuid();
    std::cerr << "D3D12 device adapter LUID=0x" << std::hex
              << static_cast<unsigned long>(adapter_luid.HighPart)
              << ':' << static_cast<unsigned long>(adapter_luid.LowPart)
              << std::dec << '\n';

    ID3D12DeviceRemovedExtendedData1* dred = nullptr;
    if (FAILED(native_device->QueryInterface(IID_PPV_ARGS(&dred)))) {
        std::cerr << "D3D12 DRED data unavailable\n";
        return;
    }

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
    const HRESULT breadcrumbs_result = dred->GetAutoBreadcrumbsOutput1(&breadcrumbs);
    if (SUCCEEDED(breadcrumbs_result)) {
        std::size_t logged_nodes = 0U;
        const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
        for (; node != nullptr && logged_nodes < 16U;
             node = node->pNext, ++logged_nodes) {
            const UINT completed = node->pLastBreadcrumbValue != nullptr
                ? *node->pLastBreadcrumbValue : 0U;
            std::cerr << "D3D12 DRED breadcrumb: queue="
                      << (node->pCommandQueueDebugNameA != nullptr
                              ? node->pCommandQueueDebugNameA : "<unnamed>")
                      << ", command_list="
                      << (node->pCommandListDebugNameA != nullptr
                              ? node->pCommandListDebugNameA : "<unnamed>")
                      << ", completed=" << completed << '/' << node->BreadcrumbCount;
            if (node->pCommandHistory != nullptr && completed < node->BreadcrumbCount) {
                std::cerr << ", next_operation="
                          << static_cast<unsigned int>(node->pCommandHistory[completed]);
            }
            std::cerr << '\n';
        }
        if (node != nullptr) {
            std::cerr << "D3D12 DRED breadcrumb output truncated after 16 command lists\n";
        }
    } else {
        std::cerr << "D3D12 DRED breadcrumbs unavailable; HRESULT=0x" << std::hex
                  << static_cast<unsigned long>(breadcrumbs_result) << std::dec << '\n';
    }

    D3D12_DRED_PAGE_FAULT_OUTPUT1 page_fault{};
    const HRESULT page_fault_result = dred->GetPageFaultAllocationOutput1(&page_fault);
    if (FAILED(page_fault_result)) {
        std::cerr << "D3D12 DRED page-fault data unavailable; HRESULT=0x" << std::hex
                  << static_cast<unsigned long>(page_fault_result) << std::dec << '\n';
    } else if (page_fault.PageFaultVA != 0U) {
        std::cerr << "D3D12 DRED page fault VA=0x" << std::hex
                  << static_cast<unsigned long long>(page_fault.PageFaultVA) << std::dec;
        if (page_fault.pHeadExistingAllocationNode != nullptr) {
            const char* name = page_fault.pHeadExistingAllocationNode->ObjectNameA;
            std::cerr << ", existing_allocation=" << (name != nullptr ? name : "<unnamed>");
        }
        if (page_fault.pHeadRecentFreedAllocationNode != nullptr) {
            const char* name = page_fault.pHeadRecentFreedAllocationNode->ObjectNameA;
            std::cerr << ", recently_freed_allocation="
                      << (name != nullptr ? name : "<unnamed>");
        }
        std::cerr << '\n';
    }
    dred->Release();
}

template <class BackendImpl>
void markDeviceLost(BackendImpl& backend, const char* operation,
                    HRESULT reason) noexcept {
    backend.open = false;
    backend.active_fence = 0U;
    backend.pending_capture.reset();
    backend.prepared = false;
    backend.health.state = RendererHealthState::DeviceLost;
    backend.health.error = {foundation::ErrorCode::Internal,
                            "D3D12 device removed during GPU work"};
    logD3D12DeviceRemoved(operation, reason, backend.device);
}

template <class BackendImpl>
[[nodiscard]] bool markDeviceLostIfRemoved(BackendImpl& backend,
                                           const char* operation) noexcept {
    const HRESULT reason = d3d12DeviceRemovedReason(backend.device);
    if (SUCCEEDED(reason)) return false;
    markDeviceLost(backend, operation, reason);
    return true;
}

template <class BackendImpl>
[[nodiscard]] RenderResult classifyD3D12Failure(BackendImpl& backend,
                                                RenderResult result,
                                                const char* operation) noexcept {
    if (result || !markDeviceLostIfRemoved(backend, operation)) return result;
    return error("D3D12 device removed during GPU work", foundation::ErrorCode::Internal);
}

template <class BackendImpl>
void markBackendFailure(BackendImpl& backend, const char* operation,
                        const char* message) noexcept {
    if (markDeviceLostIfRemoved(backend, operation)) return;
    backend.open = false;
    backend.active_fence = 0U;
    backend.pending_capture.reset();
    backend.health.state = RendererHealthState::DeviceLost;
    backend.health.error = {foundation::ErrorCode::Internal, message};
}
} // namespace
BackendResult DiligentBackend::create(RenderConfig config) { return create(config,{}); }
BackendResult DiligentBackend::create(RenderConfig config,foundation::NativeWindowHandle native) {
    if (!config.valid()) return BackendResult::failure({foundation::ErrorCode::InvalidArgument,"invalid Diligent configuration"});
    if (config.backend!=RenderBackendKind::D3D12) return BackendResult::failure({foundation::ErrorCode::Unsupported,"Windows Diligent profile requires D3D12"});
    if (!config.headless && !native.valid()) return BackendResult::failure({foundation::ErrorCode::InvalidArgument,"Diligent requires a native window"});
    try {
        auto p=std::make_unique<Impl>(config);
        auto* factory=Diligent::GetEngineFactoryD3D12();
        Diligent::EngineD3D12CreateInfo ci{};
        if (!factory) return BackendResult::failure({foundation::ErrorCode::Internal,"Diligent factory unavailable"});
        if (config.validation || d3d12DredRequested()) enableD3D12Dred();
        factory->SetBreakOnError(false);ci.EnableValidation=config.validation;
        factory->CreateDeviceAndContextsD3D12(ci,&p->device,&p->context);
        if (!p->device || !p->context) return BackendResult::failure({foundation::ErrorCode::Internal,"Diligent device/context creation failed"});
        if (!config.headless) {
            Diligent::NativeWindow w{};
            if (native.system!=foundation::NativeWindowSystem::Win32) return BackendResult::failure({foundation::ErrorCode::Unsupported,"D3D12 requires Win32"});
            w.hWnd=native.window;
            Diligent::SwapChainDesc desc{};desc.Width=config.width;desc.Height=config.height;
            desc.BufferCount=config.frames_in_flight;desc.ColorBufferFormat=Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
            desc.DepthBufferFormat=Diligent::TEX_FORMAT_D32_FLOAT;
            factory->CreateSwapChainD3D12(p->device,p->context,desc,Diligent::FullScreenModeDesc{},w,&p->swap);
            if (!p->swap) return BackendResult::failure({foundation::ErrorCode::Internal,"Diligent swap chain creation failed"});
            if (auto r=p->initializeResources();!r) return BackendResult::failure(r.error());
        }
        p->caps.initialized=true;
        std::cout<<"Genomes Diligent renderer: D3D12"
                 <<"; native meshes + skinning + direct PBR + key shadows; sRGB target\n";
        return BackendResult::success(std::unique_ptr<DiligentBackend>{new DiligentBackend{std::move(p)}});
    } catch (...) { return BackendResult::failure({foundation::ErrorCode::Internal,"Diligent initialization exception"}); }
}
DiligentBackend::DiligentBackend(std::unique_ptr<Impl> p) noexcept:impl_(std::move(p)) {}
DiligentBackend::~DiligentBackend() { shutdown(); }
RenderCapabilities DiligentBackend::capabilities() const noexcept { return impl_?impl_->caps:RenderCapabilities{}; }
RenderUploadTelemetry DiligentBackend::uploadTelemetry() const noexcept { return impl_?impl_->telemetry:RenderUploadTelemetry{}; }
RendererHealthState DiligentBackend::healthState() const noexcept {
    return impl_ ? impl_->health.state : last_health_.state;
}
RendererHealthDiagnostics DiligentBackend::healthDiagnostics() const noexcept {
    return impl_ ? impl_->health : last_health_;
}
std::pair<std::uint32_t,std::uint32_t> DiligentBackend::framebufferSize() const noexcept {
    if (!impl_) return {0,0};
    // GetDesc() is a backend call, so keep it on the logical RenderLane.
    // Non-owner diagnostics still receive the immutable configured extent.
    if (impl_->swap && impl_->render_lane.ownsCurrentThread())
        return {impl_->swap->GetDesc().Width,impl_->swap->GetDesc().Height};
    return {impl_->config.width,impl_->config.height};
}
void DiligentBackend::set_resolved_camera(const camera::ResolvedCamera& camera) noexcept {
    if (impl_ && impl_->render_lane.ownsCurrentThread()) {
        impl_->resolved_camera=camera;impl_->have_resolved_camera=true;
    }
}
RenderResult DiligentBackend::begin_frame() noexcept {
    if (impl_ && !impl_->render_lane.ownsCurrentThread()) return error("Diligent begin outside render lane");
    if (impl_ && (impl_->health.state==RendererHealthState::DeviceLost ||
                  impl_->health.state==RendererHealthState::Stopped))
        return error("Diligent renderer is not healthy",foundation::ErrorCode::Internal);
    if (!impl_ || !impl_->caps.initialized || impl_->open) return error("Diligent begin in invalid state");
    try {
        auto& p=*impl_;
        if (markDeviceLostIfRemoved(p, "before begin_frame")) {
            return error("D3D12 device was already removed before begin_frame",
                         foundation::ErrorCode::Internal);
        }
        p.active_fence=p.next_fence;
        p.retireCompleted();
        ++p.telemetry.frame;p.telemetry.mesh_uploads=0;p.telemetry.mesh_upload_bytes=0;p.telemetry.palette_updates=0;
        p.telemetry.draw_calls=0;p.telemetry.ui_draw_calls=0;p.telemetry.ui_texture_uploads=0;
        p.telemetry.ui_texture_upload_bytes=0;p.telemetry.ui_buffer_grows=0;
        p.prepared=p.drawn_meshes=p.drawn_instances=false;p.have_resolved_camera=false;p.items.clear();
        if (p.swap) {
            auto* target=p.swap->GetCurrentBackBufferRTV();
            if (!target)
                return classifyD3D12Failure(
                    p, error("missing swap-chain back buffer"), "back-buffer acquisition");
            p.context->SetRenderTargets(1U,&target,p.depth_view,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            const float clear[]{.025F,.035F,.055F,1};
            p.context->ClearRenderTarget(target,clear,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            p.context->ClearDepthStencil(p.depth_view,Diligent::CLEAR_DEPTH_FLAG,1.0F,0,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
        p.open=true;p.health.state=RendererHealthState::Healthy;return RenderResult::success();
    } catch (...) {
        markBackendFailure(*impl_, "begin_frame exception",
                           "Diligent begin/device exception");
        return error("Diligent begin/device exception",foundation::ErrorCode::Internal);
    }
}
RenderResult DiligentBackend::draw_meshes(const PresentationSnapshot& snapshot) noexcept {
    if (impl_ && !impl_->render_lane.ownsCurrentThread()) return error("Diligent draw outside render lane");
    if (!impl_ || !impl_->open) return error("Diligent draw outside frame");
    if (!impl_->swap || impl_->drawn_meshes) return RenderResult::success();
    try {
        if (auto r=impl_->prepare(snapshot);!r)
            return classifyD3D12Failure(*impl_, std::move(r), "scene preparation");
        // A single scene pass orders direct meshes and instances together, including transparent ranges.
        if (auto r=impl_->renderItems(false,false);!r)
            return classifyD3D12Failure(*impl_, std::move(r), "scene draw");
        impl_->drawn_meshes=true;return RenderResult::success();
    } catch (...) {
        markBackendFailure(*impl_, "scene draw exception",
                           "Diligent scene draw/device exception");
        return error("Diligent scene draw/device exception",foundation::ErrorCode::Internal);
    }
}
RenderResult DiligentBackend::draw_instances(const PresentationSnapshot& snapshot) noexcept {
    if (impl_ && !impl_->render_lane.ownsCurrentThread()) return error("Diligent draw outside render lane");
    if (!impl_ || !impl_->open) return error("Diligent draw outside frame");
    if (!impl_->swap || impl_->drawn_instances) return RenderResult::success();
    try {
        if (auto r=draw_meshes(snapshot);!r) return r;
        if (auto r=impl_->renderDebug(snapshot);!r)
            return classifyD3D12Failure(*impl_, std::move(r), "debug draw");
        impl_->drawn_instances=true;return RenderResult::success();
    } catch (...) {
        markBackendFailure(*impl_, "debug draw exception",
                           "Diligent debug draw/device exception");
        return error("Diligent debug draw/device exception",foundation::ErrorCode::Internal);
    }
}
RenderResult DiligentBackend::draw_ui(const ui::UiRenderFrame& frame) noexcept {
    if (impl_ && !impl_->render_lane.ownsCurrentThread()) return error("Diligent UI outside render lane");
    if (!impl_ || !impl_->open) return error("Diligent UI outside frame");
    if (!impl_->swap) return RenderResult::success();
    try {
        if (auto r=impl_->renderUi(frame);!r)
            return classifyD3D12Failure(*impl_, std::move(r), "UI draw");
        return RenderResult::success();
    }
    catch (...) {
        markBackendFailure(*impl_, "UI draw exception", "Diligent UI/device exception");
        return error("Diligent UI/device exception",foundation::ErrorCode::Internal);
    }
}
RenderResult DiligentBackend::abort_frame() noexcept {
    if (impl_ && !impl_->render_lane.ownsCurrentThread()) return error("Diligent abort outside render lane");
    if (impl_ && impl_->health.state==RendererHealthState::DeviceLost)
        return error("Diligent device is lost",foundation::ErrorCode::Internal);
    if (!impl_ || !impl_->open) return error("Diligent abort outside frame");
    if (markDeviceLostIfRemoved(*impl_, "before frame abort"))
        return error("D3D12 device removed before frame abort", foundation::ErrorCode::Internal);
    impl_->open=false;impl_->pending_capture.reset();impl_->prepared=false;
    try {
        if (impl_->frame_fence) {
            impl_->context->EnqueueSignal(impl_->frame_fence,impl_->active_fence);
            impl_->last_submitted_fence=impl_->active_fence;
            impl_->next_fence=impl_->active_fence+1U;
        }
        impl_->context->SetRenderTargets(0U,nullptr,nullptr,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->context->Flush();impl_->context->FinishFrame();impl_->active_fence=0U;impl_->items.clear();
        impl_->health.state=RendererHealthState::FrameAborted;
        impl_->health.error={foundation::ErrorCode::InvalidState,"Diligent frame aborted"};
        return RenderResult::success();
    } catch (...) {
        markBackendFailure(*impl_, "frame abort exception",
                           "Diligent abort/device exception");
        return error("Diligent abort/device exception",foundation::ErrorCode::Internal);
    }
}
RenderResult DiligentBackend::end_frame() noexcept {
    if (impl_ && !impl_->render_lane.ownsCurrentThread()) return error("Diligent end outside render lane");
    if (impl_ && impl_->health.state==RendererHealthState::DeviceLost)
        return error("Diligent device is lost",foundation::ErrorCode::Internal);
    if (!impl_ || !impl_->open) return error("Diligent end outside frame");
    try {
        if (impl_->pending_capture) {
            if (auto r=impl_->writeCapture();!r) {
                auto failure = classifyD3D12Failure(*impl_, std::move(r), "frame capture");
                (void)abort_frame();
                return failure;
            }
        }
        if (impl_->frame_fence) {
            impl_->context->EnqueueSignal(impl_->frame_fence,impl_->active_fence);
            impl_->last_submitted_fence=impl_->active_fence;
            impl_->next_fence=impl_->active_fence+1U;
        }
        impl_->context->Flush();
        if (impl_->swap) {
            if (markDeviceLostIfRemoved(*impl_, "Flush before Present")) {
                return error("D3D12 device removed after Flush and before Present",
                             foundation::ErrorCode::Internal);
            }
            impl_->swap->Present(1U);
            const HRESULT after_present = d3d12DeviceRemovedReason(impl_->device);
            if (FAILED(after_present)) {
                markDeviceLost(*impl_, "Present", after_present);
                return error("D3D12 device removed after Present",
                             foundation::ErrorCode::Internal);
            }
        } else {
            impl_->context->FinishFrame();
        }
        impl_->open=false;impl_->active_fence=0U;impl_->prune();
        impl_->health.state=RendererHealthState::Healthy;
        impl_->health.last_successful_frame=impl_->telemetry.frame;
        impl_->health.last_present_frame=impl_->telemetry.frame;
        impl_->health.last_submitted_fence=impl_->last_submitted_fence;
        return RenderResult::success();
    } catch (...) {
        // Present/device failures make all further Present and WaitForIdle calls unsafe.
        markBackendFailure(*impl_, "frame end exception",
                           "Diligent Present/device exception");
        return error("Diligent Present/device exception",foundation::ErrorCode::Internal);
    }
}
RenderResult DiligentBackend::resize(std::uint32_t width,std::uint32_t height) noexcept {
    if (impl_ && !impl_->render_lane.ownsCurrentThread()) return error("Diligent resize outside render lane");
    if (impl_ && impl_->health.state==RendererHealthState::DeviceLost)
        return error("Diligent device is lost",foundation::ErrorCode::Internal);
    if (!impl_ || impl_->open || width==0 || height==0 || width>16384U || height>16384U) return error("invalid Diligent resize");
    try {
        if (impl_->swap) {
            if (markDeviceLostIfRemoved(*impl_, "before resize"))
                return error("D3D12 device removed before resize", foundation::ErrorCode::Internal);
            impl_->context->SetRenderTargets(0,nullptr,nullptr,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            impl_->context->WaitForIdle();impl_->swap->Resize(width,height);
            const auto& d=impl_->swap->GetDesc();
            if (auto r=impl_->resizeDepth(d.Width,d.Height);!r)
                return classifyD3D12Failure(*impl_, std::move(r), "depth resize");
        }
        impl_->config.width=width;impl_->config.height=height;return RenderResult::success();
    } catch (...) {
        markBackendFailure(*impl_, "resize exception",
                           "Diligent resize/device exception");
        return error("Diligent resize/device exception",foundation::ErrorCode::Internal);
    }
}
RenderResult DiligentBackend::wait_idle() noexcept {
    if (impl_ && !impl_->render_lane.ownsCurrentThread()) return error("Diligent wait outside render lane");
    if (impl_ && (impl_->health.state==RendererHealthState::DeviceLost ||
                  impl_->health.state==RendererHealthState::Stopped))
        return error("Diligent wait suppressed after device loss",foundation::ErrorCode::Internal);
    if (!impl_ || !impl_->caps.initialized) return error("Diligent is shut down");
    if (markDeviceLostIfRemoved(*impl_, "before wait_idle"))
        return error("D3D12 device removed before wait_idle", foundation::ErrorCode::Internal);
    try { impl_->context->WaitForIdle();return RenderResult::success(); }
    catch (...) {
        markBackendFailure(*impl_, "wait_idle exception",
                           "Diligent wait/device exception");
        return error("Diligent wait exception",foundation::ErrorCode::Internal);
    }
}
void DiligentBackend::shutdown() noexcept {
    if (!impl_) return;
    if (impl_->render_lane.ownsCurrentThread() &&
        impl_->health.state!=RendererHealthState::DeviceLost)
        (void)markDeviceLostIfRemoved(*impl_, "before shutdown");
    const bool safe_gpu_shutdown=impl_->render_lane.ownsCurrentThread() &&
                                 impl_->health.state!=RendererHealthState::DeviceLost;
    try {
        if (safe_gpu_shutdown && impl_->open) (void)abort_frame();
        if (safe_gpu_shutdown && impl_->context) {
            impl_->context->Flush();impl_->context->WaitForIdle();
        }
    } catch (...) {}
    last_health_=impl_->health;
    last_health_.state=RendererHealthState::Stopped;
    // PImpl member order keeps device/context alive until all resources have been released.
    impl_.reset();
}
} // namespace genomes::render
