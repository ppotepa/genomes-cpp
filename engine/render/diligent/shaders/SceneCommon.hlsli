// CPU offsets: DiligentGpuContracts.hpp. Matrices are column-major, multiplied M * p.
cbuffer SceneConstants {
    column_major float4x4 SceneViewProjection;
    column_major float4x4 ShadowUVProjection;
    float4 SceneCameraPosition;
    float4 KeyDirectionIntensity;
    float4 KeyColor;
    float4 FillDirectionIntensity;
    float4 FillColor;
    float4 HemisphereSky;
    float4 HemisphereGround;
    float4 ShadowParameters; // bias, inverse size, enabled, shadow pass
};
cbuffer MaterialConstants {
    float4 BaseColor;
    float4 MaterialFactors; // roughness, metalness, opacity, mask cutoff
    float4 DrawTint;
    uint4 MaterialFlags; // vertex color, tint, alpha mode, receive shadow
};
struct SurfacePixel {
    float4 Position:SV_POSITION;
    float3 WorldPosition:TEXCOORD0;
    float3 Normal:NORMAL0;
    float2 UV:TEXCOORD1;
    float4 Color:COLOR0;
    nointerpolation float4 Tint:COLOR1;
    float Weight:TEXCOORD2;
};
float3 safeNormal(float3 v) { return dot(v,v)>1e-16?v*rsqrt(dot(v,v)):float3(0,1,0); }
float4 surfaceColor(SurfacePixel p) {
    return BaseColor*(MaterialFlags.x!=0?p.Color:float4(1,1,1,1))*(MaterialFlags.y!=0?p.Tint:float4(1,1,1,1));
}
float surfaceAlpha(SurfacePixel p) { return saturate(surfaceColor(p).a*MaterialFactors.z); }
