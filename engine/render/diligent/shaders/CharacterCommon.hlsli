#ifndef GENOMES_CHARACTER_COMMON_HLSLI
#define GENOMES_CHARACTER_COMMON_HLSLI

// Byte layout is checked against SkinnedPassConstants in DiligentBackend.cpp.
cbuffer SkinnedPassConstants {
    column_major float4x4 ViewProjection;
    float4 ObjectPositionScale; // xyz translation; w selected weight bone, -1 off
    float4 ObjectScaleRotation;
    float4 CameraPosition;
    float4 CharacterKeyDirectionIntensity;
    float4 CharacterKeyColor;
    float4 CharacterFillDirectionIntensity;
    float4 CharacterFillColor;
    float4 CharacterHemisphereSky;
    float4 CharacterHemisphereGround;
    float4 MorphWeights;
    column_major float4x4 BonePalette[69];
};
float3 safeNormal(float3 v) {
    float d=dot(v,v);
    return d>1.0e-12 ? v*rsqrt(d) : float3(0,1,0);
}
float3 characterLight(float3 base, float3 n, float3 world, float roughness, float specular) {
    float3 key=safeNormal(CharacterKeyDirectionIntensity.xyz);
    float3 fill=safeNormal(CharacterFillDirectionIntensity.xyz);
    float sky=saturate(n.y*0.5+0.5);
    float3 ambient=lerp(CharacterHemisphereGround.rgb*CharacterHemisphereGround.a,
                        CharacterHemisphereSky.rgb*CharacterHemisphereSky.a,sky);
    float kl=saturate(dot(n,key))*CharacterKeyDirectionIntensity.w;
    float fl=saturate(dot(n,fill))*CharacterFillDirectionIntensity.w;
    float3 view=safeNormal(CameraPosition.xyz-world);
    float3 h=safeNormal(key+view);
    float spec=pow(saturate(dot(n,h)),lerp(96.0,8.0,roughness))*specular*kl;
    return base*(ambient+kl*CharacterKeyColor.rgb+fl*CharacterFillColor.rgb)+
           spec*CharacterKeyColor.rgb;
}
// HDR-to-display mapping; the sRGB render target performs the sole gamma encode.
float3 characterToneMap(float3 x) {
    x=max(x,0.0);
    return saturate((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14));
}
#endif
