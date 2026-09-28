#pragma once

#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/ui/UiDocument.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace genomes::render::diligent_ui {
struct Vertex { float position[2]; float color[4]; };
inline constexpr std::size_t kVertexLimit = 131'072U;
using Glyph = std::array<std::uint8_t, 7U>;
inline constexpr std::array<Glyph, 36U> kFont{{
    {{14,17,17,31,17,17,17}}, {{30,17,17,30,17,17,30}},
    {{15,16,16,16,16,16,15}}, {{30,17,17,17,17,17,30}},
    {{31,16,16,30,16,16,31}}, {{31,16,16,30,16,16,16}},
    {{15,16,16,23,17,17,15}}, {{17,17,17,31,17,17,17}},
    {{31,4,4,4,4,4,31}}, {{1,1,1,1,17,17,14}},
    {{17,18,20,24,20,18,17}}, {{16,16,16,16,16,16,31}},
    {{17,27,21,21,17,17,17}}, {{17,25,21,19,17,17,17}},
    {{14,17,17,17,17,17,14}}, {{30,17,17,30,16,16,16}},
    {{14,17,17,17,21,18,13}}, {{30,17,17,30,20,18,17}},
    {{15,16,16,14,1,1,30}}, {{31,4,4,4,4,4,4}},
    {{17,17,17,17,17,17,14}}, {{17,17,17,17,17,10,4}},
    {{17,17,17,21,21,21,10}}, {{17,10,4,4,4,10,17}},
    {{17,10,4,4,4,4,4}}, {{31,2,4,4,8,16,31}},
    {{14,17,19,21,25,17,14}}, {{4,12,4,4,4,4,14}},
    {{14,17,1,2,4,8,31}}, {{30,1,1,14,1,1,30}},
    {{2,6,10,18,31,2,2}}, {{31,16,16,30,1,1,30}},
    {{14,16,16,30,17,17,14}}, {{31,1,2,4,8,8,8}},
    {{14,17,17,14,17,17,14}}, {{14,17,17,15,1,1,14}}
}};
inline Glyph glyph(char c) noexcept {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
    if (c >= 'A' && c <= 'Z') return kFont[static_cast<std::size_t>(c - 'A')];
    if (c >= '0' && c <= '9') return kFont[26U + static_cast<std::size_t>(c - '0')];
    switch (c) {
    case '-': return {0,0,0,31,0,0,0};
    case '+': return {0,4,4,31,4,4,0};
    case '/': return {1,1,2,4,8,16,16};
    case '.': return {0,0,0,0,0,0,4};
    case ':': return {0,4,0,0,0,4,0};
    case '_': return {0,0,0,0,0,0,31};
    case '(': return {2,4,8,8,8,4,2};
    case ')': return {8,4,2,2,2,4,8};
    case '[': return {14,8,8,8,8,8,14};
    case ']': return {14,2,2,2,2,2,14};
    case '|': return {4,4,4,4,4,4,4};
    case '=': return {0,0,31,0,31,0,0};
    case '<': return {0,2,4,8,4,2,0};
    case '>': return {0,8,4,2,4,8,0};
    default: return {};
    }
}
inline void rect(std::vector<Vertex>& out, float x, float y, float w, float h,
                 foundation::Color color, float screen_w, float screen_h) {
    if (w <= 0 || h <= 0 || screen_w <= 0 || screen_h <= 0 ||
        out.size() + 6U > kVertexLimit) return;
    const float l = std::clamp(x, 0.0F, screen_w) / screen_w * 2.0F - 1.0F;
    const float r = std::clamp(x + w, 0.0F, screen_w) / screen_w * 2.0F - 1.0F;
    const float t = 1.0F - std::clamp(y, 0.0F, screen_h) / screen_h * 2.0F;
    const float b = 1.0F - std::clamp(y + h, 0.0F, screen_h) / screen_h * 2.0F;
    if (r <= l || t <= b) return;
    const Vertex tl{{l,t},{color.r,color.g,color.b,color.a}};
    const Vertex tr{{r,t},{color.r,color.g,color.b,color.a}};
    const Vertex bl{{l,b},{color.r,color.g,color.b,color.a}};
    const Vertex br{{r,b},{color.r,color.g,color.b,color.a}};
    out.insert(out.end(), {tl,bl,tr,tr,bl,br});
}
inline void text(std::vector<Vertex>& out, std::string_view value, float x, float y,
                 float scale, foundation::Color color, float screen_w, float screen_h,
                 float limit_w, float limit_h, bool wrap = false) {
    if (scale <= 0 || limit_w <= 0 || limit_h < 7.0F * scale) return;
    float px = x, py = y;
    for (char c : value) {
        if (c == '\n' || px + 5.0F * scale > x + limit_w) {
            if (!wrap && c != '\n') break;
            px = x; py += 10.0F * scale;
            if (c == '\n') continue;
        }
        if (py + 7.0F * scale > y + limit_h || out.size() + 210U > kVertexLimit) break;
        const auto g = glyph(c);
        for (std::size_t row = 0; row < 7U; ++row) {
            for (std::size_t col = 0; col < 5U; ++col) {
                if ((g[row] & (1U << (4U - col))) != 0) {
                    rect(out, px + static_cast<float>(col)*scale,
                         py + static_cast<float>(row)*scale, scale, scale, color, screen_w, screen_h);
                }
            }
        }
        px += 6.0F * scale;
    }
}
inline void document(std::vector<Vertex>& out, const ui::UiDocument& doc, float w, float h) {
    const bool menu = std::any_of(doc.nodes.begin(), doc.nodes.end(), [](const auto& n) {
        return n.id == foundation::stable_id("menu.panel");
    });
    if (menu && w > 800.0F) {
        const float x = w * 0.54F;
        rect(out,x,48,w-x-48,h-96,{0.035F,0.055F,0.09F,1},w,h);
        text(out,"WORLD PREVIEW",x+20,76,1.5F,{0.64F,0.75F,0.86F,1},w,h,w-x-60,20);
    }
    float row_y = menu ? 184.0F : 164.0F;
    for (const auto& node : doc.nodes) {
        float x = node.explicit_layout ? node.left : 78.0F;
        float y = node.explicit_layout ? node.top : row_y;
        float nw = node.explicit_layout ? node.width : std::min(node.width > 0 ? node.width : w-110.0F,w-110.0F);
        float nh = node.explicit_layout ? node.height : node.height;
        float font = node.explicit_layout ? node.font_scale : 1.5F;
        foundation::Color ink = node.enabled ? foundation::Color{0.82F,0.87F,0.94F,1}
                                             : foundation::Color{0.38F,0.42F,0.48F,1};
        if (node.type == ui::UiNodeType::Panel) {
            if (!node.explicit_layout) { x=48; y=48; nw=std::min(node.width,w-96); nh=std::min(node.height,h-96); font=3; }
            rect(out,x,y,nw,nh,{0.025F,0.035F,0.06F,1},w,h);
            text(out,node.text,x+12,y+12,font,ink,w,h,nw-24,24);
        } else if (node.type == ui::UiNodeType::Button) {
            if (!node.explicit_layout) font=2;
            const foundation::Color fill = node.selected ? foundation::Color{0.10F,0.38F,0.67F,1}
                : node.enabled ? foundation::Color{0.07F,0.11F,0.18F,1}
                               : foundation::Color{0.045F,0.055F,0.075F,1};
            rect(out,x,y,nw,nh,fill,w,h);
            text(out,node.text,x+10,y+std::max(2.0F,(nh-7.0F*font)*0.5F),font,ink,w,h,nw-20,nh);
            if (!node.explicit_layout) row_y+=nh+12;
        } else if (node.type == ui::UiNodeType::Separator) {
            rect(out,x,y,nw,1,{0.19F,0.25F,0.34F,1},w,h);
            if (!node.explicit_layout) row_y+=24;
        } else {
            if (!node.explicit_layout) {
                nh=30;
                if (node.text=="PROCEDURAL WORLD" || node.text=="New world") y=112;
                else if (node.id==foundation::stable_id("menu.version") ||
                         node.id==foundation::stable_id("world-config.note")) { y=h-46; font=1; }
                else row_y+=34;
            }
            text(out,node.text,x,y,font,ink,w,h,nw,nh,node.explicit_layout);
        }
    }
}
inline void worldPlan(std::vector<Vertex>& out, const PresentationSnapshot& snapshot,
                      float width, float height) {
    float map_size = 0;
    for (const auto& instance : snapshot.instances) {
        if (instance.mesh_id == foundation::stable_id("mesh.world.terrain")) {
            map_size = std::max(std::abs(instance.scale.x),std::abs(instance.scale.z));
        }
    }
    if (map_size <= 0) return;
    const float left=width*0.54F, top=48, w=width-left-48, h=height-96;
    rect(out,left,top,w,h,{0.055F,0.10F,0.075F,1},width,height);
    for (const auto& i : snapshot.instances) {
        if (i.mesh_id == foundation::stable_id("mesh.world.terrain")) continue;
        foundation::Color c{0.18F,0.46F,0.23F,1};
        if (i.mesh_id == foundation::stable_id("mesh.world.road")) c={0.48F,0.50F,0.47F,1};
        else if (i.mesh_id == foundation::stable_id("mesh.world.building")) c={0.67F,0.38F,0.22F,1};
        else if (i.mesh_id == foundation::stable_id("mesh.world.fence")) c={0.76F,0.62F,0.29F,1};
        else if (i.mesh_id != foundation::stable_id("mesh.world.parcel") &&
                 i.mesh_id != foundation::stable_id("mesh.world.vegetation")) continue;
        const float fw=std::clamp(std::abs(i.scale.x)/map_size*w*0.84F,2.0F,w*0.8F);
        const float fh=std::clamp(std::abs(i.scale.z)/map_size*h*0.84F,2.0F,h*0.8F);
        rect(out,left+w*0.5F+i.position.x/map_size*w*0.84F-fw*0.5F,
             top+h*0.5F+i.position.z/map_size*h*0.84F-fh*0.5F,fw,fh,c,width,height);
    }
}
} // namespace genomes::render::diligent_ui
