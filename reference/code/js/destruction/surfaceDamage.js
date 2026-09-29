(function(){
  'use strict';
  const R=globalThis.RTS,V=R.DestructionSolid.V;
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  function hash(seed,text){let h=(2166136261^(seed>>>0))>>>0;for(const c of String(text))h=Math.imul(h^c.charCodeAt(0),16777619)>>>0;h^=h>>>16;h=Math.imul(h,0x7feb352d);h^=h>>>15;return (h>>>0)/4294967296;}
  function basis(axis,preferred=null){const a=V.unit(axis);let u=null;if(Array.isArray(preferred)){const projected=V.sub(preferred,V.mul(a,V.dot(preferred,a)));if(V.length(projected)>1e-7)u=V.unit(projected);}if(!u)u=V.unit(V.cross(a,Math.abs(a[1])<.8?[0,1,0]:[1,0,0]));const v=V.cross(a,u);return {axis:a,u,v};}
  function shape(material){
    if(material==='wood')return {sides:8,stretchU:1.75,stretchV:.72,irregular:.08,profile:'fibrous-tear'};
    if(material==='steel'||material==='armor')return {sides:12,stretchU:1,stretchV:1,irregular:.025,profile:'ductile-dent'};
    if(material==='glass')return {sides:10,stretchU:1.08,stretchV:.92,irregular:.16,profile:'glass-break'};
    if(material==='brick')return {sides:9,stretchU:1.18,stretchV:.88,irregular:.18,profile:'masonry-crater'};
    if(material==='concrete'||material==='rock')return {sides:10,stretchU:1.12,stretchV:.9,irregular:.13,profile:'mineral-crater'};
    return {sides:8,stretchU:1,stretchV:1,irregular:.08,profile:'generic'};
  }
  function radialPlanes(point,axis,radius,options={}){
    const {u,v}=basis(axis,options.preferred),sides=options.sides||8,stretchU=options.stretchU||1,stretchV=options.stretchV||1,irregular=options.irregular||0,seed=options.seed||1,key=options.key||'damage',planes=[];
    for(let i=0;i<sides;i++){const a=Math.PI*2*i/sides,c=Math.cos(a),s=Math.sin(a),radial=V.unit(V.add(V.mul(u,c/stretchU),V.mul(v,s/stretchV))),ellipse=radius/Math.sqrt((c/stretchU)**2+(s/stretchV)**2),j=1+(hash(seed,key+'/'+i)*2-1)*irregular,r=ellipse*j;planes.push({n:radial,d:V.dot(radial,point)+r});}
    return planes;
  }
  function bore(point,axis,radius,depth,material='concrete',seed=1,key='bore'){
    const spec=shape(material),planes=radialPlanes(point,axis,radius,{...spec,sides:Math.min(6,spec.sides),irregular:spec.irregular*.35,seed,key});
    if(Number.isFinite(depth)){const d=V.unit(axis);planes.push({n:d,d:V.dot(d,point)+depth},{n:V.mul(d,-1),d:-V.dot(d,point)+radius*.65});}
    return planes;
  }
  function crater(point,axis,radius,depth,material='concrete',seed=1,key='crater',maxSides=Infinity,options={}){
    const spec=shape(material),{u,v}=basis(axis,options.preferred),sides=Math.min(spec.sides,maxSides),planes=[],slope=radius/Math.max(.000001,depth),stretchU=options.stretchU||spec.stretchU,stretchV=options.stretchV||spec.stretchV;
    for(let i=0;i<sides;i++){
      const a=Math.PI*2*i/sides,c=Math.cos(a),s=Math.sin(a),radial=V.unit(V.add(V.mul(u,c/stretchU),V.mul(v,s/stretchV))),ellipse=radius/Math.sqrt((c/stretchU)**2+(s/stretchV)**2),j=1+(hash(seed,key+'/'+i)*2-1)*(options.irregular??spec.irregular),r=ellipse*j,scale=Math.sqrt(1+slope*slope),n=V.mul(V.add(radial,V.mul(V.unit(axis),slope)),1/scale);
      planes.push({n,d:V.dot(n,point)+r/scale});
    }
    return {planes,profile:spec.profile,sides};
  }
  function perforation(point,axis,radius,depth,material='concrete',seed=1,key='perforation',options={}){
    const spec=shape(material),{u,v,axis:d}=basis(axis,options.preferred),sides=Math.min(options.sides||4,spec.sides),planes=[],safeDepth=Math.max(.000001,depth),defaultExit=material==='glass'?1.8:['brick','concrete','rock'].includes(material)?1.35:material==='wood'?1.15:1.05,exitScale=options.exitScale??defaultExit,stretchU=options.stretchU||spec.stretchU,stretchV=options.stretchV||spec.stretchV,irregular=options.irregular??spec.irregular*.28;
    for(let i=0;i<sides;i++){
      const a=Math.PI*2*i/sides,c=Math.cos(a),s=Math.sin(a),raw=V.add(V.mul(u,c/stretchU),V.mul(v,s/stretchV)),radial=V.unit(raw),ellipse=radius/Math.sqrt((c/stretchU)**2+(s/stretchV)**2),j=1+(hash(seed,key+'/'+i)*2-1)*irregular,r0=ellipse*j,r1=r0*exitScale,k=(r1-r0)/safeDepth,nRaw=V.sub(radial,V.mul(d,k)),len=V.length(nRaw)||1,n=V.mul(nRaw,1/len);planes.push({n,d:(V.dot(nRaw,point)+r0)/len});
    }
    return {planes,profile:options.profile||spec.profile,sides,entryRadius:radius,exitRadius:radius*exitScale,exitScale,mode:options.mode||null};
  }
  function frustum(point,axis,startRadius,endRadius,depth,material='concrete',seed=1,key='frustum',options={}){
    const spec=shape(material),{u,v,axis:d}=basis(axis,options.preferred),sides=Math.min(options.sides||4,spec.sides),planes=[],safeDepth=Math.max(.000001,depth),stretchU=options.stretchU||spec.stretchU,stretchV=options.stretchV||spec.stretchV,irregular=options.irregular??spec.irregular*.22;
    for(let i=0;i<sides;i++){const a=Math.PI*2*i/sides,c=Math.cos(a),s=Math.sin(a),radial=V.unit(V.add(V.mul(u,c/stretchU),V.mul(v,s/stretchV))),ellipse=startRadius/Math.sqrt((c/stretchU)**2+(s/stretchV)**2),j=1+(hash(seed,key+'/'+i)*2-1)*irregular,r0=ellipse*j,r1=endRadius*(r0/Math.max(1e-9,startRadius)),k=(r1-r0)/safeDepth,nRaw=V.sub(radial,V.mul(d,k)),len=V.length(nRaw)||1,n=V.mul(nRaw,1/len);planes.push({n,d:(V.dot(nRaw,point)+r0)/len});}
    const pad=Math.min(startRadius,endRadius)*.12;planes.push({n:V.mul(d,-1),d:-V.dot(d,point)+pad},{n:d,d:V.dot(d,point)+safeDepth});
    return {planes,sides,startRadius,endRadius,depth:safeDepth};
  }
  function damageTunnel(point,axis,frontRadius,channelRadius,exitRadius,depth,material='concrete',seed=1,key='tunnel',options={}){
    const d=V.unit(axis),safeDepth=Math.max(.000001,depth),entryDepth=Math.min(safeDepth*.45,Math.max(safeDepth*.16,Math.min(frontRadius*1.4,safeDepth*.32))),rearDepth=Math.min(safeDepth-entryDepth,Math.max(safeDepth*.18,Math.min(exitRadius*1.1,safeDepth*.34))),middleDepth=Math.max(0,safeDepth-entryDepth-rearDepth),cutters=[];
    cutters.push(frustum(point,d,frontRadius,channelRadius,entryDepth,material,seed,key+'/entry',options));
    if(middleDepth>1e-5)cutters.push(frustum(V.add(point,V.mul(d,entryDepth)),d,channelRadius,channelRadius,middleDepth,material,seed,key+'/channel',{...options,irregular:(options.irregular??shape(material).irregular*.22)*.45}));
    cutters.push(frustum(V.add(point,V.mul(d,entryDepth+middleDepth)),d,channelRadius,exitRadius,rearDepth,material,seed,key+'/exit',options));
    return {cutters,entryDepth,middleDepth,rearDepth,frontRadius,channelRadius,exitRadius,profile:options.profile||shape(material).profile,mode:options.mode||null};
  }
  function exitFlare(point,axis,radius,depth,material='concrete',seed=1,key='exit',exitPoint=null){
    if(!['brick','concrete','rock','glass'].includes(material))return null;
    const d=V.unit(axis),center=exitPoint?V.add(exitPoint,V.mul(d,-radius*.8)):V.add(point,V.mul(d,Math.max(0,depth-radius*.8))),scale=material==='glass'?1.8:1.35;
    return crater(center,d,radius*scale,Math.max(radius*.45,Math.min(depth*.35,radius*1.8)),material,seed,key,6);
  }
  R.DestructionSurfaceDamage={shape,bore,perforation,frustum,damageTunnel,crater,exitFlare,radialPlanes};
})();