cbuffer SkinnedPassConstants {
    float4x4 ViewProjection;
    float4 ObjectPositionScale;
    float4 ObjectScaleRotation;
    float4 CharacterKeyDirectionIntensity;
    float4 CharacterKeyColor;
    float4 CharacterFillDirectionIntensity;
    float4 CharacterFillColor;
    float4 CharacterHemisphereSky;
    float4 CharacterHemisphereGround;
    float4 MorphWeights;
    float4x4 BonePalette[69];
};

struct VSInput {
    float3 Position : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV : ATTRIB2;
    float4 Color : ATTRIB3;
    float4 BoneIndices : ATTRIB4;
    float4 BoneWeights : ATTRIB5;
    float3 MorphPosition0 : ATTRIB6;
    float3 MorphPosition1 : ATTRIB7;
    float3 MorphPosition2 : ATTRIB8;
    float3 MorphPosition3 : ATTRIB9;
    float3 MorphNormal0 : ATTRIB10;
    float3 MorphNormal1 : ATTRIB11;
    float3 MorphNormal2 : ATTRIB12;
    float3 MorphNormal3 : ATTRIB13;
    uint MaterialRegion : ATTRIB14;
};

struct VSOutput {
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL0;
    float4 Color : COLOR0;
    nointerpolation uint MaterialRegion : TEXCOORD1;
    float4 CharacterKeyDirectionIntensity : TEXCOORD2;
    float4 CharacterKeyColor : TEXCOORD3;
    float4 CharacterFillDirectionIntensity : TEXCOORD4;
    float4 CharacterFillColor : TEXCOORD5;
    float4 CharacterHemisphereSky : TEXCOORD6;
    float4 CharacterHemisphereGround : TEXCOORD7;
};

VSOutput main(VSInput input) {
    float3 position = input.Position;
    float3 normal = input.Normal;
    position += input.MorphPosition0 * MorphWeights.x;
    position += input.MorphPosition1 * MorphWeights.y;
    position += input.MorphPosition2 * MorphWeights.z;
    position += input.MorphPosition3 * MorphWeights.w;
    normal += input.MorphNormal0 * MorphWeights.x;
    normal += input.MorphNormal1 * MorphWeights.y;
    normal += input.MorphNormal2 * MorphWeights.z;
    normal += input.MorphNormal3 * MorphWeights.w;
    float4 skinned_position = float4(0.0, 0.0, 0.0, 0.0);
    float3 skinned_normal = float3(0.0, 0.0, 0.0);
    float total_weight = 0.0;
    [unroll]
    for (uint i = 0; i < 4; ++i) {
        uint bone = (uint)input.BoneIndices[i];
        float weight = input.BoneWeights[i];
        if (weight > 0.0 && bone < 69) {
            skinned_position += mul(BonePalette[bone], float4(position, 1.0)) * weight;
            // The native animation contract uses uniform bone scale. A
            // non-uniform scale requires an inverse-transpose normal matrix.
            skinned_normal += mul((float3x3)BonePalette[bone], normal) * weight;
            total_weight += weight;
        }
    }
    if (total_weight <= 1.0e-6) {
        skinned_position = float4(position, 1.0);
        skinned_normal = normal;
    } else if (total_weight < 1.0) {
        skinned_position += float4(position, 1.0) * (1.0 - total_weight);
        skinned_normal += normal * (1.0 - total_weight);
    } else if (total_weight > 1.0) {
        skinned_position /= total_weight;
        skinned_normal /= total_weight;
    }
    float3 scale = ObjectScaleRotation.xyz;
    float cosine = cos(ObjectScaleRotation.w);
    float sine = sin(ObjectScaleRotation.w);
    float3 scaled = skinned_position.xyz * scale;
    float3 rotated = float3(scaled.x * cosine - scaled.z * sine,
                             scaled.y,
                             scaled.x * sine + scaled.z * cosine);
    float3 rotated_normal = float3(skinned_normal.x * cosine - skinned_normal.z * sine,
                                   skinned_normal.y,
                                   skinned_normal.x * sine + skinned_normal.z * cosine);
    VSOutput output;
    output.Position = mul(ViewProjection,
                           float4(rotated + ObjectPositionScale.xyz, 1.0));
    output.Normal = normalize(rotated_normal);
    output.Color = input.Color;
    output.MaterialRegion = input.MaterialRegion;
    output.CharacterKeyDirectionIntensity = CharacterKeyDirectionIntensity;
    output.CharacterKeyColor = CharacterKeyColor;
    output.CharacterFillDirectionIntensity = CharacterFillDirectionIntensity;
    output.CharacterFillColor = CharacterFillColor;
    output.CharacterHemisphereSky = CharacterHemisphereSky;
    output.CharacterHemisphereGround = CharacterHemisphereGround;
    return output;
}
