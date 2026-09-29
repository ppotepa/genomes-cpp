(function () {
  'use strict';
  const R = window.RTS, M = R.Math;
  const V = (x=0,y=0,z=0) => new THREE.Vector3(x,y,z);
  const bell = (x,y=0) => Math.exp(-x*x-y*y);
  function hermiteValue(t,t2,t3,a,b,ma,mb,span){
    return (2*t3-3*t2+1)*a+(t3-2*t2+t)*ma*span+(-2*t3+3*t2)*b+(t3-t2)*mb*span;
  }

  // Normalized anatomical space (multiply by height only at the buffer boundary).
  // This is the single source of attachment positions for skin, rig and hair.
  R.FaceAnatomy = class FaceAnatomy {
    constructor(face, body) {
      this.face = face; this.body = body; this.adjustments = [];this._frontPoint=V();
      this._pointSectionScratch=[0,0,0,0];this._frontSectionScratch=[0,0,0,0];
      this._normalSectionsScratch=[[0,0,0,0],[0,0,0,0],[0,0,0,0]];
      this._sectionCache=new Map();this._sectionCacheLimit=256;this._sectionCacheNext=0;this._sectionCacheValues=new Float64Array(256*5);this._sectionCacheHits=0;this._sectionCacheMisses=0;
      this._faceGridPoint=V();this._faceGridHint=V();this._faceGridColor=[0,0,0];
      this._skinWeightChannels=R.SurfaceBuilder.createWeightChannels(['chest','neck','head','jaw','cheek.L','browInner.L','browOuter.L','cheek.R','browInner.R','browOuter.R']);
      const f = face, b = body;
      const record = (key, requested, resolved) => {
        if (Math.abs(requested-resolved)>1e-8) this.adjustments.push({key,requested,resolved});
        return resolved;
      };
      const neckX = .0285*b.neckScale, neckZ = .024*b.neckScale;
      const neckJointY = M.mix(.837,.843,M.clamp((b.neckScale-.76)/.56,0,1));
      this.neck = {radiusX:neckX,radiusZ:neckZ,baseY:.828,topY:.883,jointY:neckJointY};
      const base = [
        [.881,.034,.030,.013], [.890,.039,.037,.010], [.900,.041,.041,.008],
        [.913,.045,.044,.005], [.925,.049,.046,.003], [.938,.048,.046,.002],
        [.948,.047,.046,.001], [.959,.0465,.046,0], [.973,.043,.043,-.001],
        [.986,.032,.034,-.003], [.995,.015,.018,-.004], [.999,0,0,-.004]
      ];
      const shaped = base.map(([y0,rx,rz,zc]) => {
        const low=M.smooth(M.clamp((.928-y0)/.058,0,1)), cheek=bell((y0-(.928+f.cheekboneY))/.020);
        const temple=bell((y0-.951)/.020), forehead=M.smooth((y0-.948)/.038), chin=bell((y0-.887)/.015);
        let y=y0;
        if(y0<.925) y=.925+(y0-.925)*f.jawLengthScale+f.chinHeight*chin*.65;
        if(y0>.947) y=.999-(.999-y0)*f.headLengthScale;
        let scale=M.mix(1,f.jawWidthScale,low)*M.mix(1,f.jawAngle,low*low*.40);
        scale=M.mix(scale,f.templeWidthScale,temple*.55);
        scale=M.mix(scale,f.foreheadWidthScale,forehead*.65);
        rx*=scale*f.headWidthScale*M.mix(1,b.headScale,.4);
        rz*=f.headDepthScale*M.mix(1,b.headScale,.4);
        return [y,rx,rz,zc];
      });
      shaped[0][0]=M.clamp(shaped[0][0],.8755,.8855);
      // Let the throat taper into the jaw. Spreading the entire chin ring to
      // match the widest neck created a visible shelf on otherwise narrow faces.
      for(let i=1;i<shaped.length;i++) shaped[i][0]=Math.max(shaped[i][0],shaped[i-1][0]+.003);
      this.chinY=shaped[0][0];
      this.headPivotY=M.clamp(this.chinY+.018,.892,.904);
      // Blend the jaw into the neck across the full upper-throat span. A short
      // transition made the underside of the jaw read as a horizontal shelf.
      const neckJoin=[.2,.4,.6,.8].map(t=>{
        const y=M.mix(.853,this.chinY,t),blend=M.smooth(t);
        return [y,M.mix(neckX*.99,shaped[0][1],blend),M.mix(neckZ*1.01,shaped[0][2],blend),M.mix(.001,shaped[0][3],blend)];
      });
      this.levels=[
        [.828,neckX*1.14,neckZ*1.12,-.002],
        [.841,neckX*1.06,neckZ*1.04,-.002],
        [.853,neckX,neckZ,-.002],
        ...neckJoin,
        ...shaped
      ];
      this._sectionCursor=0;
      this.levelSlopes=this.levels.map((level,i)=>this.levels.map((_,k)=>{
        if(i===0)return (this.levels[1][k]-level[k])/(this.levels[1][0]-level[0]);
        if(i===this.levels.length-1)return (level[k]-this.levels[i-1][k])/(level[0]-this.levels[i-1][0]);
        const previous=this.levels[i-1],next=this.levels[i+1];
        const dl=(level[k]-previous[k])/(level[0]-previous[0]);
        const dr=(next[k]-level[k])/(next[0]-level[0]);
        return dl*dr<=0?0:2*dl*dr/(dl+dr);
      }));
      this.topY=this.levels[this.levels.length-1][0];
      this.jaw=V(0,.915,.001);
      this.mouthY=record('mouthY',f.mouthY,M.clamp(f.mouthY,this.chinY+.014,f.eyeY-.031));
      this.mouthHalf=M.clamp(f.mouthWidth*.5,.007,this.section(this.mouthY)[1]*.57);
      this.mouth={x:0,y:this.mouthY,z:this.frontZ(0,this.mouthY),w:this.mouthHalf,h:.00014};
      this.eyes={}; this.brows={};
      const eyeW=.0083*f.eyeWidthScale, eyeH=.0034*f.eyeHeightScale;
      const radius=Math.max(eyeW*1.12,eyeH*1.65)*.82;
      const rx=this.section(f.eyeY)[1];
      const spacing=record('eyeSpacing',f.eyeSpacing,M.clamp(f.eyeSpacing,radius*1.20+.003,Math.max(radius*1.20+.003,rx*.66)));
      for(const [s,sign] of [['L',1],['R',-1]]) {
        const x=sign*spacing,y=f.eyeY+sign*f.eyeAsymmetry*.5;
        const z=this.frontZ(x,y)-radius*.80+f.eyeDepth*.16;
        this.eyes[s]={side:s,sign,x,y,z,w:eyeW,h:eyeH,radius,tilt:sign*f.eyeTilt,roundness:f.eyeRoundness};
        const by=record('browY.'+s,f.browY+sign*f.browAsymmetry*.5,Math.max(f.browY+sign*f.browAsymmetry*.5,y+eyeH+.006));
        const innerX=sign*Math.max(.006,spacing-.007+f.browSpacing*.5);
        const outerX=sign*(spacing+.008+f.browSpacing*.5);
        this.brows[s]={inner:V(innerX,by,this.frontZ(innerX,by)+.0012),outer:V(outerX,by+f.browTilt*.006,this.frontZ(outerX,by+f.browTilt*.006)+.0012)};
      }
      this.noseBaseY=record('noseBaseY',f.eyeY-.031*f.noseLengthScale,Math.max(f.eyeY-.031*f.noseLengthScale,this.mouthY+.010));
      this.hairFloor=Math.max(this.brows.L.inner.y,this.brows.R.inner.y,this.brows.L.outer.y,this.brows.R.outer.y)+.006;
      record('hairlineFront',f.hairline-f.widowPeak,Math.max(f.hairline-f.widowPeak,this.hairFloor));
    }

    section(y,out=null) {
      const cached=this._sectionCache.get(y);
      if(cached!==undefined){this._sectionCache.delete(y);this._sectionCache.set(y,cached);this._sectionCacheHits++;
        const values=this._sectionCacheValues,offset=cached*5;
        if(out){out[0]=values[offset+1];out[1]=values[offset+2];out[2]=values[offset+3];out[3]=values[offset+4];return out;}
        return [values[offset+1],values[offset+2],values[offset+3],values[offset+4]];
      }
      this._sectionCacheMisses++;
      const a=this.levels;
      // The profile is sorted by height. Locate the first interval whose upper
      // endpoint contains y instead of scanning every anatomical level.
      let i=this._sectionCursor;
      // Face rings and normal probes repeatedly sample adjacent heights.
      // Reuse their last interval when it still contains y; preserve the
      // binary-search rule that an exact interior boundary belongs below.
      const inside=i>=0&&i<a.length-1&&y<=a[i+1][0]&&(y>a[i][0]||(i===0&&y===a[0][0]));
      if(!inside){let lo=0,hi=a.length-1;while(lo<hi){const mid=(lo+hi)>>1;if(a[mid+1][0]<y)lo=mid+1;else hi=mid;}i=lo;this._sectionCursor=i;}
      if(i<a.length-1) {
        const t=M.clamp((y-a[i][0])/(a[i+1][0]-a[i][0]),0,1);
        const t2=t*t,t3=t2*t,span=a[i+1][0]-a[i][0],left=this.levelSlopes[i],right=this.levelSlopes[i+1];
        const rx=Math.max(0,hermiteValue(t,t2,t3,a[i][1],a[i+1][1],left[1],right[1],span));
        const rz=Math.max(0,hermiteValue(t,t2,t3,a[i][2],a[i+1][2],left[2],right[2],span));
        const depth=hermiteValue(t,t2,t3,a[i][3],a[i+1][3],left[3],right[3],span);
        const result=out||[y,rx,rz,depth];
        if(out){out[0]=y;out[1]=rx;out[2]=rz;out[3]=depth;}
        this._rememberSection(y,result);
        return result;
      }
      const result=out||[y,0,0,a[a.length-1][3]];
      if(out){out[0]=y;out[1]=0;out[2]=0;out[3]=a[a.length-1][3];}
      this._rememberSection(y,result);
      return result;
    }
    _rememberSection(y,value){
      if(!this._sectionCacheLimit)return;
      if(this._sectionCache.size>=this._sectionCacheLimit)this._sectionCache.delete(this._sectionCache.keys().next().value);
      const slot=this._sectionCacheNext,offset=slot*5,values=this._sectionCacheValues;
      values[offset]=y;values[offset+1]=value[0];values[offset+2]=value[1];values[offset+3]=value[2];values[offset+4]=value[3];
      this._sectionCache.set(y,slot);this._sectionCacheNext=(slot+1)%this._sectionCacheLimit;
    }
    point(y, theta, out=V(), sectionOut=null) {
      return this.pointFromSection(y,theta,this.section(y,sectionOut||this._pointSectionScratch),out);
    }
    pointFromSection(y,theta,p,out=V()) {
      const f=this.face, sx=Math.sin(theta), cz=Math.cos(theta);
      const low=M.smooth(M.clamp((.925-y)/.055,0,1));
      const chin=bell((y-(this.chinY+.014))/.020);
      const side=M.smooth((Math.abs(sx)-.10)/.72);
      const x=p[1]*sx*(1+(f.chinWidthScale-1)*.30*chin*side); let z=p[3]+p[2]*cz;
      if(cz>0 && y>.882) {
        const front=M.smooth((cz-.02)/.48);
        const cheek=bell((Math.abs(x)-.030*f.cheekboneScale)/.014,(y-(.925+f.cheekboneY))/.016);
        z+=(.0014+f.cheekFullness*.42+(f.cheekboneScale-1)*.0018)*cheek*front;
        z+=f.chinProjection*.30*bell(x/.023,(y-(this.chinY+.014))/.020)*front;
        z+=f.browRidge*bell((Math.abs(x)-f.eyeSpacing)/.017,(y-(f.eyeY+.010))/.009)*cz;
        z+=f.midfaceProjection*.38*bell(x/.034,(y-.918)/.024)*cz;
        z+=f.foreheadSlope*M.smooth((y-.947)/.045)*cz;
      }
      return out.set(x,y,z);
    }
    frontZ(x,y) {
      return this.frontZFromSection(x,y,this.section(y,this._frontSectionScratch));
    }
    frontZFromSection(x,y,section) {
      const rx=Math.max(.00001,section[1]);
      // frontZ is queried heavily while generating face normals. The old path
      // converted x/rx to an angle, then pointFromSection converted it back
      // with sin() and allocated/wrote a scratch vector. Evaluate its z-only
      // projection directly while preserving the same clamped profile.
      const sx=M.clamp(x/rx,-.9999,.9999),cz=Math.sqrt(Math.max(0,1-sx*sx));
      const f=this.face,chin=bell((y-(this.chinY+.014))/.020),side=M.smooth((Math.abs(sx)-.10)/.72);
      const px=rx*sx*(1+(f.chinWidthScale-1)*.30*chin*side);
      let z=section[3]+section[2]*cz;
      if(cz>0&&y>.882){
        const front=M.smooth((cz-.02)/.48);
        const cheek=bell((Math.abs(px)-.030*f.cheekboneScale)/.014,(y-(.925+f.cheekboneY))/.016);
        z+=(.0014+f.cheekFullness*.42+(f.cheekboneScale-1)*.0018)*cheek*front;
        z+=f.chinProjection*.30*bell(px/.023,(y-(this.chinY+.014))/.020)*front;
        z+=f.browRidge*bell((Math.abs(px)-f.eyeSpacing)/.017,(y-(f.eyeY+.010))/.009)*cz;
        z+=f.midfaceProjection*.38*bell(px/.034,(y-.918)/.024)*cz;
        z+=f.foreheadSlope*M.smooth((y-.947)/.045)*cz;
      }
      return z;
    }
    normal(x,y,out=V()) {
      const e=.0001,scratch=this._normalSectionsScratch,center=this.section(y,scratch[0]),upper=this.section(y+e,scratch[1]),lower=this.section(y-e,scratch[2]);
      const dx=(this.frontZFromSection(x+e,y,center)-this.frontZFromSection(x-e,y,center))/(2*e);
      const dy=(this.frontZFromSection(x,y+e,upper)-this.frontZFromSection(x,y-e,lower))/(2*e);
      return out.set(-dx,-dy,1).normalize();
    }
    neckWeights(y) {
      const h=M.smooth((y-(this.headPivotY-.022))/.030);
      const c=(1-M.smooth((y-(this.neck.jointY-.010))/.030))*(1-h);
      return {chest:c,neck:1-h-c,head:h};
    }
    skinWeights(p) {
      const f=this.face, front=M.smooth((p.z-.003)/.025);
      const jaw=(1-M.smooth((p.y-(this.mouthY-.002))/.018))*front*.92;
      const weights={head:1-jaw,jaw};
      if(p.y>this.headPivotY-.040) {
        for(const [s,sign] of [['L',1],['R',-1]]) {
          const cx=sign*(this.eyes?this.eyes[s].x*sign+.006:f.eyeSpacing+.006);
          const cheek=.30*front*bell((p.x-cx)/.016,(p.y-(f.eyeY-.016))/.012);
          if(cheek>.0005){Object.keys(weights).forEach(k=>weights[k]*=1-cheek);weights['cheek.'+s]=cheek;}
          const br=this.brows[s],t=M.clamp((Math.abs(p.x)-Math.min(Math.abs(br.inner.x),Math.abs(br.outer.x)))/Math.max(.001,Math.abs(br.outer.x-br.inner.x)),0,1);
          const browY=M.mix(br.inner.y,br.outer.y,t),browX=sign*M.mix(Math.abs(br.inner.x),Math.abs(br.outer.x),t);
          const brow=.28*front*bell((p.x-browX)/.016,(p.y-browY)/.010);
          if(brow>.0005){Object.keys(weights).forEach(k=>weights[k]*=1-brow);weights['browInner.'+s]=brow*(1-t);weights['browOuter.'+s]=brow*t;}
        }
      }
      const transition=M.smooth((p.y-(this.headPivotY-.038))/.032),neck=this.neckWeights(p.y),resolved={};
      for(const [name,value] of Object.entries(neck))resolved[name]=(resolved[name]||0)+value*(1-transition);
      for(const [name,value] of Object.entries(weights))resolved[name]=(resolved[name]||0)+value*transition;
      return resolved;
    }
    skinWeightChannels(p){
      const out=this._skinWeightChannels,v=out.values;v.fill(0);
      const f=this.face,front=M.smooth((p.z-.003)/.025),jaw=(1-M.smooth((p.y-(this.mouthY-.002))/.018))*front*.92;
      v[2]=1-jaw;v[3]=jaw;
      if(p.y>this.headPivotY-.040){
        for(let side=0;side<2;side++){
          const s=side===0?'L':'R',sign=side===0?1:-1,cheekIndex=side===0?4:7,innerIndex=side===0?5:8,outerIndex=side===0?6:9;
          const cx=sign*(this.eyes?this.eyes[s].x*sign+.006:f.eyeSpacing+.006),cheek=.30*front*bell((p.x-cx)/.016,(p.y-(f.eyeY-.016))/.012);
          if(cheek>.0005){for(let i=2;i<10;i++)v[i]*=1-cheek;v[cheekIndex]=cheek;}
          const br=this.brows[s],t=M.clamp((Math.abs(p.x)-Math.min(Math.abs(br.inner.x),Math.abs(br.outer.x)))/Math.max(.001,Math.abs(br.outer.x-br.inner.x)),0,1),browY=M.mix(br.inner.y,br.outer.y,t),browX=sign*M.mix(Math.abs(br.inner.x),Math.abs(br.outer.x),t),brow=.28*front*bell((p.x-browX)/.016,(p.y-browY)/.010);
          if(brow>.0005){for(let i=2;i<10;i++)v[i]*=1-brow;v[innerIndex]=brow*(1-t);v[outerIndex]=brow*t;}
        }
      }
      const transition=M.smooth((p.y-(this.headPivotY-.038))/.032),remaining=1-transition,h=M.smooth((p.y-(this.headPivotY-.022))/.030);
      // Match neckWeights exactly while avoiding its temporary return object.
      const neckChest=(1-M.smooth((p.y-(this.neck.jointY-.010))/.030))*(1-h),neck=h;
      v[0]=neckChest*remaining;v[1]=(1-neck-neckChest)*remaining;
      v[2]=h*remaining+v[2]*transition;
      for(let i=3;i<10;i++)v[i]*=transition;
      let count=0;for(let i=0;i<10;i++)if(v[i]>0){out.indices[count]=i;out.values[count]=v[i];count++;}
      out.count=count;return out;
    }
    globeFront(eye,x,y) {
      const d=(x-eye.x)**2+(y-eye.y)**2;
      return eye.z+Math.sqrt(Math.max(0,eye.radius*eye.radius-d));
    }
    hairBottom(theta) {
      const f=this.face,front=Math.max(0,Math.cos(theta)),side=Math.abs(Math.sin(theta)),back=Math.max(0,-Math.cos(theta));
      let y=f.hairline+f.templeRecession*front*side**1.8-f.widowPeak*front*(1-side)**1.5;
      y-=back*.011;
      const safety=M.mix(.931,this.hairFloor,M.smooth((front-.25)/.50));
      return M.clamp(Math.max(y,safety),.933,this.topY-.012);
    }
  };
})();
