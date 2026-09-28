// Backend-neutral clear/fill kernel for a layered field atlas.
// Descriptor/resource binding is intentionally supplied by the render backend;
// this source contains no Diligent-specific declarations.

cbuffer FieldAtlasClearConstants : register(b0)
{
    uint2 atlas_dimensions;
    uint atlas_layer;
    float clear_value;
};

RWTexture2DArray<float> field_atlas : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= atlas_dimensions.x ||
        dispatch_thread_id.y >= atlas_dimensions.y)
    {
        return;
    }

    field_atlas[uint3(dispatch_thread_id.xy, atlas_layer)] = clear_value;
}
