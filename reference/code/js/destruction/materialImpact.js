(function(){
  'use strict';
  const R=globalThis.RTS=globalThis.RTS||{},V=R.DestructionSolid.V;
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  const freeze=o=>{for(const v of Object.values(o))if(v&&typeof v==='object'&&!Object.isFrozen(v))freeze(v);return Object.freeze(o);};

  // Gameplay response classes. These coefficients tune relationships between
  // projectile classes and materials; they are not firing-table or certification data.
  const material=freeze({
    concrete:{bulkResistance:1.80,coupling:.88,front:.23,crush:.24,radial:.25,rear:.20,ejecta:.08,frontScale:2.8,rearScale:4.1,stressScale:8.5,channelScale:1.08,zoneRetention:.76,mode:'concrete-scab'},
    brick:{bulkResistance:2.50,coupling:.96,front:.20,crush:.18,radial:.31,rear:.22,ejecta:.09,frontScale:3.2,rearScale:4.8,stressScale:10.5,channelScale:1.12,zoneRetention:.84,mode:'masonry-break',masonry:true},
    steel:{bulkResistance:1.0,coupling:.70,front:.05,crush:.06,radial:.05,rear:.12,plastic:.52,ejecta:.20,frontScale:1.18,rearScale:1.45,stressScale:2.1,channelScale:1.02,zoneRetention:.42,mode:'ductile-hole'},
    armor:{bulkResistance:1.0,coupling:.66,front:.04,crush:.08,radial:.04,rear:.10,plastic:.56,ejecta:.18,frontScale:1.12,rearScale:1.34,stressScale:1.8,channelScale:1.01,zoneRetention:.36,mode:'ductile-hole'},
    wood:{bulkResistance:1.35,coupling:.80,front:.12,crush:.20,radial:.36,rear:.17,plastic:.04,ejecta:.11,frontScale:1.8,rearScale:2.4,stressScale:7.0,channelScale:1.08,zoneRetention:.70,mode:'grain-split',grain:true},
    glass:{bulkResistance:.70,coupling:.96,front:.05,crush:.06,radial:.61,rear:.12,ejecta:.16,frontScale:1.5,rearScale:2.8,stressScale:8.0,channelScale:1.04,zoneRetention:.82,mode:'shatter'},
    rock:{bulkResistance:1.50,coupling:.88,front:.21,crush:.25,radial:.24,rear:.20,ejecta:.10,frontScale:2.5,rearScale:3.4,stressScale:6.5,channelScale:1.08,zoneRetention:.74,mode:'mineral-scab'},
    tissue:{bulkResistance:1.0,coupling:.55,front:.05,crush:.55,radial:.20,rear:.05,ejecta:.15,frontScale:1.0,rearScale:1.1,stressScale:2.0,channelScale:1.0,zoneRetention:.15,mode:'soft'},
    foliage:{bulkResistance:.80,coupling:.30,front:.02,crush:.45,radial:.28,rear:.02,ejecta:.23,frontScale:1.0,rearScale:1.0,stressScale:1.5,channelScale:1.0,zoneRetention:.10,mode:'soft'}
  });

  const interactions=freeze({
    ball:{
      concrete:{penetrationScale:1.08,couplingScale:1.04},brick:{penetrationScale:1.02,couplingScale:1.05},steel:{penetrationScale:1.18,couplingScale:.95},
      armor:{penetrationScale:1.20,couplingScale:.92},wood:{penetrationScale:.95,couplingScale:1.0},glass:{penetrationScale:.72,couplingScale:1.0},
      rock:{penetrationScale:1.10,couplingScale:1.0},tissue:{penetrationScale:.92,couplingScale:.92},foliage:{penetrationScale:.90,couplingScale:.80}
    },
    ap:{
      concrete:{penetrationScale:1.05,couplingScale:1.08},brick:{penetrationScale:2.00,couplingScale:1.14},steel:{penetrationScale:.68,couplingScale:.82},
      armor:{penetrationScale:.76,couplingScale:.86},wood:{penetrationScale:1.00,couplingScale:.92},glass:{penetrationScale:.65,couplingScale:1.0},
      rock:{penetrationScale:1.10,couplingScale:1.02},tissue:{penetrationScale:.86,couplingScale:.82},foliage:{penetrationScale:.90,couplingScale:.70}
    },
    he:{
      concrete:{penetrationScale:1.05,couplingScale:1.0},brick:{penetrationScale:1.0,couplingScale:1.0},steel:{penetrationScale:1.08,couplingScale:1.0},
      armor:{penetrationScale:1.12,couplingScale:1.0},wood:{penetrationScale:1.0,couplingScale:1.0},glass:{penetrationScale:.8,couplingScale:1.0},
      rock:{penetrationScale:1.08,couplingScale:1.0},tissue:{penetrationScale:.9,couplingScale:1.0},foliage:{penetrationScale:.9,couplingScale:1.0}
    },
    fragment:{
      concrete:{penetrationScale:1.18,couplingScale:.55},brick:{penetrationScale:1.12,couplingScale:.62},steel:{penetrationScale:1.28,couplingScale:.48},
      armor:{penetrationScale:1.35,couplingScale:.45},wood:{penetrationScale:1.02,couplingScale:.70},glass:{penetrationScale:.84,couplingScale:.75},
      rock:{penetrationScale:1.22,couplingScale:.55},tissue:{penetrationScale:.95,couplingScale:.70},foliage:{penetrationScale:.92,couplingScale:.65}
    }
  });

  function dominantMaterial(part){
    if(part?.rubbleMixture){let best=part.material||'brick',weight=0;for(const [m,w] of Object.entries(part.rubbleMixture))if(w>weight){best=m;weight=w;}return best;}
    if(!part?.layers?.length)return part?.material||'concrete';
    let best=part.layers.find(l=>!l.void&&l.material!=='air')||part.layers[0];for(const layer of part.layers)if(!layer.void&&layer.material!=='air'&&(layer.thickness??layer.fraction??0)>(best.thickness??best.fraction??0))best=layer;
    return best.material||part.material||'concrete';
  }
  function pair(materialId,ammoKind){
    return interactions[ammoKind]?.[materialId]||interactions.ball[materialId]||{penetrationScale:1,couplingScale:1};
  }
  function resistance(part,ammoKind='ball'){
    const fallback=R.DestructionMaterials[part.material]||R.DestructionMaterials.concrete;
    if(part.rubbleMixture){const packing=clamp(part.packing??.56,.28,.78),porous=clamp(packing*.80,.22,.64);let sum=0,total=0;for(const [m,w] of Object.entries(part.rubbleMixture)){const mat=R.DestructionMaterials[m]||fallback,interaction=pair(m,ammoKind),profile=material[m]||material.concrete;sum+=mat.penetrationWork*interaction.penetrationScale*(profile.bulkResistance||1)*w;total+=w;}return (total?sum/total:fallback.penetrationWork)*porous;}
    if(part.layers?.length)return part.layers.reduce((sum,layer)=>{
      if(layer.void||layer.isVoid)return sum;
      const mat=R.DestructionMaterials[layer.material]||fallback,interaction=pair(layer.material,ammoKind);
      const physicalFraction=layer.thickness&&part.thickness?clamp(layer.thickness/part.thickness,0,1):(layer.fraction??layer.thickness/Math.max(1e-9,part.layers.reduce((sum,l)=>sum+l.thickness,0)));
      return sum+mat.penetrationWork*interaction.penetrationScale*((material[layer.material]||material.concrete).bulkResistance||1)*physicalFraction;
    },0);
    return fallback.penetrationWork*pair(part.material,ammoKind).penetrationScale*((material[part.material]||material.concrete).bulkResistance||1);
  }
  function normalizeShares(profile,penetrated){
    const raw={front:profile.front||0,crush:profile.crush||0,radial:profile.radial||0,rear:penetrated?(profile.rear||0):0,plastic:profile.plastic||0,ejecta:profile.ejecta||0};
    if(!penetrated)raw.front+=profile.rear||0;
    const total=Object.values(raw).reduce((a,b)=>a+b,0)||1;for(const key of Object.keys(raw))raw[key]/=total;return raw;
  }
  function budget({part,ammoKind='ball',lost=0,entryEnergy=0,residualEnergy=0,diameter=.01,penetrated=false,incidence=1,fragment=false}){
    const surfaceMaterial=part.material||'concrete',damageMaterial=dominantMaterial(part),profile=material[damageMaterial]||material.concrete,interaction=pair(damageMaterial,ammoKind),shares=normalizeShares(profile,penetrated);
    const coupling=clamp(profile.coupling*interaction.couplingScale*(fragment?.55:1),.05,1),collateral=Math.max(0,lost)*coupling,energies={};
    for(const [key,share] of Object.entries(shares))energies[key]=collateral*share;
    const allocatedEnergy=Math.min(Math.max(0,lost),Object.values(energies).reduce((s,v)=>s+Math.max(0,v),0));
    const residualRatio=entryEnergy>0?clamp(residualEnergy/entryEnergy,0,1):0,mat=R.DestructionMaterials[damageMaterial]||R.DestructionMaterials.concrete,
      reference=Math.max(1,mat.strength*Math.max(1e-9,diameter**3)*3.5),severity=clamp(Math.sqrt(collateral/reference),0,1.5),
      angleFactor=.72+.28*clamp(incidence,0,1),frontRadius=diameter*profile.frontScale*(1+.38*severity)*angleFactor,
      stressRadius=diameter*profile.stressScale*(1+.45*severity),rearRadius=penetrated?diameter*profile.rearScale*(.68+.72*residualRatio)*(1+.38*severity):0,
      channelRadius=diameter*profile.channelScale*(1+.10*severity);
    let mode=profile.mode;
    if(damageMaterial==='steel'||damageMaterial==='armor')mode=penetrated?(residualRatio<.28?'plug':residualRatio>.58?'petal':'ductile-hole'):'dent';
    return {
      surfaceMaterial,damageMaterial,ammoKind,interaction,profile,coupling,lost:Math.max(0,lost),collateral,
      penetrationDissipation:Math.max(0,lost)-collateral,energies,
      lostEnergy:Math.max(0,lost),allocatedEnergy,energyConserved:allocatedEnergy<=Math.max(0,lost)+1e-9,structuralEnergy:energies.front+energies.crush+energies.radial+energies.rear+energies.plastic,
      damageScore:severity,geometryWork:energies.front+energies.crush+energies.plastic,supportWork:energies.radial+energies.rear,
      residualRatio,severity,mode,frontRadius,stressRadius,rearRadius,channelRadius,
      exitScale:penetrated?clamp(Math.max(channelRadius,rearRadius)/Math.max(1e-9,channelRadius),1,2.35):1,
      frontDepth:diameter*(.7+.9*severity),rearDepth:penetrated?diameter*(.55+.9*severity):0,
      zoneStrength:clamp(profile.zoneRetention*(.45+.55*severity),0,.95)
    };
  }
  function closestBoundsDistance(bounds,point){
    let d2=0;for(let i=0;i<3;i++){const d=point[i]<bounds.min[i]?bounds.min[i]-point[i]:point[i]>bounds.max[i]?point[i]-bounds.max[i]:0;d2+=d*d;}return Math.sqrt(d2);
  }
  function grainAxis(part,direction){
    const g=part?.grainDirection;if(!Array.isArray(g))return null;const d=V.unit(direction),projected=V.sub(g,V.mul(d,V.dot(g,d)));return V.length(projected)>1e-7?V.unit(projected):null;
  }
  function layerIntervals(part,entryPoint,exitPoint){
    const layers=part?.layers||[],delta=V.sub(exitPoint,entryPoint),total=V.length(delta);if(!layers.length||total<1e-12)return [];
    const dir=V.unit(delta),f=part.materialFrame||part.localMaterialFrame;
    // Frames start on the first layer's face; intersection order follows the ray.
    const bounds=part.bounds||{min:entryPoint,max:exitPoint},extent=bounds.max.map((x,i)=>Math.abs(x-bounds.min[i])),axis=extent.indexOf(Math.min(...extent)),fallback=[0,0,0];fallback[axis]=1;const normal=f?.normal||fallback,origin=f?.origin||bounds.min,start=V.dot(V.sub(entryPoint,origin),normal),slope=V.dot(dir,normal),out=[];let cursor=0;
    const fractionSum=layers.reduce((sum,l)=>sum+(Number.isFinite(l.fraction)?l.fraction:0),0),fractionScale=fractionSum>0?total/fractionSum:1;
    for(let i=0;i<layers.length;i++){const l=layers[i],lo=l.start??cursor,span=l.thickness??(Number.isFinite(l.fraction)?l.fraction*fractionScale:total-cursor),hi=l.end??lo+span;cursor=hi;let a=0,b=total;
      if(Math.abs(slope)<1e-10){if(start<lo||start>=hi)continue;}else{const x=(lo-start)/slope,y=(hi-start)/slope;a=Math.max(0,Math.min(x,y));b=Math.min(total,Math.max(x,y));}
      if(b-a>1e-9)out.push({layerId:l.id??i,material:l.material,startDistance:a,endDistance:b,pathLength:b-a,startPoint:V.add(entryPoint,V.mul(dir,a)),endPoint:V.add(entryPoint,V.mul(dir,b)),isVoid:!!l.void||l.material==='air',properties:l.properties||{}});
    }
    out.sort((a,b)=>a.startDistance-b.startDistance);const filled=[];cursor=0;
    for(const l of out){if(l.startDistance>cursor+1e-9)filled.push({material:'air',isVoid:true,startDistance:cursor,endDistance:l.startDistance,pathLength:l.startDistance-cursor});filled.push(l);cursor=l.endDistance;}
    if(cursor<total-1e-9)filled.push({material:'air',isVoid:true,startDistance:cursor,endDistance:total,pathLength:total-cursor});return filled;
  }

  function traverseLayers(part,entryPoint,exitPoint,energy,mass,ammoKind='ball',diameter=.01){
    const intervals=layerIntervals(part,entryPoint,exitPoint),out=[],total=Math.max(0,energy),area=Math.max(1e-9,Math.PI*diameter**2/4);
    let remaining=total;
    for(const interval of intervals){
      const profile=material[interval.material]||material.concrete,mat=R.DestructionMaterials[interval.material]||R.DestructionMaterials[part?.material]||R.DestructionMaterials.concrete;
      const interaction=pair(interval.material,ammoKind),resistancePerArea=interval.isVoid?0:mat.penetrationWork*interaction.penetrationScale*(profile.bulkResistance||1);
      const work=Math.min(remaining,resistancePerArea*area*interval.pathLength),before=remaining;remaining=Math.max(0,remaining-work);
      out.push({...interval,resistance:resistancePerArea,entryEnergy:before,lostEnergy:work,residualEnergy:remaining});
      if(remaining<=0)break;
    }
    return {intervals:out,entryEnergy:total,residualEnergy:remaining,lostEnergy:total-remaining,stopped:remaining<=0};
  }
  function sampleLocal(part,point,channel='default'){
    if(!part?.materialField&&!part?.materialFrame&&!part?.localMaterialFrame)return null;
    const f=part?.materialFrame||part?.localMaterialFrame||{origin:[0,0,0],normal:[0,0,1],tangentU:[1,0,0],tangentV:[0,1,0]},d=V.sub(point,f.origin||[0,0,0]);
    const local=[V.dot(d,f.tangentU),V.dot(d,f.tangentV),V.dot(d,f.normal)],key=[part?.materialField?.seed??0,part?.physicalSolidId||part?.id||'',part?.assemblyId||'',channel,...local.map(x=>Math.floor(x/.05))].join('|');
    let h=2166136261;for(const c of key){h^=c.charCodeAt(0);h=Math.imul(h,16777619)}const value=((h>>>0)/4294967295)*2-1;return {local,key,value,weakness:clamp(value*.08, -.08,.08)};
  }

  R.MaterialImpact=Object.freeze({material,interactions,dominantMaterial,pair,resistance,budget,closestBoundsDistance,grainAxis,layerIntervals,traverseLayers,sampleLocal});
})();
