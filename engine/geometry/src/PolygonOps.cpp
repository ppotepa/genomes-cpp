#include <genomes/geometry/PolygonOps.hpp>

#if GENOMES_HAS_EARCUT
#include <mapbox/earcut.hpp>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#ifndef GENOMES_HAS_EARCUT
#define GENOMES_HAS_EARCUT 0
#endif

namespace genomes::geometry {
namespace {
foundation::Error failure(foundation::ErrorCode code, const char* message) { return {code, message}; }
bool finite(math::Vec2 p) { return math::finite(p); }
float cross(math::Vec2 a, math::Vec2 b, math::Vec2 c) { return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
bool onSegment(math::Vec2 a, math::Vec2 b, math::Vec2 p, float e) { return std::fabs(cross(a,b,p))<=e&&p.x>=std::min(a.x,b.x)-e&&p.x<=std::max(a.x,b.x)+e&&p.y>=std::min(a.y,b.y)-e&&p.y<=std::max(a.y,b.y)+e; }
bool segmentsCross(math::Vec2 a,math::Vec2 b,math::Vec2 c,math::Vec2 d,float e) { const float ab_c=cross(a,b,c),ab_d=cross(a,b,d),cd_a=cross(c,d,a),cd_b=cross(c,d,b); if(((ab_c>e&&ab_d<-e)||(ab_c<-e&&ab_d>e))&&((cd_a>e&&cd_b<-e)||(cd_a<-e&&cd_b>e)))return true; return std::fabs(ab_c)<=e&&onSegment(a,b,c,e)||std::fabs(ab_d)<=e&&onSegment(a,b,d,e)||std::fabs(cd_a)<=e&&onSegment(c,d,a,e)||std::fabs(cd_b)<=e&&onSegment(c,d,b,e); }
bool ringSelfIntersects(std::span<const math::Vec2> ring,float e) { for(std::size_t i=0;i<ring.size();++i)for(std::size_t j=i+1;j<ring.size();++j){if(i==j||(i+1)%ring.size()==j||(j+1)%ring.size()==i)continue;if(segmentsCross(ring[i],ring[(i+1)%ring.size()],ring[j],ring[(j+1)%ring.size()],e))return true;}return false; }
bool ringContains(std::span<const math::Vec2> ring,math::Vec2 p) { bool inside=false; for(std::size_t i=0,j=ring.size()-1;i<ring.size();j=i++){const auto a=ring[i],b=ring[j];if(((a.y>p.y)!=(b.y>p.y))&&(p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x))inside=!inside;}return inside; }
}
float signedArea(std::span<const math::Vec2> ring) noexcept { double area=0; for(std::size_t i=0;i<ring.size();++i){const auto a=ring[i],b=ring[(i+1)%ring.size()];area+=static_cast<double>(a.x)*b.y-static_cast<double>(b.x)*a.y;} return static_cast<float>(area*0.5); }
foundation::Result<void, foundation::Error> validatePolygon(const Polygon2& polygon) {
    if(!(std::isfinite(polygon.epsilon)&&polygon.epsilon>0))return foundation::Result<void,foundation::Error>::failure(failure(foundation::ErrorCode::InvalidArgument,"polygon epsilon is invalid"));
    if(polygon.outer.size()<3)return foundation::Result<void,foundation::Error>::failure(failure(foundation::ErrorCode::InvalidArgument,"polygon outer ring needs three points"));
    auto validateRing=[&](std::span<const math::Vec2> ring){for(const auto p:ring)if(!finite(p))return false;return std::fabs(signedArea(ring))>polygon.epsilon&& !ringSelfIntersects(ring,polygon.epsilon);};
    if(!validateRing(polygon.outer))return foundation::Result<void,foundation::Error>::failure(failure(foundation::ErrorCode::InvalidArgument,"polygon outer ring is degenerate or self-intersecting"));
    for(const auto& hole:polygon.holes){if(hole.size()<3||!validateRing(hole)||!ringContains(polygon.outer,hole.front()))return foundation::Result<void,foundation::Error>::failure(failure(foundation::ErrorCode::InvalidArgument,"polygon hole is invalid or outside outer ring"));}
    return foundation::Result<void,foundation::Error>::success();
}
foundation::Result<Polygon2, foundation::Error> normalizeWinding(const Polygon2& polygon,PolygonWinding desired) {
    auto valid=validatePolygon(polygon); if(!valid)return foundation::Result<Polygon2,foundation::Error>::failure(valid.error()); Polygon2 result=polygon; const bool want_ccw=desired==PolygonWinding::CounterClockwise; if((signedArea(result.outer)>0)!=want_ccw)std::reverse(result.outer.begin(),result.outer.end()); for(auto& hole:result.holes)if((signedArea(hole)>0)==want_ccw)std::reverse(hole.begin(),hole.end()); return foundation::Result<Polygon2,foundation::Error>::success(std::move(result));
}
bool contains(const Polygon2& polygon,math::Vec2 point) noexcept { if(!ringContains(polygon.outer,point))return false;for(const auto& hole:polygon.holes)if(ringContains(hole,point))return false;return true; }
foundation::Result<std::vector<std::uint32_t>, foundation::Error> triangulate(const Polygon2& polygon) {
#if !GENOMES_HAS_EARCUT
    (void)polygon; return foundation::Result<std::vector<std::uint32_t>,foundation::Error>::failure(failure(foundation::ErrorCode::Unsupported,"earcut.hpp is disabled"));
#else
    auto normalized=normalizeWinding(polygon,PolygonWinding::CounterClockwise); if(!normalized)return foundation::Result<std::vector<std::uint32_t>,foundation::Error>::failure(normalized.error());
    using Point=std::array<float,2>; std::vector<std::vector<Point>> rings; rings.reserve(normalized.value().holes.size()+1); auto copyRing=[](const std::vector<math::Vec2>& source){std::vector<Point> result;result.reserve(source.size());for(const auto p:source)result.push_back({p.x,p.y});return result;}; rings.push_back(copyRing(normalized.value().outer));for(const auto& hole:normalized.value().holes)rings.push_back(copyRing(hole));
    auto indices=mapbox::earcut<std::uint32_t>(rings); if(indices.empty())return foundation::Result<std::vector<std::uint32_t>,foundation::Error>::failure(failure(foundation::ErrorCode::InvalidState,"earcut produced no triangles")); if(indices.size()%3U!=0U)return foundation::Result<std::vector<std::uint32_t>,foundation::Error>::failure(failure(foundation::ErrorCode::InvalidState,"earcut produced incomplete triangles")); return foundation::Result<std::vector<std::uint32_t>,foundation::Error>::success(std::move(indices));
#endif
}
} // namespace genomes::geometry
