#include <genomes/render/gpu_scene/GpuScene.hpp>

#include <cassert>
#include <span>
#include <vector>

int main() {
    using namespace genomes;

    render::gpu_scene::GpuScene scene;
    const foundation::StableId first_id = foundation::stable_id("test.gpu.first");
    const foundation::StableId second_id = foundation::stable_id("test.gpu.second");
    const foundation::StableId third_id = foundation::stable_id("test.gpu.third");
    const foundation::StableId mesh_id = foundation::stable_id("mesh.test");
    const foundation::StableId material_id = foundation::stable_id("material.test");

    std::vector<render::RenderInstance> instances{
        {first_id, mesh_id, material_id, {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, 0.0F},
        {second_id, mesh_id, material_id, {2.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, 0.0F}};

    const auto initial = scene.sync(instances);
    assert(initial);
    assert(scene.liveCount() == 2);
    assert(initial.value().ranges.size() == 1);
    assert(initial.value().ranges.front().count == 2);
    const auto complete_upload = scene.fullUpload();
    assert(complete_upload.ranges.size() == 1);
    assert(complete_upload.payload.size() == scene.capacity());

    const auto first_handle = scene.find(first_id);
    const auto second_handle = scene.find(second_id);
    assert(first_handle.isValid());
    assert(second_handle.isValid());
    assert(scene.record(first_handle) != nullptr);

    const auto unchanged = scene.sync(instances);
    assert(unchanged);
    assert(unchanged.value().empty());

    instances.resize(1);
    const auto removed = scene.sync(instances);
    assert(removed);
    assert(scene.liveCount() == 1);
    assert(removed.value().removed.size() == 1);
    assert(removed.value().removed.front() == second_handle);
    assert(scene.record(second_handle) == nullptr);

    instances[0].position.x = 4.0F;
    instances.push_back(
        {third_id, mesh_id, material_id, {6.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, 0.0F});
    const auto reused = scene.sync(instances);
    assert(reused);
    assert(scene.liveCount() == 2);
    assert(scene.find(third_id).isValid());
    assert(scene.find(third_id).generation != second_handle.generation);

    instances.push_back(instances.front());
    assert(!scene.sync(instances));

    const foundation::StableId duplicate_new_id =
        foundation::stable_id("test.gpu.duplicate-new");
    const std::vector<render::RenderInstance> duplicate_new{
        {duplicate_new_id, mesh_id, material_id, {8.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F},
         0.0F},
        {duplicate_new_id, mesh_id, material_id, {9.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F},
         0.0F}};
    assert(!scene.sync(duplicate_new));
    assert(!scene.find(duplicate_new_id).isValid());

    const render::gpu_scene::GpuInstanceHandle handles[] = {scene.find(first_id),
                                                             scene.find(third_id)};
    const auto released = scene.release(handles);
    assert(released);
    assert(released.value().removed.size() == 2);
    assert(scene.liveCount() == 0);
    assert(scene.memoryBytes() > 0);

    render::gpu_scene::GpuScene delta_scene;
    const render::RenderInstance delta_instance{
        first_id, mesh_id, material_id, {1.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, 0.0F};
    const render::RenderChange add_delta{render::RenderChangeKind::Added, first_id,
                                         delta_instance};
    const auto added_delta = delta_scene.apply(std::span<const render::RenderChange>{
        &add_delta, 1});
    assert(added_delta && delta_scene.liveCount() == 1);
    const render::RenderInstance updated_instance{
        first_id, mesh_id, material_id, {5.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, 0.0F};
    const render::RenderChange update_delta{render::RenderChangeKind::Updated, first_id,
                                            updated_instance};
    const auto updated_delta = delta_scene.apply(std::span<const render::RenderChange>{
        &update_delta, 1});
    assert(updated_delta && !updated_delta.value().empty());
    const render::RenderChange remove_delta{render::RenderChangeKind::Removed, first_id,
                                            updated_instance};
    const auto removed_delta = delta_scene.apply(std::span<const render::RenderChange>{
        &remove_delta, 1});
    assert(removed_delta && delta_scene.liveCount() == 0);
    return 0;
}
