(function () {
  'use strict';
  const R=window.RTS,M=R.Math,V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z),X=V(1,0,0),Z=V(0,0,1);
  const shade=R.SurfaceColor.shade;
  const unique=values=>values.sort((a,b)=>a-b).filter((v,i,a)=>i===0||v-a[i-1]>1e-7);
  function blendWeights(a,b,t){const out={};for(const k in a)out[k]=(out[k]||0)+a[k]*(1-t);for(const k in b)out[k]=(out[k]||0)+b[k]*t;return out;}
  function patchBox(F,feature,halfX,halfY) {
    const y0=feature.y-halfY,y1=feature.y+halfY,theta=[];
    for(const y of [y0,feature.y,y1]){
      const rx=F.section(y)[1];
      theta.push(Math.asin(M.clamp((feature.x-halfX)/rx,-.94,.94)),Math.asin(M.clamp((feature.x+halfX)/rx,-.94,.94)));
    }
    return {feature,y0,y1,t0:Math.min(...theta),t1:Math.max(...theta),halfX,halfY};
  }
  function loopOf(grid,p) {
    const out=[];
    for(let c=p.c0;c<=p.c1;c++)out.push(grid[p.r0][c]);
    for(let r=p.r0+1;r<=p.r1;r++)out.push(grid[r][p.c1]);
    for(let c=p.c1-1;c>=p.c0;c--)out.push(grid[p.r1][c]);
    for(let r=p.r1-1;r>p.r0;r--)out.push(grid[r][p.c0]);
    return out;
  }

  R.InfantryFace={
    build(B,skin,high,far=false) {
      const F=B.rig.anatomy.faceLayout,f=F.face;
      const patches=[patchBox(F,F.eyes.L,F.eyes.L.radius*1.30,F.eyes.L.radius*1.23),patchBox(F,F.eyes.R,F.eyes.R.radius*1.30,F.eyes.R.radius*1.23),patchBox(F,F.mouth,F.mouth.w*1.48+.001,.0065)];
      let ys=F.levels.map(p=>p[0]);
      for(let y=F.neck.baseY;y<F.topY-.001;y+=high?.0035:far?.010:.0055)ys.push(y);
      let angles=[];const n=high?56:far?24:40;
      for(let j=0;j<n;j++)angles.push(-Math.PI+2*Math.PI*j/n);
      for(const p of patches){ys.push(p.y0,p.y1);for(let i=0;i<=8;i++)angles.push(M.mix(p.t0,p.t1,i/8));for(let i=1;i<6;i++)ys.push(M.mix(p.y0,p.y1,i/6));}
      ys=unique(ys).filter(y=>y<F.topY-.0001);angles=unique(angles);
      const locate=(a,v)=>a.findIndex(x=>Math.abs(v-x)<1e-7);
      for(const p of patches){p.r0=locate(ys,p.y0);p.r1=locate(ys,p.y1);p.c0=locate(angles,p.t0);p.c1=locate(angles,p.t1);}
      const grid=[];
      for(let r=0;r<ys.length;r++){
        const row=[],y=ys[r],section=F.section(y,F._pointSectionScratch);B.currentTag=y<F.headPivotY-.010?'neck':'head';
        for(let c=0;c<angles.length;c++){
          if(patches.some(p=>r>p.r0&&r<p.r1&&c>p.c0&&c<p.c1)){row.push(-1);continue;}
          const t=angles[c],p=F.pointFromSection(y,t,section,F._faceGridPoint),hint=F._faceGridHint.set(Math.sin(t),y>.973?(y-.973)*45:0,Math.cos(t)).normalize(),factor=.96+.04*Math.cos(t),color=F._faceGridColor;
          color[0]=Math.min(1,skin[0]*factor);color[1]=Math.min(1,skin[1]*factor);color[2]=Math.min(1,skin[2]*factor);
          const id=B.vertex(p,F.skinWeightChannels(p),color,hint,[c/angles.length,y*6]);row.push(id);
          if(y>.833&&y<.888){
            const gain=Math.sin(Math.PI*M.clamp((y-.833)/.055,0,1));
            B.morph('neckFlex',id,V(Math.sin(t)*.00065*gain,0,Math.cos(t)*.0011*gain));
          }
        }
        grid.push(row);
      }
      for(let r=0;r<grid.length-1;r++)for(let c=0;c<angles.length;c++){
        if(patches.some(p=>r>=p.r0&&r<p.r1&&c>=p.c0&&c<p.c1))continue;
        const k=(c+1)%angles.length,a=grid[r][c],b=grid[r+1][c],cc=grid[r][k],d=grid[r+1][k];
        if(Math.min(a,b,cc,d)<0)throw new Error('Niespójna granica oczodołu/ust.');
        B.triangle(a,b,cc,1);B.triangle(cc,b,d,1);
      }
      B.currentTag='neck';B.cap(grid[0],{chest:1},skin,V(0,-1,0),1);
      B.currentTag='head';
      const pole=B.vertex(F.point(F.topY,0),{head:1},skin,V(0,1,0));
      const top=grid[grid.length-1];for(let j=0;j<top.length;j++)B.triangle(top[j],pole,top[(j+1)%top.length],1);
      B.faceMetadata={eyes:{},neckConnected:true,mouthOpening:true};
      patches.forEach((p,i)=>{const loop=loopOf(grid,p);if(i<2)buildEyeSocket(B,skin,high,far,F,p,loop);else buildMouth(B,skin,high,far,F,p,loop);});
      buildNose(B,skin,high,far,F);buildEars(B,skin,high,far,F);buildBrows(B,skin,high,far,F);
      R.InfantryHair.build(B,high,f,F,n,far);
    }
  };

  function buildEyeSocket(B,skin,high,far,F,patch,outer) {
    const e=patch.feature,s=e.side,white=R.SurfaceColor.rgb(0xdad8cf),iris=R.SurfaceColor.rgb(F.face.eyeColor),dark=R.SurfaceColor.rgb(0x14191b);
    const head={head:1},w={['eye.'+s]:1};
    B.currentTag='eye.'+s;
    B.ellipsoid(V(e.x,e.y,e.z),V(e.radius,e.radius,e.radius),w,white,1,high?32:far?12:24,high?20:far?8:14);
    function disc(radius,color,offset) {
      const seg=high?28:far?12:20,loops=[];
      for(const fraction of far?[0,.65,1]:[0,.4,.75,1]){
        const loop=[];
        for(let j=0;j<seg;j++){
          const a=j/seg*Math.PI*2,x=e.x+radius*fraction*Math.cos(a),y=e.y+radius*fraction*Math.sin(a),z=F.globeFront(e,x,y)+offset;
          loop.push(B.vertex(V(x,y,z),w,color,V(x-e.x,y-e.y,z-e.z).normalize()));
        }
        if(loops.length)B.bridge(loops[loops.length-1],loop,1);loops.push(loop);
      }
    }
    B.currentTag='iris.'+s;disc(.00285*F.face.eyeWidthScale,iris,.00012);
    B.currentTag='pupil.'+s;disc(.00122*F.face.eyeWidthScale,dark,.00022);
    // The skin actually has an aperture. Its boundary shares indices with the
    // head; no hidden face triangles remain behind the moving eyelids.
    let last=outer;const rim=[];
    for(const t of far?[.5,1]:[.16,.32,.48,.64,.80,1]){
      const loop=[];
      for(let j=0;j<outer.length;j++){
        const edge=B.point(outer[j]);
        const angle=Math.atan2((edge.y-e.y)/patch.halfY,(edge.x-e.x)/patch.halfX);
        const lx=e.w*Math.cos(angle),sin=Math.sin(angle),ly=e.h*sin*Math.pow(Math.abs(sin),M.mix(.12,.45,M.clamp(1.3-e.roundness,0,1)));
        const c=Math.cos(e.tilt),ss=Math.sin(e.tilt);
        const target=V(e.x+lx*c-ly*ss,e.y+lx*ss+ly*c,0);
        // A sub-millimetre, depth-separated seal avoids a float32 pinhole on
        // the meeting line; eye corners themselves stay fixed.
        const seal=-Math.sign(sin)*.00005*Math.sqrt(Math.abs(sin));
        const closed=V(e.x+lx*c-seal*ss,e.y+lx*ss+seal*c,0);
        const openP=edge.clone().lerp(target,t),closedP=openP.clone().addScaledVector(closed.clone().sub(target),t);
        const surfaceZ=(p)=>{
          const d=(p.x-e.x)**2+(p.y-e.y)**2;
          const skinRadius=e.radius+.00135;
          const globe=d<skinRadius*skinRadius?e.z+Math.sqrt(skinRadius*skinRadius-d):-Infinity;
          return Math.max(F.frontZ(p.x,p.y),globe)+.00008;
        };
        openP.z=surfaceZ(openP);closedP.z=surfaceZ(closedP)+(sin>0?.00010*t:0);
        const mid=openP.clone().lerp(closedP,.5);mid.z=surfaceZ(mid);
        B.currentTag=(sin>=0?'lidUpper.':'lidLower.')+s;
        const lidBone=sin>=0?'lidUpper.':'lidLower.';
        const lidShare=(sin>=0?.32:.24)*M.smooth((t-.20)/.80);
        const movingLid={head:1-lidShare,[lidBone+s]:lidShare};
        const wt=blendWeights(F.skinWeights(edge),movingLid,M.smooth(t));
        const id=B.vertex(openP,wt,shade(skin,t>.99?.86:.98),F.normal(openP.x,openP.y));
        B.morph('eyelidsClose',id,closedP.clone().sub(openP));
        B.morph('eyelidsArc',id,mid.sub(openP.clone().lerp(closedP,.5)));
        loop.push(id);if(t===1)rim.push(id);
      }
      B.bridge(last,loop,1);last=loop;
    }
    B.faceMetadata.eyes[s]={centre:[e.x,e.y,e.z],radius:e.radius,rim,outer:outer.slice(),neutralOpen:F.face.neutralEyeOpen};
  }

  function lipWeights(sin,cos,t,base) {
    const side=cos>=0?'L':'R',corner=Math.pow(Math.abs(cos),6)*.75;
    const control=sin>=0?{mouthUpper:1}:{mouthLower:1};
    return blendWeights(base,blendWeights(control,{['mouthCorner.'+side]:.70,jaw:sin<0?.30:0,head:sin>=0?.30:0},corner),t);
  }
  function buildMouth(B,skin,high,far,F,patch,outer) {
    const m=F.mouth,lip=skin.map((v,i)=>Math.min(1,v*(i===0?1.05:i===1?.91:.89))),inside=R.SurfaceColor.rgb(0x40282a);
    let last=outer;const mouthPoints=[];
    for(const t of far?[.55,1]:[.28,.55,.78,1]) {
      const loop=[];
      for(const index of outer){
        const edge=B.point(index),a=Math.atan2((edge.y-m.y)/patch.halfY,(edge.x-m.x)/patch.halfX),cs=Math.cos(a),sn=Math.sin(a);
        const target=V(m.w*cs,m.y+m.h*sn+F.face.neutralMouth*.0010*Math.abs(cs)+F.face.mouthAsymmetry*.35*cs,0);
        const p=edge.clone().lerp(target,t);
        const flesh=sn>=0?F.face.upperLip:F.face.lowerLip;
        p.z=F.frontZ(p.x,p.y)+.0003+Math.sin(t*Math.PI)*flesh*.34;
        const wt=lipWeights(sn,cs,M.smooth(t),F.skinWeights(edge));
        B.currentTag=t>.55?(sn>=0?'upperLip':'lowerLip'):'head';
        const color=t>.55?shade(lip,sn>=0?.94:1.05):skin;
        loop.push(B.vertex(p,wt,color,V(0,0,1)));
        if(t===1)mouthPoints.push({p,wt});
      }
      B.bridge(last,loop,1);last=loop;
    }
    // Closed rear wall and a short inner funnel. No skin cap across the mouth.
    B.currentTag='mouthInner';
    for(const depth of far?[.011]:[.003,.011]) {
      const loop=mouthPoints.map(({p,wt})=>B.vertex(V(p.x*.90,m.y+(p.y-m.y)*1.4,p.z-depth),wt,inside,V(0,0,1)));
      B.bridge(last,loop,1);last=loop;
    }
    B.cap(last,{head:.5,jaw:.5},inside,V(0,0,1),1);
  }

  function buildNose(B,skin,high,far,F) {
    B.currentTag='nose';const f=F.face,w={head:1},y0=f.eyeY+.005,yb=F.noseBaseY;
    const profiles=far?[[y0,.0028*f.noseBridgeScale,.0013,.0005],[M.mix(y0,yb,.5),.0046*f.noseBridgeScale,.004,.003], [yb,.0068*f.noseWidthScale*f.noseTipWidthScale,.006,.0075*f.noseProjectionScale]]:[[y0,.0028*f.noseBridgeScale,.0013,.0005],[M.mix(y0,yb,.26),.0036*f.noseBridgeScale,.0035,.002], [M.mix(y0,yb,.70),.0053*f.noseWidthScale,.0055,.004], [yb,.0068*f.noseWidthScale*f.noseTipWidthScale,.006,.0075*f.noseProjectionScale], [yb-.003+f.noseTipRotation*.004,.0063*f.noseWidthScale,.0035,.007*f.noseProjectionScale]];
    let last=null;
    for(const [y,rx,rz,extrusion] of profiles) {
      const z=F.frontZ(0,y)+extrusion,ring=B.ring(V(0,y,z),X,Z,rx,rz,high?20:14,w,shade(skin,1.01));
      if(last)B.bridge(last,ring,1);else B.cap(ring,w,skin,V(0,1,0),1);last=ring;
    }
    B.cap(last,w,shade(skin,.84),V(0,-1,0),1);
    for(const sign of [-1,1]){
      const x=sign*.0065*f.noseWidthScale*f.nostrilWidthScale,y=yb-.001,z=F.frontZ(x,y)+.0058*f.noseProjectionScale;
      B.ellipsoid(V(x,y,z),V(.0035*f.nostrilWidthScale,.0026,.0035),w,shade(skin,.95),1,high?14:far?6:10,far?4:8);
      if(high)B.ellipsoid(V(x,y-.0018,z+.0016),V(.0016,.00065,.0011),w,shade(skin,.42),1,10,6);
    }
  }
  function buildEars(B,skin,high,far,F) {
    B.currentTag='ears';const f=F.face;
    for(const [s,sign] of [['L',1],['R',-1]]) {
      const y=.927+sign*f.earAsymmetry*.5,side=F.point(y,sign*Math.PI/2),q=new THREE.Quaternion().setFromEuler(new THREE.Euler(0,sign*f.earAngle,0));
      const p=V(side.x+sign*.003,y,side.z-.004);
      B.ellipsoid(p,V(.0065*f.earScale,.0135*f.earScale,.0075*f.earScale),{head:1},shade(skin,.94),1,high?20:far?8:14,high?12:far?5:9,q);
      if(high)B.ellipsoid(p.clone().add(V(sign*.003,0,.004)),V(.0025*f.earScale,.0073*f.earScale,.0025),{head:1},shade(skin,.68),1,12,8,q);
    }
  }
  function buildBrows(B,skin,high,far,F) {
    const f=F.face,color=R.SurfaceColor.rgb(f.hairColor);
    for(const [s,sign] of [['L',1],['R',-1]]) {
      const br=F.brows[s],points=[];
      for(let i=0;i<=10;i++){
        const t=i/10,p=br.inner.clone().lerp(br.outer,t);p.y+=Math.sin(t*Math.PI)*.0015;p.z=F.frontZ(p.x,p.y)+.0013;points.push(p);
      }
      B.currentTag='browInner.'+s;
      const weight=p=>{const t=M.clamp((p.x-br.inner.x)/(br.outer.x-br.inner.x),0,1);return{['browInner.'+s]:1-t,['browOuter.'+s]:t};};
      B.tubePath(points,f.browThickness,weight,color,1,high?8:far?4:6);
    }
  }
})();
