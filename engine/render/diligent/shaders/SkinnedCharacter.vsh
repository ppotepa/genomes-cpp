struct VSInput {
    float3 Position:ATTRIB0;float3 Normal:ATTRIB1;float2 UV:ATTRIB2;float4 Color:ATTRIB3;
    float4 BoneIndices:ATTRIB4;float4 BoneWeights:ATTRIB5;
    float3 MorphPosition0:ATTRIB6;float3 MorphPosition1:ATTRIB7;float3 MorphPosition2:ATTRIB8;float3 MorphPosition3:ATTRIB9;
    float3 MorphNormal0:ATTRIB10;float3 MorphNormal1:ATTRIB11;float3 MorphNormal2:ATTRIB12;float3 MorphNormal3:ATTRIB13;
    uint MaterialRegion:ATTRIB14;
    float4 InstancePositionScale:ATTRIB15;float4 InstanceScaleRotation:ATTRIB16;
    float4 InstanceTint:ATTRIB17;float4 InstanceMorphWeights:ATTRIB18;
    uint PaletteIndex:ATTRIB19;uint DebugWeightBone:ATTRIB20;
};
SurfacePixel main(VSInput input) {
    float3 p=input.Position+input.MorphPosition0*input.InstanceMorphWeights.x+input.MorphPosition1*input.InstanceMorphWeights.y+
        input.MorphPosition2*input.InstanceMorphWeights.z+input.MorphPosition3*input.InstanceMorphWeights.w;
    float3 n=input.Normal+input.MorphNormal0*input.InstanceMorphWeights.x+input.MorphNormal1*input.InstanceMorphWeights.y+
        input.MorphNormal2*input.InstanceMorphWeights.z+input.MorphNormal3*input.InstanceMorphWeights.w;
    float3 sp=0,sn=0;float sum=0,heat=0;
    [unroll] for(uint k=0;k<GENOMES_SKINNED_LAYOUT_PROFILE_INFLUENCE_COUNT;++k) {
        uint bone=(uint)input.BoneIndices[k];float w=input.BoneWeights[k];
        if(w>0 && bone<GENOMES_SKINNED_LAYOUT_PROFILE_BONE_COUNT) {
            sp+=mul(BonePaletteBuffer[input.PaletteIndex+bone],float4(p,1)).xyz*w;
            sn+=transformSkinNormal((float3x3)BonePaletteBuffer[input.PaletteIndex+bone],n)*w;
            sum+=w;if(bone==input.DebugWeightBone)heat+=w;
        }
    }
    if(sum<=0){sp=p;sn=n;}else if(sum<1){sp+=p*(1-sum);sn+=n*(1-sum);}else{sp/=sum;sn/=sum;heat/=sum;}
    sp*=input.InstanceScaleRotation.xyz;sn/=input.InstanceScaleRotation.xyz;
    float c=cos(input.InstanceScaleRotation.w),s=sin(input.InstanceScaleRotation.w);
    SurfacePixel o;
    o.WorldPosition=float3(c*sp.x-s*sp.z,sp.y,s*sp.x+c*sp.z)+input.InstancePositionScale.xyz;
    o.Position=mul(SceneViewProjection,float4(o.WorldPosition,1));
    o.Normal=safeNormal(float3(c*sn.x-s*sn.z,sn.y,s*sn.x+c*sn.z));
    o.Color=input.Color;o.Tint=input.InstanceTint;o.UV=input.UV;
    o.Weight=input.DebugWeightBone==0xffffffff?-1:saturate(heat);
    return o;
}
