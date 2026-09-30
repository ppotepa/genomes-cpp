#include "DiligentBackendImpl.hpp"
#include <DiligentTools/TextureLoader/interface/Image.h>
#include <DiligentCore/Primitives/interface/DataBlob.h>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>

#ifndef GENOMES_DILIGENT_BUILD_SHA
#define GENOMES_DILIGENT_BUILD_SHA "unknown"
#endif

namespace genomes::render {
using namespace diligent_detail;
namespace {
// Mapping belongs to one capture only. Never retain a mapped pointer across frames.
struct TextureReadLease final {
    Diligent::IDeviceContext* context;
    Diligent::ITexture* texture;
    bool mapped{false};
    ~TextureReadLease() {
        if (mapped) context->UnmapTextureSubresource(texture,0U,0U);
    }
};
bool writeBytes(const std::filesystem::path& path,const void* data,std::size_t size) {
    if (!data || size>static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) return false;
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    if (!stream) return false;
    stream.write(static_cast<const char*>(data),static_cast<std::streamsize>(size));
    stream.flush();
    if (!stream.good()) return false;
    stream.close();
    return !stream.fail();
}
}
RenderResult DiligentBackend::capture(const std::filesystem::path& path) noexcept {
    if (!impl_ || !impl_->caps.initialized || !impl_->swap) return error("capture requires an initialized windowed Diligent renderer");
    try {
        std::string extension=path.extension().string();
        for (char& c:extension) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (path.empty() || extension!=".png") return error("Diligent captures require a .png path",foundation::ErrorCode::InvalidArgument);
        if (impl_->pending_capture) return error("a Diligent capture is already queued");
        std::error_code ec;
        auto metadata=path;metadata+=".json";
        if (std::filesystem::exists(path,ec) || ec || std::filesystem::exists(metadata,ec) || ec)
            return error("capture destination already exists or cannot be inspected; use a fresh evidence path");
        impl_->pending_capture=path;
        return RenderResult::success();
    } catch (...) { return error("could not queue capture",foundation::ErrorCode::Internal); }
}
RenderResult DiligentBackend::Impl::writeCapture() {
    if (!pending_capture || !swap || !open) return error("invalid capture frame state");
    const auto destination=*pending_capture;
    auto metadata_path=destination;metadata_path+=".json";
    auto png_temp=destination;png_temp+=".genomes-pending";
    auto json_temp=metadata_path;json_temp+=".genomes-pending";
    const auto cleanup=[&]() noexcept {
        std::error_code ignored;
        std::filesystem::remove(png_temp,ignored);
        std::filesystem::remove(json_temp,ignored);
    };
    auto* view=swap->GetCurrentBackBufferRTV();
    if (!view) return error("capture back buffer is missing");
    auto* source=view->GetTexture();
    if (!source) return error("capture back buffer texture is missing");
    const auto source_desc=source->GetDesc();
    if (source_desc.SampleCount!=1U || source_desc.Width==0U || source_desc.Height==0U)
        return error("unsupported capture sample count or dimensions");
    const auto format=source_desc.Format;
    if (format!=Diligent::TEX_FORMAT_RGBA8_UNORM && format!=Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB &&
        format!=Diligent::TEX_FORMAT_BGRA8_UNORM && format!=Diligent::TEX_FORMAT_BGRA8_UNORM_SRGB)
        return error("capture requires an RGBA8/BGRA8 swap chain",foundation::ErrorCode::Unsupported);
    Diligent::TextureDesc desc{};
    desc.Name="Genomes capture staging";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width=source_desc.Width;desc.Height=source_desc.Height;desc.Format=format;
    desc.Usage=Diligent::USAGE_STAGING;desc.BindFlags=Diligent::BIND_NONE;desc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;
    Ptr<Diligent::ITexture> staging;device->CreateTexture(desc,nullptr,&staging);
    if (!staging) return error("capture staging allocation failed",foundation::ErrorCode::Internal);
    context->SetRenderTargets(0U,nullptr,nullptr,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::CopyTextureAttribs copy{};
    copy.pSrcTexture=source;copy.SrcTextureTransitionMode=Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copy.pDstTexture=staging;copy.DstTextureTransitionMode=Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    context->CopyTexture(copy);
    // Deliberate synchronous evidence path. Normal rendering never waits for a readback.
    context->Flush();context->WaitForIdle();
    const std::size_t row_bytes=static_cast<std::size_t>(desc.Width)*4U;
    if (desc.Height>std::numeric_limits<std::size_t>::max()/row_bytes) return error("capture size overflow");
    std::vector<std::uint8_t> pixels(row_bytes*desc.Height);
    {
        TextureReadLease lease{context,staging,false};
        Diligent::MappedTextureSubresource mapped{};
        context->MapTextureSubresource(staging,0U,0U,Diligent::MAP_READ,Diligent::MAP_FLAG_DO_NOT_WAIT,nullptr,mapped);
        lease.mapped=mapped.pData!=nullptr;
        if (!lease.mapped || mapped.Stride<row_bytes || mapped.Stride>std::numeric_limits<std::size_t>::max()/desc.Height)
            return error("capture staging map failed or has invalid row pitch",foundation::ErrorCode::Internal);
        const auto* bytes=static_cast<const std::uint8_t*>(mapped.pData);
        for (std::size_t y=0;y<desc.Height;++y)
            std::memcpy(pixels.data()+y*row_bytes,bytes+y*static_cast<std::size_t>(mapped.Stride),row_bytes);
    }
    Diligent::Image::EncodeInfo encode{};
    encode.Width=desc.Width;encode.Height=desc.Height;encode.TexFormat=format;
    encode.KeepAlpha=true;encode.FlipY=false;encode.pData=pixels.data();
    encode.Stride=static_cast<Diligent::Uint32>(row_bytes);encode.FileFormat=Diligent::IMAGE_FILE_FORMAT_PNG;
    Ptr<Diligent::IDataBlob> encoded;Diligent::Image::Encode(encode,&encoded);
    if (!encoded || !encoded->GetDataPtr() || encoded->GetSize()==0U)
        return error("Diligent PNG encoding failed",foundation::ErrorCode::Internal);
    std::ostringstream metadata;
    metadata<<"{\n\"schema\":1,\"configured_git_sha\":\""<<GENOMES_DILIGENT_BUILD_SHA
            <<"\",\"backend\":\""<<(config.backend==RenderBackendKind::D3D12?"D3D12":"Vulkan")
            <<"\",\"frame\":"<<telemetry.frame<<",\"width\":"<<desc.Width<<",\"height\":"<<desc.Height
            <<",\"mesh_uploads\":"<<telemetry.mesh_uploads<<",\"mesh_upload_bytes\":"<<telemetry.mesh_upload_bytes
            <<",\"skin_constant_writes\":"<<telemetry.palette_updates<<",\"draw_calls_including_shadows\":"<<telemetry.draw_calls
            <<",\"ui_draw_calls\":"<<telemetry.ui_draw_calls;
    if (!capture_metadata.empty()) metadata<<','<<capture_metadata;
    metadata<<"\n}\n";
    const auto text=metadata.str();
    std::error_code ec;
    if (destination.has_parent_path()) std::filesystem::create_directories(destination.parent_path(),ec);
    if (ec) return error("capture directory creation failed",foundation::ErrorCode::Internal);
    if (std::filesystem::exists(png_temp,ec)||ec||std::filesystem::exists(json_temp,ec)||ec)
        return error("capture temporary files already exist; inspect or remove stale evidence first");
    if (!writeBytes(png_temp,encoded->GetDataPtr(),encoded->GetSize()) || !writeBytes(json_temp,text.data(),text.size())) {
        cleanup();return error("capture file write failed",foundation::ErrorCode::Internal);
    }
    if (std::filesystem::exists(destination,ec)||ec||std::filesystem::exists(metadata_path,ec)||ec) {
        cleanup();return error("capture destination changed while rendering");
    }
    std::filesystem::rename(png_temp,destination,ec);
    if (ec) {cleanup();return error("capture PNG publication failed",foundation::ErrorCode::Internal);}
    std::filesystem::rename(json_temp,metadata_path,ec);
    if (ec) {
        // Do not report a PNG without its provenance as a completed capture.
        std::error_code ignored;std::filesystem::remove(destination,ignored);cleanup();
        return error("capture metadata publication failed",foundation::ErrorCode::Internal);
    }
    pending_capture.reset();return RenderResult::success();
}
} // namespace genomes::render
