struct VSInput {
    float3 Position:ATTRIB0;
    float3 Normal:ATTRIB1;
    float2 UV:ATTRIB2;
    float4 Color:ATTRIB3;
    float4 BoneIndices:ATTRIB4;
    float4 BoneWeights:ATTRIB5;
    float3 MorphPosition0:ATTRIB6;
    float3 MorphPosition1:ATTRIB7;
    float3 MorphPosition2:ATTRIB8;
    float3 MorphPosition3:ATTRIB9;
    float3 MorphNormal0:ATTRIB10;
    float3 MorphNormal1:ATTRIB11;
    float3 MorphNormal2:ATTRIB12;
    float3 MorphNormal3:ATTRIB13;
    uint MaterialRegion:ATTRIB14;
};
struct VSOutput {
    float4 Position:SV_POSITION;
    float3 Normal:NORMAL0;
    float4 Color:COLOR0;
    float3 WorldPosition:TEXCOORD0;
    float2 UV:TEXCOORD1;
    nointerpolation uint MaterialRegion:TEXCOORD2;
    float Weight:TEXCOORD3;
};
VSOutput main(VSInput input) {
    float3 p=input.Position+input.MorphPosition0*MorphWeights.x+
        input.MorphPosition1*MorphWeights.y+input.MorphPosition2*MorphWeights.z+
        input.MorphPosition3*MorphWeights.w;
    float3 n=input.Normal+input.MorphNormal0*MorphWeights.x+
        input.MorphNormal1*MorphWeights.y+input.MorphNormal2*MorphWeights.z+
        input.MorphNormal3*MorphWeights.w;
    float3 sp=0, sn=0;
    float total=0, heat=0;
    [unroll] for (uint i=0;i<4;++i) {
        uint bone=(uint)input.BoneIndices[i];
        float w=input.BoneWeights[i];
        if (w>0 && bone<69) {
            sp+=mul(BonePalette[bone],float4(p,1)).xyz*w;
            // Native rig poses are rigid: model dimensions are baked in bind
            // geometry, never introduced as non-uniform runtime bone scale.
            sn+=mul((float3x3)BonePalette[bone],n)*w;
            total+=w;
            if ((int)bone==(int)ObjectPositionScale.w) heat+=w;
        }
    }
    if (total<=0) { sp=p; sn=n; }
    else if (total<1) { sp+=p*(1-total); sn+=n*(1-total); }
    else if (total>1) { sp/=total; sn/=total; heat/=total; }
    float3 scale=ObjectScaleRotation.xyz;
    float c=cos(ObjectScaleRotation.w), s=sin(ObjectScaleRotation.w);
    sp*=scale;
    sn/=scale;
    VSOutput o;
    o.WorldPosition=float3(sp.x*c-sp.z*s,sp.y,sp.x*s+sp.z*c)+ObjectPositionScale.xyz;
    o.Position=mul(ViewProjection,float4(o.WorldPosition,1));
    o.Normal=safeNormal(float3(sn.x*c-sn.z*s,sn.y,sn.x*s+sn.z*c));
    o.Color=input.Color; o.UV=input.UV; o.MaterialRegion=input.MaterialRegion;
    o.Weight=saturate(heat);
    return o;
}
