// Retains the complete audited 4640-byte ABI; unused lighting fields remain reserved.
cbuffer SkinnedPassConstants {
    column_major float4x4 ViewProjection;
    float4 ObjectPositionScale;
    float4 ObjectScaleRotation;
    float4 CameraPosition;
    float4 CharacterKeyDirectionIntensity;
    float4 CharacterKeyColor;
    float4 CharacterFillDirectionIntensity;
    float4 CharacterFillColor;
    float4 CharacterHemisphereSky;
    float4 CharacterHemisphereGround;
    float4 MorphWeights;
    column_major float4x4 BonePalette[GENOMES_SKINNED_LAYOUT_PROFILE_BONE_COUNT];
};
StructuredBuffer<float4x4> BonePaletteBuffer;
static const uint BonePaletteIndex = (uint)CharacterKeyDirectionIntensity.x;
float3 transformSkinNormal(float3x3 m,float3 n) {
    float3 c0=cross(m[1],m[2]),c1=cross(m[2],m[0]),c2=cross(m[0],m[1]);
    float determinant=dot(m[0],c0);
    return abs(determinant)>1e-10?mul(float3x3(c0,c1,c2),n)/determinant:mul(m,n);
}
