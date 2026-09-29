(function () {
  'use strict';
  const R=window.RTS;
  const assert=(v,m)=>{if(!v)throw new Error(m);};
  function skinDepth(model,x,y,morphs=null,includeLids=false) {
    const g=model.mesh.geometry,p=g.attributes.position.array,ids=g.index.array,tags=model.surface.tags;
    const tagsSet=new Set([...(tags.head||[]),...(tags.neck||[]),...(includeLids?[...(tags['lidUpper.L']||[]),...(tags['lidUpper.R']||[]),...(tags['lidLower.L']||[]),...(tags['lidLower.R']||[])]:[])]);
    const point=i=>{const q=[p[3*i],p[3*i+1],p[3*i+2]];if(morphs)for(let m=0;m<morphs.length;m++)for(let k=0;k<3;k++)q[k]+=g.morphAttributes.position[m].array[3*i+k]*morphs[m];return q;};
    let depth=-Infinity;
    for(let t=0;t<ids.length;t+=3){
      const ia=ids[t],ib=ids[t+1],ic=ids[t+2];if(!tagsSet.has(ia)||!tagsSet.has(ib)||!tagsSet.has(ic))continue;
      const a=point(ia),b=point(ib),c=point(ic),d=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);if(Math.abs(d)<1e-12)continue;
      const u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/d,v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/d,w=1-u-v;
      if(Math.min(u,v,w)>=-1e-7)depth=Math.max(depth,u*a[2]+v*b[2]+w*c[2]);
    }
    return depth;
  }
  function verify(model) {
    const result=R.UnitDiagnostics.validate(model),g=model.mesh.geometry;
    assert(result.ok,'Niepoprawne bufory lub anatomia');
    const neck=new Set(model.surface.tags.neck),head=new Set(model.surface.tags.head);let cross=0;
    const ids=g.index.array;
    for(let i=0;i<ids.length;i+=3)for(let k=0;k<3;k++){const a=ids[i+k],b=ids[i+(k+1)%3];if(neck.has(a)&&head.has(b)||head.has(a)&&neck.has(b))cross++;}
    assert(cross>0,'Brak wspólnej topologii głowy i szyi');
    const H=model.anatomy.height,F=model.anatomy.faceLayout,eyes=[];
    for(const [s,e] of Object.entries(F.eyes)) {
      assert(model.anatomy.parents['lidUpper.'+s]==='head','Powieka pod gałką');
      const globeFront=(e.z+e.radius)*H,x=e.x*H,y=e.y*H;
      const open=skinDepth(model,x,y),closed=skinDepth(model,x,y,[1,0,0],true);
      assert(open<globeFront-.0001,'Skóra zasłania środek oka '+s);
      assert(closed>globeFront,'Zamknięta powieka nie zasłania oka '+s);
      eyes.push({side:s,open:true,closed:true});
    }
    const nd=g.morphAttributes.position.length;
    const names=new Set(g.morphAttributes.position.map(a=>a.name));
    for(const name of ['eyelidsClose','eyelidsArc','neckFlex','handsRelax'])assert(names.has(name),'Brak kanału korekty '+name);
    assert(nd===names.size&&g.morphAttributes.normal.length===nd,'Niepasujące kanały korekt/normalnych');
    return {vertices:result.vertices,triangles:result.triangles,neckConnectionEdges:cross,eyes,morphTargets:nd};
  }
  R.CoreChecks={assert,verify,skinDepth};
})();
