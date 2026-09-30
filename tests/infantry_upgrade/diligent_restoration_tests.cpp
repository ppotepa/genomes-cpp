#include "TestSupport.hpp"
#include <DiligentGpuContracts.hpp>
#include <genomes/render/DrawMaterialPlan.hpp>
#include <genomes/camera/CameraController.hpp>
#include <genomes/camera/Camera.hpp>
#include <genomes/render/RenderFrameTransaction.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
using namespace genomes;
using upgrade_test::check;
using render::RenderResult;
RenderResult ok() { return RenderResult::success(); }
RenderResult fail() { return RenderResult::failure({foundation::ErrorCode::InvalidArgument,"original failure"}); }

void frameTransactions() {
    render::RenderFrameTransaction frame;
    int begins=0,submits=0,presents=0,aborts=0;
    frame.begin([&]{++begins;return ok();});
    frame.submit([&]{++submits;return fail();});
    frame.submit([&]{++submits;return ok();});
    frame.end([&]{++presents;return ok();},[&]{++aborts;return RenderResult::failure({foundation::ErrorCode::Internal,"abort failure"});});
    check(!frame.open()&&!frame.healthy(),"failed frame was not closed");
    check(begins==1&&submits==1&&presents==0&&aborts==1,"failed frame was presented or submitted twice");
    check(frame.error().message=="original failure","cleanup overwrote the original error");
    frame.end([&]{++presents;return ok();},[&]{++aborts;return ok();});
    check(aborts==1,"frame was closed twice");
    frame.begin([&]{++begins;return ok();});
    frame.submit([&]{++submits;return ok();});
    frame.end([&]{++presents;return ok();},[&]{++aborts;return ok();});
    check(frame.healthy()&&!frame.open()&&presents==1,"next frame did not recover");
    frame.begin([]{return fail();});
    frame.end([&]{++presents;return ok();},[&]{++aborts;return ok();});
    check(presents==1&&aborts==1,"failed begin attempted a close");
    frame.begin([]{return ok();});
    frame.submit([]()->RenderResult{throw std::runtime_error("test");});
    frame.end([&]{++presents;return ok();},[&]{++aborts;return ok();});
    check(aborts==2&&!frame.healthy(),"exception skipped the abort path");
    frame.begin([]{return ok();});frame.begin([]{return ok();});
    frame.end([&]{++presents;return ok();},[&]{++aborts;return ok();});
    check(aborts==3&&!frame.open(),"nested begin left a frame open");
}

render::RenderMesh mesh() {
    render::RenderMesh m;
    m.mesh_id=foundation::stable_id("mesh.test.restoration");m.revision=1;
    m.vertices.resize(4U);
    m.vertices[0].position={0,0,0};m.vertices[1].position={1,0,0};
    m.vertices[2].position={1,1,0};m.vertices[3].position={0,1,0};
    m.indices={0,1,2,0,2,3};
    return m;
}
void materials() {
    auto m=mesh();const auto original=m.indices;
    auto plan=render::buildMaterialDrawPlan(m);
    check(plan&&plan.value().size()==1U,"fallback material failed");
    check(plan.value()[0].index_count==6U,"fallback range incomplete");
    render::MaterialDescriptor a{},b{};
    a.material_id=foundation::stable_id("material.a");b.material_id=foundation::stable_id("material.b");
    b.alpha_mode=render::MaterialAlphaMode::Mask;b.alpha_cutoff=.3F;
    m.materials={a,b};m.material_groups={{0,3,0},{3,3,1}};
    plan=render::buildMaterialDrawPlan(m);
    check(plan&&plan.value().size()==2U,"material partition failed");
    check(m.indices==original,"planning changed topology");
    check(render::effectiveAlpha(plan.value()[1],{1,1,1,.2F})==render::MaterialAlphaMode::Mask,"mask classification changed");
    m.materials[0].instance_tint=true;
    plan=render::buildMaterialDrawPlan(m);
    check(render::effectiveAlpha(plan.value()[0],{1,1,1,.5F})==render::MaterialAlphaMode::Blend,"instance alpha ignored");
    m.vertices[0].color.a=.5F;m.materials[0].instance_tint=false;
    plan=render::buildMaterialDrawPlan(m);
    check(render::effectiveAlpha(plan.value()[0],{1,1,1,1})==render::MaterialAlphaMode::Blend,"vertex alpha ignored");
    m.materials[0].vertex_color=false;
    plan=render::buildMaterialDrawPlan(m);
    check(render::effectiveAlpha(plan.value()[0],{1,1,1,1})==render::MaterialAlphaMode::Opaque,"disabled vertex color affected alpha");
    auto bad=m;bad.material_groups[1].first_index=2;
    check(!render::buildMaterialDrawPlan(bad),"overlapping groups accepted");
    bad=m;bad.material_groups[1].material_index=2;
    check(!render::buildMaterialDrawPlan(bad),"bad material index accepted");
    bad=m;bad.material_groups.clear();check(!render::buildMaterialDrawPlan(bad),"ambiguous materials accepted");
    bad=m;bad.indices.back()=99;check(!render::buildMaterialDrawPlan(bad),"bad geometry index accepted");
    bad=m;bad.materials[0].base_color_texture=123;
    check(!render::buildMaterialDrawPlan(bad),"unresolved texture was silently ignored");
}

void palette() {
    using namespace render::diligent_contract;
    render::SkinnedMeshPrototype m;
    render::SkinnedBonePalette p;
    const std::array<float,16U> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    p.matrices.assign(kBoneCount,identity);
    check(validPalette(m,p),"identity palette rejected");
    p.matrices[2][15]=0;check(!validPalette(m,p),"non-affine palette accepted");
    p.matrices[2]=identity;p.matrices[1][0]=std::numeric_limits<float>::quiet_NaN();
    check(!validPalette(m,p),"non-finite palette accepted");
    p.matrices[1]=identity;p.debug_weight_bone=69;
    check(!validPalette(m,p),"bad weight heatmap bone accepted");
    p.debug_weight_bone=-1;p.morph_weights[0]=std::numeric_limits<float>::infinity();
    check(!validPalette(m,p),"non-finite morph accepted");
    p.morph_weights[0]=0;p.matrices.pop_back();check(!validPalette(m,p),"short palette accepted");
    check(sizeof(SkinnedPassConstants)==4640U&&sizeof(MaterialConstants)==64U,"GPU constant ABI changed");
}

void orbit() {
    camera::CameraController controller(camera::ControlMode::Orbit);
    camera::CameraRequest requested{};
    requested.position={0,1,5}; requested.target={0,1,0};
    controller.reset(requested);
    const auto original=requested.position;
    controller.update(requested,{0.5F,0.25F,0,0,0,0,false,false,false},1.0F/60.0F);
    check(!upgrade_test::equal(original,requested.position),"orbit input did not move camera");
    controller.update(requested,{0,0,0,0,0,0,false,false,true},1.0F/60.0F);
    check(upgrade_test::equal(original,requested.position),"focus loss did not reset camera");
    controller.setMode(camera::ControlMode::Fly);
    controller.update(requested,{0,0,0,0,1,0,false,false,false},1.0F);
    check(requested.position.z>original.z,"fly input did not move camera");
    controller.setMode(camera::ControlMode::RTS);
    controller.update(requested,{0,0,0,0,0,1,false,false,false},1.0F/60.0F);
    check(requested.position.y>0.0F,"RTS camera lost positive height");
}

void resolvedCameraAndSnapshot() {
    camera::CameraRequest request{};
    request.position = {0.0F, 2.0F, 6.0F};
    request.target = {0.0F, 1.0F, 0.0F};
    request.lens = {0.8F, 0.1F, 100.0F};
    const auto resolved = camera::resolve(request, 1920, 1080);
    check(resolved, "camera request did not resolve");
    check(resolved.value().viewport.width == 1920 && resolved.value().viewport.height == 1080,
          "resolved camera lost framebuffer viewport");
    const auto projected = camera::project(resolved.value(), {0.0F, 1.0F, 0.0F});
    check(projected && projected.value().z >= 0.0F && projected.value().z <= 1.0F,
          "D3D camera projection escaped [0,1]");

    render::PresentationSnapshot snapshot{};
    snapshot.simulation_tick = 41U;
    snapshot.scene_epoch = 9U;
    snapshot.resolved_camera = resolved.value();
    snapshot.has_resolved_camera = true;
    snapshot.clear_scene_payload();
    check(snapshot.simulation_tick == 41U && snapshot.scene_epoch == 9U,
          "scene payload clear erased frame metadata");
    check(snapshot.has_resolved_camera && snapshot.resolved_camera.viewport.width == 1920,
          "scene payload clear erased resolved camera");
}

void cameraOnlyPreservesPrototypeIdentity() {
    render::PresentationSnapshot snapshot{};
    auto prototype = std::make_shared<render::RenderMesh>(mesh());
    snapshot.instance_prototypes.push_back(prototype);
    snapshot.camera.revision = 3U;
    const auto before = snapshot.instance_prototypes.front();
    snapshot.camera.revision = 4U;
    check(snapshot.instance_prototypes.size() == 1U &&
              snapshot.instance_prototypes.front() == before,
          "camera-only revision rebuilt or replaced an immutable prototype");
    render::RenderUploadTelemetry telemetry{};
    telemetry.mesh_uploads = 0U;
    telemetry.palette_updates = 0U;
    check(telemetry.mesh_uploads == 0U && telemetry.palette_updates == 0U,
          "camera-only telemetry contract was not zeroed");
}
}
int main() {frameTransactions();materials();palette();orbit();resolvedCameraAndSnapshot();cameraOnlyPreservesPrototypeIdentity();return 0;}
