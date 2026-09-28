#pragma once

// Private backend shader sources. Character shaders remain versioned .vsh/.psh
// assets; these small UI/debug/terrain shaders have no domain dependencies.
namespace genomes::render::diligent_builtin {
inline constexpr char ui_vs[] = R"HLSL(
struct In { float2 Position:ATTRIB0; float4 Color:ATTRIB1; };
struct Out { float4 Position:SV_POSITION; float4 Color:COLOR0; };
Out main(In v) { Out o; o.Position=float4(v.Position,0,1); o.Color=v.Color; return o; }
)HLSL";
inline constexpr char color_ps[] = R"HLSL(
struct In { float4 Position:SV_POSITION; float4 Color:COLOR0; };
float4 main(In v):SV_TARGET { return v.Color; }
)HLSL";
inline constexpr char debug_vs[] = R"HLSL(
cbuffer CameraConstants { column_major float4x4 ViewProjection; };
struct In { float3 Position:ATTRIB0; float4 Color:ATTRIB1; };
struct Out { float4 Position:SV_POSITION; float4 Color:COLOR0; };
Out main(In v) { Out o; o.Position=mul(ViewProjection,float4(v.Position,1)); o.Color=v.Color; return o; }
)HLSL";
inline constexpr char terrain_vs[] = R"HLSL(
cbuffer CameraConstants { column_major float4x4 ViewProjection; };
struct In { float3 Position:ATTRIB0; float3 Normal:ATTRIB1; float2 UV:ATTRIB2; float4 Color:ATTRIB3; };
struct Out { float4 Position:SV_POSITION; float3 Normal:NORMAL0; float2 UV:TEXCOORD0; float4 Color:COLOR0; };
Out main(In v) {
    Out o; o.Position=mul(ViewProjection,float4(v.Position,1));
    o.Normal=v.Normal; o.UV=v.UV; o.Color=v.Color; return o;
}
)HLSL";
inline constexpr char terrain_ps[] = R"HLSL(
struct In { float4 Position:SV_POSITION; float3 Normal:NORMAL0; float2 UV:TEXCOORD0; float4 Color:COLOR0; };
float4 main(In v):SV_TARGET {
    float l=saturate(dot(normalize(v.Normal),normalize(float3(-0.45,0.85,-0.35))));
    float bands=0.92+0.08*sin(v.UV.x*36+v.UV.y*21);
    return float4(v.Color.rgb*bands*(0.35+0.65*l),v.Color.a);
}
)HLSL";
inline constexpr char instance_vs[] = R"HLSL(
cbuffer CameraConstants { column_major float4x4 ViewProjection; };
cbuffer InstancePassConstants { uint4 RequiredOptions; uint4 RequiredMeshId; uint4 RequiredMaterialId; };
struct InstanceRecord { uint4 Data0; uint4 Data1; uint4 Data2; uint4 Data3; };
StructuredBuffer<InstanceRecord> Instances;
StructuredBuffer<uint> InstanceIndices;
struct In {
    float3 Position:ATTRIB0; float3 Normal:ATTRIB1; float2 UV:ATTRIB2;
    float4 Color:ATTRIB3; uint MaterialRegion:ATTRIB4;
};
struct Out { float4 Position:SV_POSITION; float3 Normal:NORMAL0; float4 Color:COLOR0; };
Out main(In v, uint id:SV_InstanceID) {
    InstanceRecord r=Instances[InstanceIndices[id]];
    uint2 mesh=r.Data0.zw, material=r.Data1.xy;
    uint flags=r.Data3.y;
    Out o;
    if ((flags&1)==0 || (flags&RequiredOptions.x)==0 ||
        any(mesh!=RequiredMeshId.xy) ||
        (RequiredOptions.y!=0 && any(material!=RequiredMaterialId.xy))) {
        o.Position=float4(0,0,2,1); o.Normal=float3(0,1,0); o.Color=0; return o;
    }
    float3 p=float3(asfloat(r.Data1.z),asfloat(r.Data1.w),asfloat(r.Data2.x));
    float3 s=float3(asfloat(r.Data2.y),asfloat(r.Data2.z),asfloat(r.Data2.w));
    float a=asfloat(r.Data3.x), c=cos(a), sn=sin(a);
    float3 local=v.Position*s;
    float3 n=v.Normal/s;
    o.Position=mul(ViewProjection,float4(float3(local.x*c-local.z*sn,local.y,local.x*sn+local.z*c)+p,1));
    o.Normal=normalize(float3(n.x*c-n.z*sn,n.y,n.x*sn+n.z*c));
    o.Color=v.Color;
    // Team coloration is restricted to cloth; skin, eyes and hair retain identity.
    if ((flags&4)!=0 && (v.MaterialRegion==0 || v.MaterialRegion==11)) {
        float3 team=(flags&8)!=0 ? float3(0.82,0.22,0.18) : float3(0.18,0.42,0.88);
        o.Color.rgb=lerp(o.Color.rgb,team,0.35);
    }
    return o;
}
)HLSL";
inline constexpr char instance_ps[] = R"HLSL(
struct In { float4 Position:SV_POSITION; float3 Normal:NORMAL0; float4 Color:COLOR0; };
float4 main(In v):SV_TARGET {
    float l=saturate(dot(normalize(v.Normal),normalize(float3(-0.35,0.8,-0.25))));
    return float4(v.Color.rgb*(0.35+0.65*l),v.Color.a);
}
)HLSL";
} // namespace genomes::render::diligent_builtin
