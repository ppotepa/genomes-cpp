#include "ThreeppSceneRenderer.hpp"
#include "ThreeppGlBootstrap.hpp"
#include "ThreeppUiPass.hpp"

#include <genomes/platform/Platform.hpp>

#include <threepp/cameras/PerspectiveCamera.hpp>
#include <threepp/core/BufferAttribute.hpp>
#include <threepp/core/BufferGeometry.hpp>
#include <threepp/lights/DirectionalLight.hpp>
#include <threepp/lights/HemisphereLight.hpp>
#include <threepp/materials/LineBasicMaterial.hpp>
#include <threepp/materials/MeshStandardMaterial.hpp>
#include <threepp/objects/Bone.hpp>
#include <threepp/objects/LineSegments.hpp>
#include <threepp/objects/Mesh.hpp>
#include <threepp/objects/Skeleton.hpp>
#include <threepp/objects/SkinnedMesh.hpp>
#include <threepp/renderers/GLRenderer.hpp>
#include <threepp/scenes/Scene.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genomes::render {
namespace {

using GeometryPtr = std::shared_ptr<threepp::BufferGeometry>;

template<class Attribute>
std::shared_ptr<threepp::BufferAttribute> share(std::unique_ptr<Attribute> attribute) {
    return std::shared_ptr<threepp::BufferAttribute>(std::move(attribute));
}

threepp::Color color(foundation::Color value) {
    return {value.r, value.g, value.b};
}

void setTransform(threepp::Object3D& object, const RenderInstance& instance) {
    object.position.set(instance.position.x, instance.position.y, instance.position.z);
    object.scale.set(instance.scale.x, instance.scale.y, instance.scale.z);
    object.rotation.set(0.0F, instance.rotation_y, 0.0F);
}

void setBoneTransform(threepp::Bone& bone, const SkinnedBoneTransform& transform) {
    bone.position.set(transform.translation.x, transform.translation.y, transform.translation.z);
    bone.quaternion.set(transform.rotation.x, transform.rotation.y,
                        transform.rotation.z, transform.rotation.w);
    bone.scale.set(transform.scale.x, transform.scale.y, transform.scale.z);
    bone.matrixWorldNeedsUpdate = true;
}

GeometryPtr regularGeometry(const RenderMesh& source) {
    auto geometry = threepp::BufferGeometry::create();
    std::vector<float> positions, normals, uvs, colors;
    positions.reserve(source.vertices.size() * 3U);
    normals.reserve(source.vertices.size() * 3U);
    uvs.reserve(source.vertices.size() * 2U);
    colors.reserve(source.vertices.size() * 3U);
    for (const auto& v : source.vertices) {
        positions.insert(positions.end(), {v.position.x, v.position.y, v.position.z});
        normals.insert(normals.end(), {v.normal.x, v.normal.y, v.normal.z});
        uvs.insert(uvs.end(), {v.uv.x, v.uv.y});
        colors.insert(colors.end(), {v.color.r, v.color.g, v.color.b});
    }
    std::vector<unsigned int> indices(source.indices.begin(), source.indices.end());
    geometry->setAttribute("position", share(threepp::FloatBufferAttribute::create(std::move(positions), 3)));
    geometry->setAttribute("normal", share(threepp::FloatBufferAttribute::create(std::move(normals), 3)));
    geometry->setAttribute("uv", share(threepp::FloatBufferAttribute::create(std::move(uvs), 2)));
    geometry->setAttribute("color", share(threepp::FloatBufferAttribute::create(std::move(colors), 3)));
    geometry->setIndex(std::move(indices));
    geometry->computeBoundingBox();
    geometry->computeBoundingSphere();
    return geometry;
}

std::uint16_t triangleRegion(const SkinnedMeshPrototype& source, std::size_t offset) {
    const auto i0 = source.indices[offset];
    const auto i1 = source.indices[offset + 1U];
    const auto i2 = source.indices[offset + 2U];
    const auto a = source.vertices[i0].material_region;
    const auto b = source.vertices[i1].material_region;
    const auto c = source.vertices[i2].material_region;
    if (a == b || a == c) return a;
    if (b == c) return b;
    return a;
}

GeometryPtr skinnedGeometry(const SkinnedMeshPrototype& source) {
    auto geometry = threepp::BufferGeometry::create();
    geometry->morphTargetsRelative = true;
    std::vector<float> positions, normals, uvs, colors, skin_indices, skin_weights;
    positions.reserve(source.vertices.size() * 3U);
    normals.reserve(source.vertices.size() * 3U);
    uvs.reserve(source.vertices.size() * 2U);
    colors.reserve(source.vertices.size() * 3U);
    skin_indices.reserve(source.vertices.size() * 4U);
    skin_weights.reserve(source.vertices.size() * 4U);
    for (const auto& v : source.vertices) {
        positions.insert(positions.end(), {v.position.x, v.position.y, v.position.z});
        normals.insert(normals.end(), {v.normal.x, v.normal.y, v.normal.z});
        uvs.insert(uvs.end(), {v.uv.x, v.uv.y});
        colors.insert(colors.end(), {v.color.r, v.color.g, v.color.b});
        for (std::size_t k = 0U; k < 4U; ++k) {
            skin_indices.push_back(static_cast<float>(v.bone_indices[k]));
            skin_weights.push_back(v.bone_weights[k]);
        }
    }
    std::vector<unsigned int> indices(source.indices.begin(), source.indices.end());
    geometry->setAttribute("position", share(threepp::FloatBufferAttribute::create(std::move(positions), 3)));
    geometry->setAttribute("normal", share(threepp::FloatBufferAttribute::create(std::move(normals), 3)));
    geometry->setAttribute("uv", share(threepp::FloatBufferAttribute::create(std::move(uvs), 2)));
    geometry->setAttribute("color", share(threepp::FloatBufferAttribute::create(std::move(colors), 3)));
    // Pinned threepp's SkinnedMesh::boneTransform intentionally reads skinIndex as float.
    geometry->setAttribute("skinIndex", share(threepp::FloatBufferAttribute::create(std::move(skin_indices), 4)));
    geometry->setAttribute("skinWeight", share(threepp::FloatBufferAttribute::create(std::move(skin_weights), 4)));
    geometry->setIndex(std::move(indices));

    for (std::size_t morph = 0U;
         morph < source.morph_target_count && morph < source.morphs.size(); ++morph) {
        std::vector<float> delta_position, delta_normal;
        delta_position.reserve(source.vertices.size() * 3U);
        delta_normal.reserve(source.vertices.size() * 3U);
        const auto& target = source.morphs[morph];
        for (std::size_t vertex = 0U; vertex < source.vertices.size(); ++vertex) {
            const auto p = vertex < target.position_deltas.size()
                ? target.position_deltas[vertex] : foundation::Vec3{};
            const auto n = vertex < target.normal_deltas.size()
                ? target.normal_deltas[vertex] : foundation::Vec3{};
            delta_position.insert(delta_position.end(), {p.x, p.y, p.z});
            delta_normal.insert(delta_normal.end(), {n.x, n.y, n.z});
        }
        geometry->getOrCreateMorphAttribute("position")->push_back(
            share(threepp::FloatBufferAttribute::create(std::move(delta_position), 3)));
        geometry->getOrCreateMorphAttribute("normal")->push_back(
            share(threepp::FloatBufferAttribute::create(std::move(delta_normal), 3)));
    }

    // Preserve source index order. Groups only describe contiguous material runs.
    if (source.indices.size() >= 3U) {
        std::size_t run_start = 0U;
        std::uint16_t run_region = triangleRegion(source, 0U);
        for (std::size_t offset = 3U; offset < source.indices.size(); offset += 3U) {
            const auto region = triangleRegion(source, offset);
            if (region == run_region) continue;
            geometry->addGroup(static_cast<int>(run_start),
                               static_cast<int>(offset - run_start), run_region);
            run_start = offset;
            run_region = region;
        }
        geometry->addGroup(static_cast<int>(run_start),
                           static_cast<int>(source.indices.size() - run_start), run_region);
    }
    geometry->computeBoundingBox();
    geometry->computeBoundingSphere();
    return geometry;
}

std::shared_ptr<threepp::MeshStandardMaterial> standardMaterial(float roughness, float metalness,
                                                                bool morphs) {
    auto material = threepp::MeshStandardMaterial::create(
        threepp::MeshStandardMaterial::Params{}
            .color(threepp::Color(1.0F, 1.0F, 1.0F))
            .roughness(roughness)
            .metalness(metalness));
    material->vertexColors = true;
    material->morphTargets = morphs;
    material->morphNormals = morphs;
    return material;
}

std::vector<std::shared_ptr<threepp::Material>> characterMaterials() {
    std::vector<std::shared_ptr<threepp::Material>> result;
    result.reserve(18U);
    for (std::uint16_t region = 0U; region < 18U; ++region) {
        float roughness = 0.82F, metalness = 0.0F;
        switch (region) {
        case 1U: case 2U: case 8U: case 9U: case 10U: case 12U:
            roughness = 0.68F; break; // skin / lips / hand
        case 4U: roughness = 0.55F; break; // hair
        case 5U: case 6U: case 7U:
            roughness = 0.22F; break; // eye surfaces
        case 13U: case 15U:
            roughness = 0.46F; break; // leather
        case 16U:
            roughness = 0.28F; metalness = 0.72F; break;
        case 17U:
            roughness = 0.52F; metalness = 0.08F; break;
        default: break;
        }
        result.push_back(standardMaterial(roughness, metalness, true));
    }
    return result;
}

std::shared_ptr<threepp::Material> ordinaryMaterial() {
    return standardMaterial(0.82F, 0.0F, false);
}

} // namespace

struct ThreeppSceneRenderer::Impl final {
    struct GeometryCache final {
        std::uint64_t revision{0U};
        GeometryPtr geometry;
        std::uint64_t last_seen{0U};
        std::size_t source_bytes{0U};
    };
    struct OrdinaryInstance final {
        foundation::StableId mesh_id{0U};
        std::uint64_t mesh_revision{0U};
        std::shared_ptr<threepp::Mesh> mesh;
        std::uint64_t last_seen{0U};
    };
    struct SkinnedInstance final {
        foundation::StableId mesh_id{0U};
        std::uint64_t mesh_revision{0U};
        std::shared_ptr<threepp::SkinnedMesh> mesh;
        std::shared_ptr<threepp::Skeleton> skeleton;
        std::vector<std::shared_ptr<threepp::Bone>> bones;
        std::uint64_t last_seen{0U};
    };

    platform::SdlPlatform& platform;
    std::unique_ptr<threepp::GLRenderer> renderer;
    std::shared_ptr<threepp::Scene> scene;
    std::shared_ptr<threepp::PerspectiveCamera> camera;
    std::shared_ptr<threepp::HemisphereLight> hemisphere;
    std::shared_ptr<threepp::DirectionalLight> key;
    std::shared_ptr<threepp::DirectionalLight> fill;
    std::shared_ptr<threepp::Object3D> key_target;
    std::shared_ptr<threepp::Object3D> fill_target;
    std::shared_ptr<threepp::LineBasicMaterial> debug_material;
    std::vector<std::shared_ptr<threepp::Material>> character_materials;
    std::shared_ptr<threepp::Material> ordinary_material;
    ThreeppUiPass ui;
    std::unordered_map<foundation::StableId, GeometryCache> ordinary_geometry;
    std::unordered_map<foundation::StableId, GeometryCache> skinned_geometry;
    std::unordered_map<foundation::StableId, OrdinaryInstance> ordinary_instances;
    std::unordered_map<foundation::StableId, SkinnedInstance> skinned_instances;
    RenderCapabilities caps{};
    RenderUploadTelemetry telemetry{};
    foundation::Error error{};
    bool healthy{true};
    bool frame_open{false};
    std::uint32_t width{1U};
    std::uint32_t height{1U};

    explicit Impl(platform::SdlPlatform& host) : platform(host) {}

    void fail(foundation::ErrorCode code, std::string_view message) noexcept {
        healthy = false;
        error = {code, message};
    }

    GeometryPtr ensureOrdinary(const std::shared_ptr<const RenderMesh>& mesh) {
        if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) return {};
        auto& cache = ordinary_geometry[mesh->mesh_id];
        if (!cache.geometry || cache.revision != mesh->revision) {
            cache.geometry = regularGeometry(*mesh);
            cache.revision = mesh->revision;
            cache.source_bytes = mesh->vertices.size() * sizeof(RenderMeshVertex) +
                                 mesh->indices.size() * sizeof(std::uint32_t);
            ++telemetry.mesh_uploads;
            ++telemetry.total_mesh_uploads;
            telemetry.mesh_upload_bytes += cache.source_bytes;
            telemetry.total_mesh_upload_bytes += cache.source_bytes;
        }
        cache.last_seen = telemetry.frame;
        return cache.geometry;
    }

    GeometryPtr ensureSkinned(const std::shared_ptr<const SkinnedMeshPrototype>& mesh) {
        if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) return {};
        auto& cache = skinned_geometry[mesh->mesh_id];
        if (!cache.geometry || cache.revision != mesh->revision) {
            cache.geometry = skinnedGeometry(*mesh);
            cache.revision = mesh->revision;
            cache.source_bytes = mesh->vertices.size() * sizeof(SkinnedMeshVertex) +
                                 mesh->indices.size() * sizeof(std::uint32_t);
            ++telemetry.mesh_uploads;
            ++telemetry.total_mesh_uploads;
            telemetry.mesh_upload_bytes += cache.source_bytes;
            telemetry.total_mesh_upload_bytes += cache.source_bytes;
        }
        cache.last_seen = telemetry.frame;
        return cache.geometry;
    }

    const SkinnedBonePalette* palette(const PresentationSnapshot& snapshot,
                                      foundation::StableId object_id) const noexcept {
        for (const auto& value : snapshot.skinned_palettes)
            if (value.instance_id == object_id) return &value;
        return nullptr;
    }

    const std::shared_ptr<const SkinnedMeshPrototype>* skinnedPrototype(
        const PresentationSnapshot& snapshot, foundation::StableId mesh_id) const noexcept {
        for (const auto& value : snapshot.skinned_prototypes)
            if (value && value->mesh_id == mesh_id) return &value;
        return nullptr;
    }

    const std::shared_ptr<const RenderMesh>* ordinaryPrototype(
        const PresentationSnapshot& snapshot, foundation::StableId mesh_id) const noexcept {
        for (const auto& value : snapshot.instance_prototypes)
            if (value && value->mesh_id == mesh_id) return &value;
        if (snapshot.infantry_mesh && snapshot.infantry_mesh->mesh_id == mesh_id)
            return &snapshot.infantry_mesh;
        return nullptr;
    }

    OrdinaryInstance& ensureOrdinaryInstance(const RenderInstance& source,
                                             const std::shared_ptr<const RenderMesh>& prototype) {
        auto geometry = ensureOrdinary(prototype);
        auto& instance = ordinary_instances[source.object_id];
        if (!instance.mesh || instance.mesh_id != source.mesh_id ||
            instance.mesh_revision != prototype->revision) {
            instance.mesh = threepp::Mesh::create(geometry, ordinary_material);
            instance.mesh_id = source.mesh_id;
            instance.mesh_revision = prototype->revision;
        }
        instance.last_seen = telemetry.frame;
        setTransform(*instance.mesh, source);
        return instance;
    }

    void prune() {
        const auto old = telemetry.frame > 600U ? telemetry.frame - 600U : 0U;
        for (auto it = ordinary_instances.begin(); it != ordinary_instances.end();)
            it = it->second.last_seen < old ? ordinary_instances.erase(it) : std::next(it);
        for (auto it = skinned_instances.begin(); it != skinned_instances.end();)
            it = it->second.last_seen < old ? skinned_instances.erase(it) : std::next(it);
        for (auto it = ordinary_geometry.begin(); it != ordinary_geometry.end();)
            it = it->second.last_seen < old ? ordinary_geometry.erase(it) : std::next(it);
        for (auto it = skinned_geometry.begin(); it != skinned_geometry.end();)
            it = it->second.last_seen < old ? skinned_geometry.erase(it) : std::next(it);
    }
};

foundation::Result<std::unique_ptr<ThreeppSceneRenderer>, foundation::Error>
ThreeppSceneRenderer::create(platform::SdlPlatform& platform) {
    using Result = foundation::Result<std::unique_ptr<ThreeppSceneRenderer>, foundation::Error>;
    if (platform.graphics_api() != platform::WindowGraphicsApi::OpenGL) {
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "threepp renderer requires an SDL OpenGL window"});
    }
    if (auto current = platform.make_gl_current(); !current) {
        return Result::failure(current.error());
    }
    if (auto info = initializeThreeppGl(platform); !info) {
        return Result::failure(info.error());
    }
    try {
        auto impl = std::make_unique<Impl>(platform);
        impl->width = static_cast<std::uint32_t>(std::max(1, platform.width()));
        impl->height = static_cast<std::uint32_t>(std::max(1, platform.height()));
        impl->renderer = std::make_unique<threepp::GLRenderer>(
            std::pair<int,int>{static_cast<int>(impl->width), static_cast<int>(impl->height)});
        impl->renderer->setPixelRatio(1.0F);
        impl->renderer->outputColorSpace = threepp::ColorSpace::sRGB;
        impl->renderer->toneMapping = threepp::ToneMapping::ACESFilmic;
        impl->renderer->toneMappingExposure = 1.0F;
        impl->renderer->useLegacyLights = false;
        impl->renderer->shadowMap().enabled = true;
        impl->renderer->setClearColor(threepp::Color(0.025F, 0.035F, 0.055F), 1.0F);

        impl->scene = threepp::Scene::create();
        impl->camera = threepp::PerspectiveCamera::create(60.0F,
            static_cast<float>(impl->width) / static_cast<float>(impl->height), 0.05F, 10'000.0F);
        impl->hemisphere = threepp::HemisphereLight::create(0xffffff, 0x201812, 0.8F);
        impl->key = threepp::DirectionalLight::create(0xffe0b8, 2.0F);
        impl->fill = threepp::DirectionalLight::create(0x8fb8ff, 0.55F);
        impl->key_target = threepp::Object3D::create();
        impl->fill_target = threepp::Object3D::create();
        impl->key->setTarget(*impl->key_target);
        impl->fill->setTarget(*impl->fill_target);
        impl->ordinary_material = ordinaryMaterial();
        impl->character_materials = characterMaterials();
        impl->debug_material = threepp::LineBasicMaterial::create(
            threepp::LineBasicMaterial::Params{}.color(threepp::Color(1.0F,1.0F,1.0F)));
        impl->debug_material->vertexColors = true;
        if (auto ui = impl->ui.initialize(); !ui) return Result::failure(ui.error());

        impl->caps.backend = RenderBackendKind::OpenGL;
        impl->caps.initialized = true;
        impl->caps.headless = false;
        impl->caps.presentation = true;
        impl->caps.instanced_rendering = true;
        impl->caps.gpu_skinning = true;
        impl->caps.api_major = 3U;
        impl->caps.api_minor = 3U;
        return Result::success(std::unique_ptr<ThreeppSceneRenderer>(
            new ThreeppSceneRenderer(std::move(impl))));
    } catch (...) {
        return Result::failure({foundation::ErrorCode::Internal,
                                "threepp renderer construction failed"});
    }
}

ThreeppSceneRenderer::ThreeppSceneRenderer(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}
ThreeppSceneRenderer::~ThreeppSceneRenderer() { shutdown(); }

void ThreeppSceneRenderer::begin_frame() {
    if (!impl_ || !impl_->healthy) return;
    if (impl_->frame_open) {
        impl_->fail(foundation::ErrorCode::InvalidState,
                    "threepp frame began before previous frame ended");
        return;
    }
    if (auto current = impl_->platform.make_gl_current(); !current) {
        impl_->fail(current.error().code, current.error().message);
        return;
    }
    ++impl_->telemetry.frame;
    impl_->telemetry.mesh_uploads = 0U;
    impl_->telemetry.mesh_upload_bytes = 0U;
    impl_->telemetry.palette_updates = 0U;
    impl_->telemetry.draw_calls = 0U;
    impl_->frame_open = true;
}

void ThreeppSceneRenderer::submit(const PresentationSnapshot& snapshot,
                                  const ui::UiRenderFrame& frame) {
    if (!impl_ || !impl_->healthy || !impl_->frame_open) return;
    try {
        impl_->scene->clear();
        const auto& camera = snapshot.camera;
        if (camera.enabled && camera.valid()) {
            const int vw = std::max(1, static_cast<int>(impl_->width * camera.viewport_width));
            const int vh = std::max(1, static_cast<int>(impl_->height * camera.viewport_height));
            const int vx = std::clamp(static_cast<int>(impl_->width * camera.viewport_left),
                                      0, static_cast<int>(impl_->width) - 1);
            const int vy_top = std::clamp(static_cast<int>(impl_->height * camera.viewport_top),
                                          0, static_cast<int>(impl_->height) - 1);
            const int vy = std::max(0, static_cast<int>(impl_->height) - vy_top - vh);
            impl_->renderer->setViewport(vx, vy, vw, vh);
            impl_->camera->fov = camera.vertical_fov * 57.29577951308232F;
            impl_->camera->aspect = static_cast<float>(vw) / static_cast<float>(vh);
            impl_->camera->near = camera.near_plane;
            impl_->camera->far = camera.far_plane;
            impl_->camera->updateProjectionMatrix();
            impl_->camera->position.set(camera.position.x, camera.position.y, camera.position.z);
            impl_->camera->up.set(camera.up.x, camera.up.y, camera.up.z);
            impl_->camera->lookAt(camera.target.x, camera.target.y, camera.target.z);
        } else {
            impl_->renderer->setViewport(0, 0, static_cast<int>(impl_->width),
                                         static_cast<int>(impl_->height));
            impl_->camera->position.set(3.0F, 2.2F, 3.0F);
            impl_->camera->lookAt(0.0F, 1.0F, 0.0F);
            impl_->camera->aspect = static_cast<float>(impl_->width) /
                                    static_cast<float>(impl_->height);
            impl_->camera->updateProjectionMatrix();
        }

        const auto& lights = snapshot.character_lights;
        impl_->hemisphere->color = color(lights.hemisphere.sky);
        impl_->hemisphere->groundColor = color(lights.hemisphere.ground);
        impl_->hemisphere->intensity = lights.hemisphere.intensity;
        const auto setDirectional = [](threepp::DirectionalLight& light,
                                       threepp::Object3D& target,
                                       const DirectionalLight& source) {
            light.color = color(source.color);
            light.intensity = source.intensity;
            target.position.set(0.0F, 1.0F, 0.0F);
            light.position.set(-source.direction.x * 8.0F,
                               1.0F - source.direction.y * 8.0F,
                               -source.direction.z * 8.0F);
        };
        setDirectional(*impl_->key, *impl_->key_target, lights.key);
        setDirectional(*impl_->fill, *impl_->fill_target, lights.fill);
        impl_->scene->add(impl_->hemisphere);
        impl_->scene->add(impl_->key_target);
        impl_->scene->add(impl_->key);
        impl_->scene->add(impl_->fill_target);
        impl_->scene->add(impl_->fill);

        const auto addStandalone = [&](const std::shared_ptr<const RenderMesh>& mesh) {
            if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) return;
            auto geometry = impl_->ensureOrdinary(mesh);
            const auto object_id = foundation::stableHashCombine(
                foundation::stable_id("threepp.direct-mesh"), mesh->mesh_id);
            auto& instance = impl_->ordinary_instances[object_id];
            if (!instance.mesh || instance.mesh_id != mesh->mesh_id ||
                instance.mesh_revision != mesh->revision) {
                instance.mesh = threepp::Mesh::create(geometry, impl_->ordinary_material);
                instance.mesh_id = mesh->mesh_id;
                instance.mesh_revision = mesh->revision;
            }
            instance.last_seen = impl_->telemetry.frame;
            instance.mesh->position.set(0,0,0);
            instance.mesh->scale.set(1,1,1);
            instance.mesh->rotation.set(0,0,0);
            impl_->scene->add(instance.mesh);
            ++impl_->telemetry.draw_calls;
        };
        addStandalone(snapshot.terrain_mesh);
        addStandalone(snapshot.world_mesh);

        for (const auto& instance_data : snapshot.instances) {
            if (const auto* skinned_owner = impl_->skinnedPrototype(snapshot, instance_data.mesh_id)) {
                const auto& prototype_owner = *skinned_owner;
                const auto* pose = impl_->palette(snapshot, instance_data.object_id);
                if (!pose) {
                    impl_->fail(foundation::ErrorCode::InvalidState,
                                "threepp skinned instance has no local pose");
                    return;
                }
                auto geometry = impl_->ensureSkinned(prototype_owner);
                auto& instance = impl_->skinned_instances[instance_data.object_id];
                if (!instance.mesh || instance.mesh_id != prototype_owner->mesh_id ||
                    instance.mesh_revision != prototype_owner->revision) {
                    instance = {};
                    instance.mesh_id = prototype_owner->mesh_id;
                    instance.mesh_revision = prototype_owner->revision;
                    instance.mesh = threepp::SkinnedMesh::create(
                        geometry, impl_->character_materials.front());
                    instance.mesh->setMaterials(impl_->character_materials);
                    instance.bones.reserve(prototype_owner->bones.size());
                    for (std::size_t bone = 0U; bone < prototype_owner->bones.size(); ++bone) {
                        auto node = threepp::Bone::create();
                        node->name = "bone-" + std::to_string(bone);
                        setBoneTransform(*node, prototype_owner->bones[bone].local_bind);
                        instance.bones.push_back(std::move(node));
                    }
                    for (std::size_t bone = 0U; bone < prototype_owner->bones.size(); ++bone) {
                        const auto parent = prototype_owner->bones[bone].parent;
                        if (parent == 0xffffU) {
                            instance.mesh->add(instance.bones[bone]);
                        } else if (parent < instance.bones.size()) {
                            instance.bones[parent]->add(instance.bones[bone]);
                        } else {
                            impl_->fail(foundation::ErrorCode::InvalidState,
                                        "threepp skeleton parent is out of range");
                            return;
                        }
                    }
                    instance.mesh->updateMatrixWorld(true);
                    instance.skeleton = threepp::Skeleton::create(instance.bones);
                    threepp::Matrix4 bind;
                    bind.identity();
                    instance.mesh->bind(instance.skeleton, bind);
                    instance.mesh->morphTargetInfluences().assign(
                        prototype_owner->morph_target_count, 0.0F);
                }
                instance.last_seen = impl_->telemetry.frame;
                if (!pose->local_poses.empty() &&
                    pose->local_poses.size() != instance.bones.size()) {
                    impl_->fail(foundation::ErrorCode::InvalidState,
                                "threepp local pose does not match skeleton");
                    return;
                }
                for (std::size_t bone = 0U; bone < instance.bones.size(); ++bone) {
                    const auto& transform = pose->local_poses.empty()
                        ? prototype_owner->bones[bone].local_bind
                        : pose->local_poses[bone];
                    setBoneTransform(*instance.bones[bone], transform);
                }
                auto& influences = instance.mesh->morphTargetInfluences();
                for (std::size_t morph = 0U; morph < influences.size(); ++morph)
                    influences[morph] = morph < pose->morph_weights.size()
                        ? pose->morph_weights[morph] : 0.0F;
                setTransform(*instance.mesh, instance_data);
                impl_->scene->add(instance.mesh);
                ++impl_->telemetry.palette_updates;
                ++impl_->telemetry.draw_calls;
                continue;
            }
            if (const auto* ordinary_owner = impl_->ordinaryPrototype(snapshot, instance_data.mesh_id)) {
                auto& instance = impl_->ensureOrdinaryInstance(instance_data, *ordinary_owner);
                impl_->scene->add(instance.mesh);
                ++impl_->telemetry.draw_calls;
            }
        }

        if (!snapshot.debug_lines.empty()) {
            std::vector<float> positions, colors;
            positions.reserve(snapshot.debug_lines.size() * 6U);
            colors.reserve(snapshot.debug_lines.size() * 6U);
            for (const auto& line : snapshot.debug_lines) {
                positions.insert(positions.end(), {line.start.x,line.start.y,line.start.z,
                                                   line.end.x,line.end.y,line.end.z});
                colors.insert(colors.end(), {line.color.r,line.color.g,line.color.b,
                                             line.color.r,line.color.g,line.color.b});
            }
            auto geometry = threepp::BufferGeometry::create();
            geometry->setAttribute("position",
                share(threepp::FloatBufferAttribute::create(std::move(positions), 3)));
            geometry->setAttribute("color",
                share(threepp::FloatBufferAttribute::create(std::move(colors), 3)));
            auto lines = threepp::LineSegments::create(geometry, impl_->debug_material);
            impl_->scene->add(lines);
            ++impl_->telemetry.draw_calls;
        }

        impl_->renderer->render(*impl_->scene, *impl_->camera);
        impl_->renderer->resetState();
        if (auto ui_result = impl_->ui.draw(frame, impl_->width, impl_->height); !ui_result) {
            impl_->fail(ui_result.error().code, ui_result.error().message);
            return;
        }
        impl_->renderer->resetState();
        impl_->prune();
    } catch (...) {
        impl_->fail(foundation::ErrorCode::Internal, "threepp scene submission failed");
    }
}

void ThreeppSceneRenderer::end_frame() {
    if (!impl_ || !impl_->frame_open) return;
    if (impl_->healthy) {
        if (auto swapped = impl_->platform.swap_gl_window(); !swapped)
            impl_->fail(swapped.error().code, swapped.error().message);
    }
    impl_->frame_open = false;
}

RenderCapabilities ThreeppSceneRenderer::capabilities() const noexcept {
    return impl_ ? impl_->caps : RenderCapabilities{};
}
RenderUploadTelemetry ThreeppSceneRenderer::uploadTelemetry() const noexcept {
    return impl_ ? impl_->telemetry : RenderUploadTelemetry{};
}
bool ThreeppSceneRenderer::healthy() const noexcept { return impl_ && impl_->healthy; }
foundation::Error ThreeppSceneRenderer::last_error() const noexcept {
    return impl_ ? impl_->error : foundation::Error{foundation::ErrorCode::InvalidState,
                                                     "threepp renderer is not initialized"};
}

foundation::Result<void, foundation::Error> ThreeppSceneRenderer::resize(
    std::uint32_t width, std::uint32_t height) noexcept {
    using Result = foundation::Result<void, foundation::Error>;
    if (!impl_ || width == 0U || height == 0U)
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "invalid threepp framebuffer size"});
    try {
        impl_->width = width;
        impl_->height = height;
        impl_->renderer->setSize({static_cast<int>(width), static_cast<int>(height)});
        impl_->camera->aspect = static_cast<float>(width) / static_cast<float>(height);
        impl_->camera->updateProjectionMatrix();
        return Result::success();
    } catch (...) {
        return Result::failure({foundation::ErrorCode::Internal, "threepp resize failed"});
    }
}

foundation::Result<void, foundation::Error> ThreeppSceneRenderer::capture(
    const std::filesystem::path& path) noexcept {
    using Result = foundation::Result<void, foundation::Error>;
    if (!impl_ || path.empty())
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "invalid threepp capture path"});
    try {
        impl_->renderer->writeFramebuffer(path);
        return Result::success();
    } catch (...) {
        return Result::failure({foundation::ErrorCode::Internal, "threepp capture failed"});
    }
}

void ThreeppSceneRenderer::shutdown() noexcept {
    if (!impl_) return;
    if (impl_->platform.gl_context_current() || impl_->platform.make_gl_current()) {
        impl_->ui.shutdown();
        impl_->skinned_instances.clear();
        impl_->ordinary_instances.clear();
        impl_->skinned_geometry.clear();
        impl_->ordinary_geometry.clear();
        impl_->scene.reset();
        impl_->camera.reset();
        impl_->hemisphere.reset();
        impl_->key.reset();
        impl_->fill.reset();
        impl_->key_target.reset();
        impl_->fill_target.reset();
        impl_->debug_material.reset();
        impl_->character_materials.clear();
        impl_->ordinary_material.reset();
        impl_->renderer.reset();
    }
    impl_.reset();
}

} // namespace genomes::render
