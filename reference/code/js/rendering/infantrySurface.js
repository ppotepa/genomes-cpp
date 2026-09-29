(function () {
  'use strict';
  const R=window.RTS, V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z), Y=V(0,1,0), X=V(1,0,0), Z=V(0,0,1);
  const M=R.Math, shade=R.SurfaceColor.shade;
  // Ciągła bluza z otworami barkowymi. Sąsiednie fragmenty współdzielą indeksy.
  const jacket=[
    [.504,.112,.071],[.519,.113,.071],[.552,.110,.067],[.595,.094,.056],
    [.640,.100,.059],[.698,.113,.066],[.735,.122,.066],[.759,.129,.064],
    [.785,.134,.062],[.809,.127,.057],[.825,.112,.051],[.841,.070,.040],
    [.852,.045,.036],[.859,.042,.034]
  ];
  function baseProfileAt(y){
    for(let i=0;i<jacket.length-1;i++)if(y<=jacket[i+1][0]){const a=jacket[i],b=jacket[i+1],t=M.clamp((y-a[0])/(b[0]-a[0]),0,1);return[M.mix(a[1],b[1],t),M.mix(a[2],b[2],t)];}
    return jacket[jacket.length-1].slice(1);
  }
  function torsoWeights(p,A){
    const y=p.y,H=A.height,sl=A.points.spineLower.y/H,su=A.points.spineUpper.y/H,ch=A.points.chest.y/H,hip=A.hipY;
    if(y<sl){const t=M.smooth((y-hip)/Math.max(.001,sl-hip));return {hips:1-t,spineLower:t};}
    if(y<su){const t=M.smooth((y-sl)/Math.max(.001,su-sl));return {spineLower:1-t,spineUpper:t};}
    if(y<ch){const t=M.smooth((y-su)/Math.max(.001,ch-su));return {spineUpper:1-t,chest:t};}
    if(y>.832)return A.faceLayout.neckWeights(y);
    return {chest:1};
  }
  function profileAt(y,jacketProfile){
    for(let i=0;i<jacketProfile.length-1;i++)if(y<=jacketProfile[i+1][0]){const a=jacketProfile[i],b=jacketProfile[i+1],t=M.clamp((y-a[0])/(b[0]-a[0]),0,1);return[M.mix(a[1],b[1],t),M.mix(a[2],b[2],t)];}
    return jacketProfile[jacketProfile.length-1].slice(1);
  }
  function clothPoint(x,y,depth,ctx){
    const yy=ctx.mapTorsoY(y),base=baseProfileAt(y),d=profileAt(yy,ctx.jacket),xx=x*(d[0]/Math.max(.001,base[0]));
    return V(xx,yy,d[1]*Math.sqrt(Math.max(0,1-xx*xx/(d[0]*d[0])))+depth);
  }
  function weightBlend(a,b,t){return {[a]:1-t,[b]:t};}
  R.InfantrySurface={
    create(rig,side,detail='high',equipment=null,fit=null){const steps=this.createSteps(rig,side,detail,equipment,fit);let step=steps.next();while(!step.done)step=steps.next();return step.value;},
    async createAsync(rig,side,detail='high',equipment=null,fit=null,yieldFrame,check=null,budgetMs=4){
      if(typeof yieldFrame!=='function')throw new TypeError('Async infantry surface generation requires a frame-yield callback.');
      const budget=Number.isFinite(budgetMs)?Math.max(0,budgetMs):4,now=()=>typeof performance!=='undefined'&&performance.now?performance.now():Date.now();
      const steps=this.createSteps(rig,side,detail,equipment,fit);let step=steps.next(),sliceStart=budget>0?now():0,checkpointsSinceClock=0;
      while(!step.done){
        if(check)check();
        // Geometry generation already exposes bounded checkpoints. Sample the
        // time budget every eight checkpoints while checking cancellation at
        // each one and after every frame yield.
        const timeDue=budget>0&&++checkpointsSinceClock>=8;
        if(budget===0||(timeDue&&(checkpointsSinceClock=0,now()-sliceStart>=budget))){await yieldFrame();sliceStart=budget>0?now():0;if(check)check();}
        step=steps.next();
      }
      return step.value;
    },
    *createSteps(rig,side,detail='high',equipment=null,fit=null){
      const B=new R.SurfaceBuilder(rig), A=rig.anatomy, H=A.height, high=detail==='high', far=detail==='far';
      B.materialRegions=true;
      // At far LOD, small facial deformations are subpixel; retain hand curl
      // because weapon grip remains part of the readable silhouette.
      B.omitFacialMorphs=far;
      const activeFit=fit||new R.EquipmentFit(A,equipment),activeAnatomy=A;
      const activeBody={...A.body,armThicknessScale:A.body.armThicknessScale*(activeFit.shirt.ease||1),legThicknessScale:A.body.legThicknessScale*(activeFit.pants.ease||1)};
      const mapTorsoY=y=>activeFit.mapTorsoY(y);
      const mapLegY=y=>.045+(y-.045)*(A.hipY-.045)/(.54-.045);
      const activeJacket=activeFit.jacket,ctx={activeFit,activeAnatomy,activeBody,jacket:activeJacket,mapTorsoY,mapLegY};
      B.equipmentFit=activeFit;
      // Hair samples hidden by headgear are rejected before SurfaceBuilder.vertex,
      // so hair triangles only ever reference visible vertices. Rechecking all
      // three points in triangleFilter repeated that visibility work per face.
      const n=high?32:far?16:24,color=R.SurfaceColor.rgb(side.uniformColor),skin=R.SurfaceColor.rgb(activeBody.skinColor),boot=R.SurfaceColor.rgb(0x302d29);
      const P=name=>A.points[name].clone().multiplyScalar(1/H);
      const ringColor=p=>shade(color,1+.026*Math.sin(p.y*310)+.012*Math.cos(p.x*213));
      const torsoRings=[];
      B.currentTag='jacket';
      for(let row=0;row<activeJacket.length;row++){
        const [y,rx,rz]=activeJacket[row];
        torsoRings.push(B.ring(V(0,y,0),X,Z,rx,rz,n,p=>{
          const w=torsoWeights(p,activeAnatomy);
          if(y>A.shoulderY-.055&&y<A.neckY-.005&&Math.abs(p.x)>A.shoulderHalf*.65){
            const s=p.x>0?'L':'R',t=M.clamp((Math.abs(p.x)-A.shoulderHalf*.65)/Math.max(.02,A.shoulderHalf*.48),0,.55);
            for(const k in w)w[k]*=1-t;w['clavicle.'+s]=t*.46;w['upperArm.'+s]=t*.54;
          }
          return w;
        },ringColor,y*22));
        if((row&3)===3)yield;
      }
      const cspan=high?4:far?2:3, r0=7,r1=10;
      // Porty barków usuwają prostokątny fragment parametrycznej siatki tułowia.
      function inPort(row,col){
        if(row<r0||row>=r1)return false;
        const near=j=>{const d=(j+n)%n;return d<cspan||d>=n-cspan;};
        return near(col)||near(col-n/2);
      }
      for(let row=0;row<torsoRings.length-1;row++){
        for(let j=0;j<n;j++){
          if(inPort(row,j))continue;
          const k=(j+1)%n,a=torsoRings[row],b=torsoRings[row+1];
          B.triangle(a[j],b[j],a[k]);B.triangle(a[k],b[j],b[k]);
          if((j&7)===7)yield;
        }
      }
      B.cap(torsoRings[0],{hips:1},color,V(0,-1,0));
      function getPort(center){
        const list=[],index=(r,c)=>torsoRings[r][(c+n)%n];
        for(let c=center-cspan;c<=center+cspan;c++)list.push(index(r0,c));
        for(let r=r0+1;r<=r1;r++)list.push(index(r,center+cspan));
        for(let c=center+cspan-1;c>=center-cspan;c--)list.push(index(r1,c));
        for(let r=r1-1;r>r0;r--)list.push(index(r,center-cspan));
        return list;
      }
      for(const [s,sign,portCenter] of [['L',1,0],['R',-1,n/2]]){
        B.currentTag='sleeve.'+s;
        const shoulder=P('upperArm.'+s),elbow=P('foreArm.'+s),wrist=P('hand.'+s);
        const dir=elbow.clone().sub(shoulder).normalize(),u=dir.clone().cross(Z).normalize(),v=Z.clone();
        let port=getPort(portCenter);
        // Zachowujemy kolejność krawędzi portu; odwzorowujemy ją na przekrój rękawa.
        const portCenterPoint=V();port.forEach(i=>portCenterPoint.add(B.point(i)));portCenterPoint.multiplyScalar(1/port.length);
        const projections=port.map(i=>{const p=B.point(i).sub(portCenterPoint);return [p.dot(u),p.dot(v)];});
        let area=0;projections.forEach((p,i)=>{const q=projections[(i+1)%projections.length];area+=p[0]*q[1]-p[1]*q[0];});
        const startAngle=Math.atan2(projections[0][1],projections[0][0]);
        const angles=port.map((_,i)=>startAngle+Math.sign(area)*2*Math.PI*i/port.length);let prev=port;
        const total=shoulder.distanceTo(wrist),elbowD=shoulder.distanceTo(elbow),thick=activeBody.armThicknessScale;
        const armProfile=[
          [-.012,.023,.027],[.008,.038,.039],[.028,.044,.044],[.057,.043,.041],[.102,.039,.039],[.141,.035,.036],
          [.164,.035,.034],[.176,.034,.033],[.188,.033,.032],[.201,.033,.031],[.226,.033,.031],[.262,.030,.029],[.296,.027,.021],[.322,.025,.016],[.337,.0255,.013]
        ].map(([d,rx,rz])=>[d<0?d:d/.337*total,rx*thick,rz*thick]);
        for(const [d,rx,rz] of armProfile){
          const t=M.smooth((d-(elbowD-.018))/.052);let weights=weightBlend('upperArm.'+s,'foreArm.'+s,t);
          if(d<.04){
            const armWeight=M.mix(.45,.72,M.smooth((d+.014)/.055)),upper=M.smooth((d+.012)/.044);
            weights={chest:1-armWeight, ['clavicle.'+s]:armWeight*(1-upper), ['upperArm.'+s]:armWeight*upper};
          }
          const ring=B.ring(shoulder.clone().addScaledVector(dir,d),u,v,rx,rz,prev.length,weights,ringColor,d*19,angles);
          B.bridge(prev,ring);prev=ring;
          yield;
        }
        B.cap(prev,{['foreArm.'+s]:1},shade(color,.82),dir);
        // Obręb mankietu jest częścią ubrania, a nie bransoletą/ekwipunkiem.
        const cuffA=B.ring(shoulder.clone().addScaledVector(dir,total-.012),u,v,.0262*activeBody.armThicknessScale,.014*activeBody.armThicknessScale,prev.length,{['foreArm.'+s]:1},shade(color,.72),0,angles);
        const cuffB=B.ring(shoulder.clone().addScaledVector(dir,total),u,v,.0262*activeBody.armThicknessScale,.014*activeBody.armThicknessScale,prev.length,{['foreArm.'+s]:1},shade(color,.80),1,angles);B.bridge(cuffA,cuffB);
        buildHand(B,wrist,dir,s,activeFit.gloves.style?R.SurfaceColor.rgb(0x3d4136):skin,high,far,ctx);
        yield;
      }
      buildPants(B,n,color,P,high,far,ctx);
      yield;
      for(const [s,sign] of [['L',1],['R',-1]]){buildBoot(B,s,sign,boot,high,far,ctx);yield;}
      buildClothDetails(B,color,high,far,ctx);
      yield;
      R.InfantryFace.build(B,skin,high,far);
      yield;
      if(equipment&&equipment.definition('face')?.id==='balaclava'){
        const fabric=R.SurfaceColor.rgb(0x343e36),F=A.faceLayout;
        for(const tag of ['head','neck','ears','nose']){
          for(const i of B.tags[tag]||[]){
            const p=B.point(i),mouth=p.z>0&&Math.abs(p.y-F.mouthY)<.009&&Math.abs(p.x)<F.mouth.w*1.5;
            const eyes=Object.values(F.eyes).some(e=>p.z>0&&Math.abs(p.x-e.x)<e.w+.004&&Math.abs(p.y-e.y)<e.h+.004);
            if(!mouth&&!eyes)for(let k=0;k<3;k++)B.colors[i*3+k]=fabric[k];
          }
          yield;
        }
      }
      const out=yield* B.finishSteps();out.detail=detail;out.clothingFit=activeFit;return out;
    }
  };
  function buildHand(B,wrist,dir,s,skin,high,far,ctx){
    const {activeBody,activeFit}=ctx;
    B.currentTag='hand.'+s;
    const w={['hand.'+s]:1},u=dir.clone().cross(Z).normalize(),v=Z;
    const hs=activeBody.handScale,profile=[[0,.013,.011],[.017,.022,.011],[.042,.023,.009],[.058,.021,.008]].map(([d,x,z])=>[d*hs,x*hs,z*hs]);let prev=null;
    for(const [d,rx,rz] of profile){
      const ring=B.ring(wrist.clone().addScaledVector(dir,d),u,v,rx,rz,high?16:far?6:10,w,skin,d*10);
      if(prev)B.bridge(prev,ring,1);prev=ring;
    }
    B.cap(prev,w,skin,dir,1);
    const digitSkin=activeFit.gloves.style==='fingerless'?R.SurfaceColor.rgb(activeBody.skinColor):skin;
    const lengths=s==='L'?[.029,.039,.042,.033]:[.033,.042,.039,.029];
    for(let f=0;f<4;f++){
      const offset=(f-1.5)*.0101*hs,start=wrist.clone().addScaledVector(dir,.052*hs).addScaledVector(u,offset);
      const len=lengths[f]*hs,r=(f===3?.0045:.0049)*hs;
      const axis=dir.clone().addScaledVector(v,.08).normalize();
      makeDigit(B,start,axis,len,r,w,digitSkin,high,far,2.25,B.rig.anatomy.fingers[s][f]);
    }
    const thumbSign=s==='L'?1:-1;
    const thumbBase=wrist.clone().addScaledVector(dir,.024*hs).addScaledVector(u,thumbSign*.017*hs);
    makeDigit(B,thumbBase,dir.clone().addScaledVector(u,thumbSign*.75).addScaledVector(v,.22).normalize(),.034*hs,.007*hs,w,digitSkin,high,far,.9,B.rig.anatomy.fingers[s][4]);
  }
  function makeDigit(B,start,dir,len,r,weights,skin,high,far,curl,digit){
    const first=B.positions.length/3;
    const u=dir.clone().cross(Z).normalize(),v=u.clone().cross(dir).normalize();let prev=null;
    const ps=[[0,1],[.32,1.04],[.58,.96],[.84,.88],[1,.28]];
    ps.forEach(([t,f])=>{const ring=B.ring(start.clone().addScaledVector(dir,len*t),u,v,r*f,r*.80*f,high?10:far?4:6,digit?R.HandAnatomy.weights(digit,t):weights,skin,t);if(prev)B.bridge(prev,ring,1);prev=ring;});
    B.cap(prev,digit?{[digit.bones[2]]:1}:weights,skin,dir,1);
    // A relaxed grip, shared by bare fingers and gloves. Keep the neutral
    // support hand unchanged; the animator blends this only for free arms.
    for(let i=first;i<B.positions.length/3;i++) {
      const p=B.point(i),offset=p.clone().sub(start);
      const t=M.clamp(offset.dot(dir)/len,0,1),angle=curl*t;
      const center=start.clone().addScaledVector(dir,len*Math.sin(angle)/curl)
        .addScaledVector(v,len*(1-Math.cos(angle))/curl);
      const normal=v.clone().multiplyScalar(Math.cos(angle)).addScaledVector(dir,-Math.sin(angle));
      const target=center.addScaledVector(u,offset.dot(u)).addScaledVector(normal,offset.dot(v));
      B.morph('handsRelax',i,target.sub(p));
    }
  }
  function buildPants(B,n,color,P,high,far,ctx){
    B.currentTag='trousers';let prev=null,last;
    const {mapLegY,activeBody:body,activeAnatomy:A}=ctx,H=A.height,hipX=Math.abs(A.points['thigh.L'].x/H);
    const baseProfiles=[[.565,.099,.061],[.543,.106,.064],[.520,.109,.067],[.491,.108,.066],[.468,.103,.059]];
    const profiles=baseProfiles.map(([y,rx,rz])=>{
      const hipT=M.smooth((y-.468)/(.565-.468));
      return [mapLegY(y),rx*M.mix(body.legThicknessScale,body.hipWidthScale,hipT),rz*M.mix(body.legThicknessScale,(body.hipWidthScale+body.waistDepthScale)*.5,hipT)];
    });
    for(const [y,rx,rz]of profiles){
      const w=p=>{
        const hip=A.hipY,lower=mapLegY(.468),t=M.smooth((hip-p.y)/Math.max(.001,hip-lower))*.65,sl=p.x>=0?'L':'R';
        return {hips:1-t,['thigh.'+sl]:t};
      };
      const loop=B.ring(V(0,y,0),X,Z,rx,rz,n,w,shade(color,.91),y*18);
      if(prev)B.bridge(prev,loop);prev=loop;last=loop;
    }
    const front=n/4,back=3*n/4,seam=[last[front]],count=high?8:far?4:6;
    for(let i=1;i<count;i++){
      const t=i/count,p=V(0,mapLegY(.468-.025*Math.sin(t*Math.PI)),M.mix(.059,-.059,t)*body.legThicknessScale);
      seam.push(B.vertex(p,{hips:.45,'thigh.L':.275,'thigh.R':.275},shade(color,.84),V(0,-1,0),[0,t]));
    }
    seam.push(last[back]);
    for(const [s,sign]of [['L',1],['R',-1]]){
      B.currentTag='leg.'+s;let loop=[];
      if(sign>0){for(let k=back;k<=n+front;k++)loop.push(last[k%n]);loop.push(...seam.slice(1,-1));}
      else {for(let k=front;k<=back;k++)loop.push(last[k]);loop.push(...seam.slice(1,-1).reverse());}
      const center=V(sign*hipX,mapLegY(.455),0),projections=loop.map(i=>{const p=B.point(i).sub(center);return[p.x,p.z];});
      let area=0;projections.forEach((p,i)=>{const q=projections[(i+1)%projections.length];area+=p[0]*q[1]-p[1]*q[0];});
      const startAngle=Math.atan2(projections[0][1],projections[0][0]),angles=loop.map((_,i)=>startAngle+Math.sign(area)*2*Math.PI*i/loop.length);let lastLoop=loop;
      const legProfiles=[
        [.438,.052,.055],[.407,.050,.052],[.370,.045,.047],[.330,.038,.042],[.305,.035,.037],[.293,.034,.035],[.282,.034,.034],[.270,.034,.034],
        [.249,.035,.036],[.219,.037,.037],[.184,.034,.034],[.151,.0305,.032],[.134,.0305,.032]
      ];
      const kneeY=A.points['shin.'+s].y/H;
      for(const [baseY,baseRx,baseRz]of legProfiles){
        const y=mapLegY(baseY),knee=M.smooth((kneeY-y)/Math.max(.018,.058*(A.hipY/.54))),w=weightBlend('thigh.'+s,'shin.'+s,knee);
        const calf=1+.08*(body.muscle-1),taper=M.mix(1,calf,M.smooth((.28-baseY)/.10));
        const rx=baseRx*body.legThicknessScale*taper,rz=baseRz*body.legThicknessScale*taper;
        const rr=B.ring(V(sign*hipX,y,0),X,Z,rx,rz,lastLoop.length,w,p=>shade(color,.90+.025*Math.sin(p.y*370)),y*24,angles);
        B.bridge(lastLoop,rr);lastLoop=rr;
      }
      B.cap(lastLoop,{['shin.'+s]:1},shade(color,.7),V(0,-1,0));
    }
  }
  function buildBoot(B,s,sign,color,high,far,ctx){
    const {mapLegY,activeBody:body,activeAnatomy:A,activeFit}=ctx;
    let prev=null;const n=high?28:far?8:16,H=A.height,footX=Math.abs(A.points['foot.'+s].x/H),fs=body.footScale;
    const levels=[
      [0,.034,.087,.041],[.010,.034,.087,.041],[.016,.033,.086,.041],[.023,.032,.083,.040],[.032,.031,.078,.038],[.045,.030,.065,.024],
      [.064,.029,.039,.006],[.088,.029,.032,0],[.119,.031,.033,0],[.151,.0335,.034,0]
    ];
    levels.forEach(([baseY,rx,rz,zc],level)=>{
      const y=baseY<=.045?baseY:(baseY>.10?mapLegY(.10)+(mapLegY(baseY)-mapLegY(.10))*(activeFit.boots.shaft||1):mapLegY(baseY));B.currentTag=level===0?'sole.'+s:'boot.'+s;
      const w=p=>{if(p.y>mapLegY(.070)){const t=M.smooth((p.y-mapLegY(.07))/Math.max(.01,mapLegY(.135)-mapLegY(.07)));return{['foot.'+s]:1-t,['shin.'+s]:t};}const t=M.smooth((p.z-.062*fs)/(.054*fs));return{['foot.'+s]:1-t,['toes.'+s]:t};};
      const dark=level<3?shade(color,.47):shade(color,1+(level%2)*.11),sx=fs*(.95+.05*body.legThicknessScale)*(activeFit.boots.width||1);
      // Slightly tapered toe and medial bias keep the boot from reading as a
      // perfectly symmetric capsule while preserving the flat sole contact.
      const loop=B.ring(V(sign*footX,y,zc*fs),X,Z,rx*sx,rz*fs*(activeFit.boots.width||1),n,w,dark,y*15,null,(xx,zz,a,out)=>{
        out[0]=Math.sign(xx)*Math.pow(Math.abs(xx),.88)-sign*.055*Math.pow(Math.max(0,zz),2);
        out[1]=Math.sign(zz)*Math.pow(Math.abs(zz),.88);
      });
      if(prev)B.bridge(prev,loop,2);else B.cap(loop,{['foot.'+s]:1},shade(color,.4),V(0,-1,0),2);prev=loop;
    });
    B.currentTag='boot.'+s;B.cap(prev,{['shin.'+s]:1},shade(color,.45),Y,2);
    if(high)for(let i=0;i<5;i++){
      const y=mapLegY(.065+i*.013),z=(.037-i*.0008)*fs;
      B.tubePath([V(sign*footX-.016*fs,y-.004,z),V(sign*footX+.016*fs,y+.004,z+.001)],.0013*fs,{['shin.'+s]:.3,['foot.'+s]:.7},shade(color,.40),2,6);
    }
  }
  function buildClothDetails(B,color,high,far,ctx){
    const {activeAnatomy,jacket:activeJacket}=ctx;
    B.currentTag='tailoring';
    // Naszywane kieszenie deformują się z tymi samymi kośćmi co fragment bluzy.
    function patch(x0,x1,y0,y1,depth,c){
      const rows=high?4:far?1:2,cols=high?4:far?1:2,grid=[];
      for(let r=0;r<=rows;r++){
        const y=M.mix(y0,y1,r/rows),line=[];
        for(let i=0;i<=cols;i++){
          const x=M.mix(x0,x1,i/cols),p=clothPoint(x,y,depth,ctx);
          line.push(B.vertex(p,torsoWeights(p,activeAnatomy),c,V(0,0,1),[i/cols,r/rows]));
        }grid.push(line);
      }
      for(let r=0;r<rows;r++)for(let i=0;i<cols;i++){
        B.triangle(grid[r][i],grid[r+1][i],grid[r][i+1]);B.triangle(grid[r][i+1],grid[r+1][i],grid[r+1][i+1]);
      }
    }
    for(const sign of [1,-1]){
      const x=sign*.054;
      patch(x-.023,x+.023,.694,.750,.0025,shade(color,.88));
      patch(x-.025,x+.025,.738,.755,.004,shade(color,.74));
      if(high){
        const line=[];for(let i=0;i<6;i++)line.push(clothPoint(x-.021,.697+i*.009,.003,ctx));
        B.tubePath(line,.00065,p=>torsoWeights(p,activeAnatomy),shade(color,.57),0,5);
        const hem=[];for(let i=0;i<6;i++)hem.push(clothPoint(x-.022+i*.009,.696,.003,ctx));B.tubePath(hem,.0006,p=>torsoWeights(p,activeAnatomy),shade(color,.62),0,5);
      }
    }
    patch(-.005,.005,.522,.819,.0021,shade(color,.76));
    if(high)for(let i=0;i<6;i++){
      const p=clothPoint(0,.552+i*.048,.0045,ctx);B.ellipsoid(p,V(.0021,.0021,.0012),torsoWeights(p,activeAnatomy),shade(color,.42),0,8,4);
    }
    // Lapels sit on the resolved jacket. Every point uses the same weights as
    // the garment underneath, rather than a rigid, independent chest/neck quad.
    B.currentTag='collar';
    const baseNeck=.030,ratio=activeAnatomy.faceLayout.neck.radiusX/baseNeck;
    for(const sign of [1,-1]){
      const corners=[[sign*.031*ratio,.853],[sign*.049*ratio,.829],[sign*.022*ratio,.806],[sign*.011*ratio,.833]];
      const grid=[];
      for(let row=0;row<=4;row++){
        const line=[],t=row/4;
        for(let col=0;col<=4;col++){
          const u=col/4,x=M.mix(M.mix(corners[0][0],corners[3][0],t),M.mix(corners[1][0],corners[2][0],t),u);
          const y=M.mix(M.mix(corners[0][1],corners[3][1],t),M.mix(corners[1][1],corners[2][1],t),u);
          const p=clothPoint(x,y,.0014,ctx);
          line.push(B.vertex(p,torsoWeights(p,activeAnatomy),shade(color,.80),V(0,0,1),[u,t]));
        }
        grid.push(line);
      }
      for(let row=0;row<4;row++)for(let col=0;col<4;col++){
        B.triangle(grid[row][col],grid[row+1][col],grid[row][col+1]);
        B.triangle(grid[row][col+1],grid[row+1][col],grid[row+1][col+1]);
      }
    }
    // The jacket itself is the standing collar; only a small hem is added.
    const y=.858,d=profileAt(y,activeJacket),N=high?32:far?12:24;
    const c1=B.ring(V(0,y-.001,0),X,Z,d[0]+.0006,d[1]+.0006,N,p=>torsoWeights(p,activeAnatomy),shade(color,.86));
    const c2=B.ring(V(0,y+.0007,0),X,Z,d[0]+.0005,d[1]+.0005,N,p=>torsoWeights(p,activeAnatomy),shade(color,.86));
    B.bridge(c1,c2);
  }
})();
