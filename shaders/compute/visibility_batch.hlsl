// Optional visibility proxy kernel for Chapter 21.3.
//
// This shader consumes a compact, revisioned AABB proxy.  It is deliberately
// not compiled by the native CMake build; a render backend owns resource
// creation, upload/fence ordering and shader compilation.  CPU WorldQuery LOS
// remains authoritative whenever the proxy is unavailable, stale or near a
// boundary.

struct VisibilityRequest {
    float3 eye;
    float3 aim;
    uint observer_id;
    uint target_id;
    uint collision_mask;
    uint padding;
};

struct VisibilityProxy {
    float3 minimum;
    float3 maximum;
    uint semantic_id;
    uint source_kind;
    uint collision_mask;
    uint padding0;
};

struct VisibilityResult {
    uint visible;
    uint ambiguous;
    uint blocker_id;
    float hit_distance;
    uint world_revision_low;
    uint world_revision_high;
};

StructuredBuffer<VisibilityRequest> requests : register(t0);
StructuredBuffer<VisibilityProxy> proxies : register(t1);
RWStructuredBuffer<VisibilityResult> results : register(u0);

cbuffer VisibilityConstants : register(b0) {
    uint request_count;
    uint proxy_count;
    uint world_revision_low;
    uint world_revision_high;
    float boundary_epsilon;
};

bool segment_aabb(float3 origin, float3 delta, VisibilityProxy proxy, out float enter) {
    float minimum_t = 0.0f;
    float maximum_t = 1.0f;
    const float3 safe_delta = float3(
        abs(delta.x) < 1.0e-8f ? (delta.x < 0.0f ? -1.0e-8f : 1.0e-8f) : delta.x,
        abs(delta.y) < 1.0e-8f ? (delta.y < 0.0f ? -1.0e-8f : 1.0e-8f) : delta.y,
        abs(delta.z) < 1.0e-8f ? (delta.z < 0.0f ? -1.0e-8f : 1.0e-8f) : delta.z);
    const float3 inverse_delta = 1.0f / safe_delta;
    const float3 near_value = (proxy.minimum - origin) * inverse_delta;
    const float3 far_value = (proxy.maximum - origin) * inverse_delta;
    const float3 near_t = min(near_value, far_value);
    const float3 far_t = max(near_value, far_value);
    minimum_t = max(minimum_t, max(near_t.x, max(near_t.y, near_t.z)));
    maximum_t = min(maximum_t, min(far_t.x, min(far_t.y, far_t.z)));
    enter = minimum_t;
    return minimum_t <= maximum_t;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    const uint index = dispatch_thread_id.x;
    if (index >= request_count) {
        return;
    }
    const VisibilityRequest request = requests[index];
    const float3 delta = request.aim - request.eye;
    const float length = max(1.0e-8f, sqrt(dot(delta, delta)));
    VisibilityResult output;
    output.visible = 1u;
    output.ambiguous = 0u;
    output.blocker_id = 0u;
    output.hit_distance = length;
    output.world_revision_low = world_revision_low;
    output.world_revision_high = world_revision_high;

    float nearest = 1.0f;
    for (uint proxy_index = 0u; proxy_index < proxy_count; ++proxy_index) {
        const VisibilityProxy proxy = proxies[proxy_index];
        if ((proxy.collision_mask & request.collision_mask) == 0u ||
            proxy.semantic_id == request.observer_id) {
            continue;
        }
        float enter = 0.0f;
        if (!segment_aabb(request.eye, delta, proxy, enter) || enter > nearest) {
            continue;
        }
        if (proxy.semantic_id == request.target_id) {
            output.hit_distance = enter * length;
            nearest = enter;
            continue;
        }
        output.visible = 0u;
        output.blocker_id = proxy.semantic_id;
        output.hit_distance = enter * length;
        nearest = enter;
    }
    // Proxy precision and terrain/rubble representation can disagree at a
    // boundary.  A backend should increase this flag around its tolerance and
    // let the CPU WorldQuery path confirm the request.
    if (nearest <= boundary_epsilon || nearest >= 1.0f - boundary_epsilon) {
        output.ambiguous = 1u;
    }
    results[index] = output;
}
