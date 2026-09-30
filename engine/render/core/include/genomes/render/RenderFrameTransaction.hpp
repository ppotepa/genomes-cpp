#pragma once
#include <genomes/render/RenderBackend.hpp>

namespace genomes::render {

// Close every successful begin, including failed submissions. Preserve the first error.
class RenderFrameTransaction final {
public:
    [[nodiscard]] bool open() const noexcept { return open_; }
    [[nodiscard]] bool healthy() const noexcept { return !failed_; }
    [[nodiscard]] foundation::Error error() const noexcept { return error_; }
    template<class Begin> void begin(Begin&& operation) {
        if (open_) { fail({foundation::ErrorCode::InvalidState, "frame already open"}); return; }
        failed_ = false; error_ = {};
        try { const auto r = operation(); if (!r) fail(r.error()); else open_ = true; }
        catch (...) { fail({foundation::ErrorCode::Internal, "frame begin exception"}); }
    }
    template<class Submit> void submit(Submit&& operation) {
        if (!open_) { fail({foundation::ErrorCode::InvalidState, "submit outside frame"}); return; }
        if (failed_) return;
        try { const auto r = operation(); if (!r) fail(r.error()); }
        catch (...) { fail({foundation::ErrorCode::Internal, "frame submit exception"}); }
    }
    template<class Present, class Abort> void end(Present&& present, Abort&& abort) {
        if (!open_) return;
        open_ = false;
        try { const auto r = failed_ ? abort() : present(); if (!r) fail(r.error()); }
        catch (...) { fail({foundation::ErrorCode::Internal, "frame close exception"}); }
    }
private:
    void fail(foundation::Error e) noexcept { if (!failed_) error_ = e; failed_ = true; }
    bool open_{false}, failed_{false};
    foundation::Error error_{};
};
} // namespace genomes::render
