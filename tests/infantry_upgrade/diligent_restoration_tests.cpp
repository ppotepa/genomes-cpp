#include "TestSupport.hpp"
#include <DiligentGpuContracts.hpp>
#include <genomes/render/DrawMaterialPlan.hpp>
#include <genomes/render/OrbitCameraController.hpp>
#include <genomes/render/RenderFrameTransaction.hpp>
#include <limits>
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

input::Event mouse(input::EventType type,int button,float x,float y) {
    input::Event e{};e.type=type;e.mouse_button=button;e.x=x;e.y=y;return e;
}
void orbit() {
    render::OrbitCameraController controller;
    render::RenderCamera requested{};requested.enabled=true;requested.position={0,1,5};requested.target={0,1,0};
    requested.interactive_orbit=true;requested.revision=1;
    requested.viewport_left=.4F;requested.viewport_width=.6F;
    const auto original=controller.resolve(requested,1000,600);
    input::InputFrame f{};
    f.events={mouse(input::EventType::MouseButtonDown,1,100,200),mouse(input::EventType::MouseMove,0,200,250)};
    controller.input(f);
    check(controller.dragButton()==0,"UI press started orbit");
    check(upgrade_test::equal(original.position,controller.resolve(requested,1000,600).position),"UI press moved camera");
    f.events={mouse(input::EventType::MouseButtonDown,1,600,200),mouse(input::EventType::MouseMove,0,680,225)};
    controller.input(f);
    const auto rotated=controller.resolve(requested,1000,600);
    check(!upgrade_test::equal(original.position,rotated.position),"viewport drag did not rotate");
    f.events={mouse(input::EventType::MouseButtonUp,1,2000,2000)};controller.input(f);
    check(controller.dragButton()==0,"release outside viewport left a stuck drag");
    check(upgrade_test::equal(rotated.position,controller.resolve(requested,2000,1200).position),"resize reset orbit");
    requested.revision=2;
    check(upgrade_test::equal(original.position,controller.resolve(requested,1000,600).position),"preset revision did not reset camera");
    f.mouse_x=700;f.mouse_y=300;f.events={mouse(input::EventType::MouseWheel,0,0,0)};f.events[0].wheel_y=1;
    controller.input(f);
    const auto zoomed=controller.resolve(requested,1000,600);
    check(zoomed.position.z<original.position.z,"positive wheel did not zoom in");
    f.events={mouse(input::EventType::MouseButtonDown,3,700,250),mouse(input::EventType::MouseMove,0,720,260)};
    controller.input(f);
    check(!upgrade_test::equal(zoomed.target,controller.resolve(requested,1000,600).target),"pan did not move target");
    requested.interactive_orbit=false;
    check(upgrade_test::equal(requested.position,controller.resolve(requested,1000,600).position)&&controller.dragButton()==0,"non-orbit camera retained interactive ownership");
}
}
int main() {frameTransactions();materials();palette();orbit();return 0;}
