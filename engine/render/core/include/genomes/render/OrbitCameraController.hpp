#pragma once
#include <genomes/render/RenderTypes.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace genomes::render {

// Native, Y-up orbit state. Input is filtered by RmlUi and expressed in framebuffer
// pixels. Absolute pointer positions avoid coupling to SDL/threepp delta conventions.
class OrbitCameraController final {
public:
    [[nodiscard]] RenderCamera resolve(const RenderCamera& requested,std::uint32_t width,
                                        std::uint32_t height) noexcept {
        width_=width;height_=height;
        if (!requested.enabled || !requested.valid() || !requested.interactive_orbit) {
            active_=false;drag_=0;return requested;
        }
        if (!active_ || revision_!=requested.revision) {
            current_=requested;revision_=requested.revision;drag_=0;
            const auto delta=sub(current_.position,current_.target);
            radius_=std::sqrt(dot(delta,delta));
            yaw_=std::atan2(delta.x,delta.z);
            pitch_=std::asin(std::clamp(delta.y/radius_,-1.0F,1.0F));
        }
        // Resizing updates the viewport without discarding the user's orbit.
        current_.viewport_left=requested.viewport_left;
        current_.viewport_top=requested.viewport_top;
        current_.viewport_width=requested.viewport_width;
        current_.viewport_height=requested.viewport_height;
        current_.vertical_fov=requested.vertical_fov;
        current_.near_plane=requested.near_plane;
        current_.far_plane=requested.far_plane;
        active_=width>0U && height>0U;
        return current_;
    }
    void input(const ::genomes::input::InputFrame& frame) noexcept {
        using EventType=::genomes::input::EventType;
        if (!active_) {drag_=0;return;}
        for (const auto& event:frame.events) {
            if (event.type==EventType::MouseButtonUp) {
                if (event.mouse_button==drag_) drag_=0;
            } else if (event.type==EventType::MouseButtonDown) {
                if ((event.mouse_button==1 || event.mouse_button==2 || event.mouse_button==3) &&
                    inside(event.x,event.y)) {
                    drag_=event.mouse_button;last_x_=event.x;last_y_=event.y;
                }
            } else if (event.type==EventType::MouseMove && drag_!=0) {
                if (!std::isfinite(event.x)||!std::isfinite(event.y)) continue;
                const float dx=event.x-last_x_,dy=event.y-last_y_;
                last_x_=event.x;last_y_=event.y;
                if (drag_==1) {
                    yaw_-=dx*0.008F;
                    pitch_=std::clamp(pitch_+dy*0.006F,-1.50F,1.50F);
                } else {
                    const auto forward=norm(sub(current_.target,current_.position));
                    const auto right=norm(cross(forward,current_.up));
                    const auto up=norm(cross(right,forward));
                    const float units=2.0F*radius_*std::tan(current_.vertical_fov*0.5F)/
                        std::max(1.0F,static_cast<float>(height_)*current_.viewport_height);
                    current_.target=add(current_.target,
                        add(scale(right,-dx*units),scale(up,dy*units)));
                }
                updateEye();
            } else if (event.type==EventType::MouseWheel && inside(frame.mouse_x,frame.mouse_y)) {
                if (!std::isfinite(event.wheel_y)) continue;
                const float nearest=std::max(0.05F,current_.near_plane*2.0F);
                const float farthest=std::max(nearest,current_.far_plane*0.45F);
                radius_=std::clamp(radius_*std::exp(std::clamp(-event.wheel_y*0.10F,-4.0F,4.0F)),
                                   nearest,farthest);
                updateEye();
            }
        }
    }
    void cancelDrag() noexcept {drag_=0;}
    [[nodiscard]] int dragButton() const noexcept {return drag_;}
private:
    using V=foundation::Vec3;
    static V sub(V a,V b) noexcept {return {a.x-b.x,a.y-b.y,a.z-b.z};}
    static V add(V a,V b) noexcept {return {a.x+b.x,a.y+b.y,a.z+b.z};}
    static V scale(V a,float s) noexcept {return {a.x*s,a.y*s,a.z*s};}
    static float dot(V a,V b) noexcept {return a.x*b.x+a.y*b.y+a.z*b.z;}
    static V cross(V a,V b) noexcept {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
    static V norm(V a) noexcept {
        const float n=std::sqrt(dot(a,a));
        return n>1.0e-8F?scale(a,1.0F/n):V{0,1,0};
    }
    bool inside(float x,float y) const noexcept {
        const float width=static_cast<float>(width_),height=static_cast<float>(height_);
        return std::isfinite(x)&&std::isfinite(y)&&x>=width*current_.viewport_left&&
            y>=height*current_.viewport_top&&x<width*(current_.viewport_left+current_.viewport_width)&&
            y<height*(current_.viewport_top+current_.viewport_height);
    }
    void updateEye() noexcept {
        const float horizontal=radius_*std::cos(pitch_);
        current_.position=add(current_.target,
            {horizontal*std::sin(yaw_),radius_*std::sin(pitch_),horizontal*std::cos(yaw_)});
    }
    RenderCamera current_{};
    std::uint64_t revision_{0};
    std::uint32_t width_{0},height_{0};
    float radius_{1},yaw_{0},pitch_{0},last_x_{0},last_y_{0};
    int drag_{0};
    bool active_{false};
};
} // namespace genomes::render
