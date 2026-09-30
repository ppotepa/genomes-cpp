struct VSInput {
    float3 Position:ATTRIB0;float3 Normal:ATTRIB1;float2 UV:ATTRIB2;float4 Color:ATTRIB3;
    float4 InstancePositionRotation:ATTRIB4;float4 InstanceScale:ATTRIB5;float4 InstanceTint:ATTRIB6;
};
SurfacePixel main(VSInput input) {
    float3 p=input.Position*input.InstanceScale.xyz;
    float3 n=input.Normal/input.InstanceScale.xyz;
    float c=cos(input.InstancePositionRotation.w),s=sin(input.InstancePositionRotation.w);
    SurfacePixel o;
    o.WorldPosition=float3(c*p.x-s*p.z,p.y,s*p.x+c*p.z)+input.InstancePositionRotation.xyz;
    o.Position=mul(SceneViewProjection,float4(o.WorldPosition,1));
    o.Normal=safeNormal(float3(c*n.x-s*n.z,n.y,s*n.x+c*n.z));
    o.Color=input.Color;o.Tint=input.InstanceTint;o.UV=input.UV;o.Weight=-1;
    return o;
}
