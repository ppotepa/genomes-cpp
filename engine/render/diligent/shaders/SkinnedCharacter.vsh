struct VSInput {
    float3 Position:ATTRIB0;float3 Normal:ATTRIB1;float2 UV:ATTRIB2;float4 Color:ATTRIB3;
    float4 BoneIndices:ATTRIB4;float4 BoneWeights:ATTRIB5;
    float3 MorphPosition0:ATTRIB6;float3 MorphPosition1:ATTRIB7;float3 MorphPosition2:ATTRIB8;float3 MorphPosition3:ATTRIB9;
    float3 MorphNormal0:ATTRIB10;float3 MorphNormal1:ATTRIB11;float3 MorphNormal2:ATTRIB12;float3 MorphNormal3:ATTRIB13;
    uint MaterialRegion:ATTRIB14;
};
SurfacePixel main(VSInput input) {
    float3 p=input.Position+input.MorphPosition0*MorphWeights.x+input.MorphPosition1*MorphWeights.y+
        input.MorphPosition2*MorphWeights.z+input.MorphPosition3*MorphWeights.w;
    float3 n=input.Normal+input.MorphNormal0*MorphWeights.x+input.MorphNormal1*MorphWeights.y+
        input.MorphNormal2*MorphWeights.z+input.MorphNormal3*MorphWeights.w;
    float3 sp=0,sn=0;float sum=0,heat=0;
    [unroll] for(uint k=0;k<GENOMES_SKINNED_LAYOUT_PROFILE_INFLUENCE_COUNT;++k) {
        uint bone=(uint)input.BoneIndices[k];float w=input.BoneWeights[k];
        if(w>0 && bone<GENOMES_SKINNED_LAYOUT_PROFILE_BONE_COUNT) {
            sp+=mul(BonePalette[bone],float4(p,1)).xyz*w;
            sn+=transformSkinNormal((float3x3)BonePalette[bone],n)*w;
            sum+=w;if((int)bone==(int)ObjectPositionScale.w)heat+=w;
        }
    }
    if(sum<=0){sp=p;sn=n;}else if(sum<1){sp+=p*(1-sum);sn+=n*(1-sum);}else{sp/=sum;sn/=sum;heat/=sum;}
    sp*=ObjectScaleRotation.xyz;sn/=ObjectScaleRotation.xyz;
    float c=cos(ObjectScaleRotation.w),s=sin(ObjectScaleRotation.w);
    SurfacePixel o;
    o.WorldPosition=float3(c*sp.x-s*sp.z,sp.y,s*sp.x+c*sp.z)+ObjectPositionScale.xyz;
    o.Position=mul(SceneViewProjection,float4(o.WorldPosition,1));
    o.Normal=safeNormal(float3(c*sn.x-s*sn.z,sn.y,s*sn.x+c*sn.z));
    o.Color=input.Color;o.Tint=DrawTint;o.UV=input.UV;
    o.Weight=ObjectPositionScale.w<0?-1:saturate(heat);
    return o;
}
