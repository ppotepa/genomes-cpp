(function () {
  'use strict';
  const R=window.RTS,M=R.Math,V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z),shade=R.SurfaceColor.shade;
  const X=V(1,0,0),Y=V(0,1,0),Z=V(0,0,1);
  let sharedMaterials=null;
  const namedWeights=new Map();
  const palettes={sand:0x8e8061,forest:0x505d3d,graphite:0x434a4a};
  const PART_CACHE=new Map();let partCacheBytes=0,partCacheLimit=24*1024*1024,partCacheHits=0,partCacheMisses=0;
  function palette(side,eq){
    const main=R.SurfaceColor.rgb(palettes[eq.palette]||side.uniformColor);
    return {cloth:main,web:shade(main,.58),edge:shade(main,.73),rubber:R.SurfaceColor.rgb(0x292b28),metal:R.SurfaceColor.rgb(0x545951),light:R.SurfaceColor.rgb(0xb9b097)};
  }
  function weights(name){let result=namedWeights.get(name);if(!result){result=R.SurfaceBuilder.staticWeights({[name]:1});namedWeights.set(name,result);}return result;}
  function aroundNormal(n){return new THREE.Quaternion().setFromUnitVectors(Z,n.clone().normalize());}
  const C=class Context {
    constructor(B,fit,side,eq){this.B=B;this.fit=fit;this.eq=eq;this.H=fit.H;this.colors=palette(side,eq);this.item=null;this._color=[0,0,0];}
    color(key='cloth',value=1){
      const source=this.colors[key],factor=value*(this.item?this.item.variant.shade:1),fade=(key==='cloth'?.10*this.eq.wear:0)*.18,out=this._color;
      let c=Math.min(1,source[0]*factor);out[0]=c+(1-c)*fade;
      c=Math.min(1,source[1]*factor);out[1]=c+(1-c)*fade;
      c=Math.min(1,source[2]*factor);out[2]=c+(1-c)*fade;
      return out;
    }
    box(p,s,w,c=this.color(),m=0,q=null){this.B.box(p,s,w,c,m,.20,q);}
    line(points,w,c=this.color('web'),width=.010){this.B.ribbon(points,width,w,c);}
    pouch(p,size,w,normal=Z,style='utility'){
      const q=aroundNormal(normal),offset=(x,y,z)=>p.clone().add(V(x,y,z).applyQuaternion(q));
      this.box(p,size,w,this.color('cloth'),0,q);
      this.box(offset(0,size.y*.30,size.z*.48),V(size.x*1.025,size.y*.24,.003),w,this.color('edge'),0,q);
      const front=size.z*.54;
      this.box(offset(0,0,front),V(.008,size.y*.50,.003),w,this.color('web'),0,q);
      this.box(offset(0,-size.y*.12,front+.001),V(.014,.010,.004),w,this.color('rubber'),1,q);
      if(style==='medical'||style==='tools'){
        for(const sign of [-1,1])this.box(offset(sign*size.x*.22,-size.y*.06,front+.001),V(.004,size.y*.55,.002),w,this.color('light',.6),0,q);
      }
    }
  };

  function headgear(ctx,def){
    const {B,fit}=ctx,F=fit.face,w=weights('head'),N=B.high?48:B.far?16:32,rows=B.high?14:B.far?6:10,style=def.visual.style,helmet=def.kind==='helmet';
    let last=null,first=null;
    const padding=(helmet?.008:.0035)/ctx.H,extra=(helmet?.017:(style==='beanie'?.018:.014))/ctx.H;
    for(let r=0;r<rows;r++){
      const t=r/rows,loop=[];
      for(let j=0;j<N;j++){
        const theta=-Math.PI+2*Math.PI*j/N,bottom=fit.headBottom(theta),y=M.mix(bottom,F.topY-.0001,t),p=F.point(y,theta);
        const round=Math.cos(t*Math.PI/2),bulk=helmet?.004:.0015;
        p.x+=Math.sin(theta)*(padding+bulk)*round;p.z+=Math.cos(theta)*(padding+bulk)*round;p.y+=extra*Math.sin(t*Math.PI/2);
        if(style==='beret'){p.x+=.013*Math.sin(t*Math.PI/2);p.y+=.005*Math.sin(theta)*round;}
        if(style==='patrol'&&t>.45)p.y-=.006*Math.sin((t-.45)/.55*Math.PI);
        const color=ctx.color(style==='cover'||!helmet?'cloth':'edge',1+.025*Math.sin(theta*10+t*8));
        loop.push(B.vertex(p,w,color,V(Math.sin(theta)*round,t,Math.cos(theta)*round),[j/N*3,t*4]));
      }
      if(last)B.bridge(last,loop,helmet?1:0);else first=loop;last=loop;
    }
    const crown=F.point(F.topY,0);crown.y+=extra;if(style==='beret')crown.x+=.013;
    const tip=B.vertex(crown,w,ctx.color(helmet?'edge':'cloth'),Y);for(let j=0;j<N;j++)B.triangle(last[j],tip,last[(j+1)%N],helmet?1:0);
    const rim=[];for(let j=0;j<N;j++)rim.push(B.point(first[j]));rim.push(rim[0].clone());
    B.tube(rim,.0018,w,ctx.color(helmet?'rubber':'web'),helmet?1:0);
    // Return lip: gives a physical shell thickness at the open lower edge.
    const inner=first.map(i=>{const p=B.point(i),a=Math.atan2(p.x,p.z);p.x-=Math.sin(a)*padding*.7;p.z-=Math.cos(a)*padding*.7;p.y+=.0007;return B.vertex(p,w,ctx.color('web'),Y);});B.bridge(first,inner,1);
    if(style==='cap'||style==='patrol'){
      const y=fit.headBottom(0)+.0015,rx=F.section(y)[1],z=F.frontZ(0,y);
      const grid=[];for(let r=0;r<=4;r++){
        const row=[],t=r/4;for(let j=0;j<=16;j++){
          const x=(j/16*2-1)*rx*.90,p=V(x,y-.007*t*t,z+.037*t*(.73+.27*Math.cos(x/rx*Math.PI/2))-.009*(x/rx)**2);
          row.push(B.vertex(p,w,ctx.color('cloth',.90),Y,[j/16,t]));
        }grid.push(row);
      }
      for(let r=0;r<4;r++)for(let j=0;j<16;j++){B.triangle(grid[r][j],grid[r+1][j],grid[r][j+1]);B.triangle(grid[r][j+1],grid[r+1][j],grid[r+1][j+1]);}
      B.tube(grid[4].map(i=>B.point(i)),.0012,w,ctx.color('web'));
    }
    if(style==='boonie'){
      const rings=[];for(let r=0;r<3;r++){
        const loop=[];for(let j=0;j<N;j++){
          const a=j/N*2*Math.PI,bottom=fit.headBottom(a),p=F.point(bottom,a),rad=.004+r*.012;
          p.add(V(Math.sin(a)*rad,-.002+r*.001*Math.sin(a*2),Math.cos(a)*rad));loop.push(B.vertex(p,w,ctx.color('cloth'),Y));
        }if(r)B.bridge(rings[r-1],loop);rings.push(loop);
      }
      const edge=rings[2].map(i=>B.point(i));edge.push(edge[0]);B.tube(edge,.0012,w,ctx.color('web'));
    }
    if(helmet){
      const jawY=F.chinY+.006;
      for(const sign of [-1,1]){
        const a=sign*Math.PI*.5,y=fit.headBottom(a),p=F.point(y,a),under=V(sign*F.section(jawY)[1]*.45,jawY-.004,F.frontZ(sign*.015,jawY)+.002);
        ctx.line([p.clone().add(V(sign*.005,0,0)),V(sign*F.section(.904)[1]*1.03,.908,.022),under],w,ctx.color('web'),.0045);
        ctx.box(p.clone().add(V(sign*.006,.005,0)),V(.006,.014,.022),w,ctx.color('rubber'),1);
      }
      if(style==='cover')for(const s of [-1,1]){
        const points=[];for(let i=0;i<9;i++){const theta=s*(.18+i*.12),p=F.point(.976,theta);p.add(V(Math.sin(theta)*.009,.009,Math.cos(theta)*.009));points.push(p);}
        B.tube(points,.0008,w,ctx.color('web'));
      }
    }
  }

  function eyewear(ctx,def){
    const {B,fit}=ctx,w=weights('head'),F=fit.face,goggles=def.visual.style==='goggles';
    const ends=[];
    for(const s of ['L','R']){
      const e=F.eyes[s],p=V(e.x,e.y,F.globeFront(e,e.x,e.y)+.0025),rx=e.w*(goggles?1.5:1.25),ry=e.h*(goggles?1.75:1.4),loop=[];
      for(let j=0;j<=24;j++){const a=j/24*2*Math.PI;loop.push(p.clone().add(V(rx*Math.cos(a),ry*Math.sin(a),-.001*Math.sin(a)**2)));}
      B.tube(loop,goggles?.0018:.0008,w,ctx.color('rubber'),1);
      const inner=p.clone().add(V(-e.sign*rx,0,0));ends.push(inner);
      const outer=p.clone().add(V(e.sign*rx,0,0)),side=F.point(e.y,e.sign*Math.PI/2);side.x+=e.sign*.003;
      B.tube([outer,outer.clone().add(V(e.sign*.004,0,-.010)),side],goggles?.0017:.0008,w,ctx.color('web'),1);
    }
    B.tube(ends,.0009,w,ctx.color('metal'),1);
    // Lenses intentionally remain clear/open: no opaque disc hiding the eyes.
  }
  function mask(ctx,def){
    if(def.visual.style==='balaclava')return; // Conforming skin-region cloth in base surface.
    const F=ctx.fit.face,y=F.noseBaseY-.006,z=F.frontZ(0,y)+.009,w=weights('head');
    ctx.box(V(0,y,z),V(.038,.033,.021),w,ctx.color('rubber'),1);
    for(const sign of [-1,1]){
      const p=V(sign*.024,y-.004,z),end=p.clone().add(V(0,0,.012));
      ctx.B.cylinder(p,end,.010,w,ctx.color('edge'),1,12);
      for(let i=-2;i<=2;i++)ctx.B.tube([end.clone().add(V(-.006,i*.002,0)),end.clone().add(V(.006,i*.002,0))],.0005,w,ctx.color('rubber'),1);
    }
  }
  function neckwear(ctx,def){
    const F=ctx.fit.face,{B}=ctx,N=32;let last=null;
    for(let r=0;r<=5;r++){
      const y=.847+r*.0045,sec=F.section(y),radius=.003+Math.sin(r/5*Math.PI)*.002;
      const ring=B.ring(V(0,y,sec[3]),X,Z,sec[1]+radius,sec[2]+radius,N,p=>F.neckWeights(p.y),ctx.color('cloth',r%2?.87:1));
      if(last)B.bridge(last,ring);last=ring;
    }
    if(def.visual.style==='scarf'){
      const p=ctx.fit.front(.018,.807,.008);ctx.box(p,V(.023,.072,.007),p=>ctx.fit.torsoWeights(p),ctx.color('cloth'));
    }
  }
  function torsoPanel(ctx,back,y0,y1,widthFactor,gap,color){
    const {B,fit}=ctx,grid=[],rows=8,cols=12,sign=back?-1:1;
    for(let r=0;r<=rows;r++){
      const y=M.mix(y0,y1,r/rows),rx=fit.profile(y)[0],row=[];
      const taper=1-.16*Math.max(0,(r/rows-.68)/.32);
      for(let j=0;j<=cols;j++){
        const x=(j/cols*2-1)*rx*widthFactor*taper,p=fit.front(x,y,gap);p.z*=sign;
        row.push(B.vertex(p,fit.torsoWeights(p),color,V(x,0,sign*.2).normalize(),[j/cols*3,r/rows*5]));
      }grid.push(row);
    }
    for(let r=0;r<rows;r++)for(let j=0;j<cols;j++){B.triangle(grid[r][j],grid[r+1][j],grid[r][j+1]);B.triangle(grid[r][j+1],grid[r+1][j],grid[r+1][j+1]);}
    for(const row of [grid[0],grid[rows]])B.tube(row.map(i=>B.point(i)),.0015,p=>fit.torsoWeights(p),ctx.color('web'));
    for(const col of [0,cols])B.tube(grid.map(row=>B.point(row[col])),.0015,p=>fit.torsoWeights(p),ctx.color('web'));
  }
  function shoulderStraps(ctx,gap,width){
    const F=ctx.fit,y0=F.mapTorsoY(.665),y1=F.mapTorsoY(.790),shoulderY=F.A.shoulderY+.012;
    for(const sign of [-1,1]){
      const x=sign*F.profile(y1)[0]*.65,points=[];
      for(let i=0;i<7;i++){const y=M.mix(y0,y1,i/6),p=F.front(x*.86,y,gap);points.push(p);}
      points.push(V(x,shoulderY,.02),V(x,shoulderY,-.02));
      for(let i=0;i<7;i++){const y=M.mix(y1,y0,i/6),p=F.front(x*.86,y,gap);p.z=-p.z;points.push(p);}
      ctx.line(points,p=>F.torsoWeights(p),ctx.color('web'),width);
    }
  }
  function armor(ctx,def){
    const F=ctx.fit,gap=F.armorGap()+.003,y0=F.mapTorsoY(def.visual.style==='heavy'?.603:.635),y1=F.mapTorsoY(.793);
    for(const back of [false,true])torsoPanel(ctx,back,y0,y1,def.visual.style==='heavy'?.94:.79,gap,ctx.color('cloth',.87));
    shoulderStraps(ctx,gap+.001,.022);
    // Side panels follow the same deformation weights as the jacket underneath.
    for(const sign of [-1,1]){
      const y=F.mapTorsoY(.673),rx=F.profile(y)[0],p=V(sign*(rx+.004),y,0);
      ctx.box(p,V(.009,.037,F.profile(y)[1]*1.7),p=>F.torsoWeights(p),ctx.color('web'));
    }
    if(def.visual.style==='heavy'){
      const y=F.mapTorsoY(.603),p=F.front(0,y,gap+.004);
      ctx.box(p,V(F.profile(y)[0]*1.25,.040,.009),p=>F.torsoWeights(p),ctx.color('edge'));
    }
  }
  function chestRig(ctx,def){
    const F=ctx.fit,y=F.mapTorsoY(.711),gap=F.armorGap()+.010,w=p=>F.torsoWeights(p),count=def.visual.count;
    if(!F.armor.thickness)shoulderStraps(ctx,gap,.013);
    const width=F.profile(y)[0]*1.40/count,height=def.visual.style==='ammo'?.066:.056;
    for(let i=0;i<count;i++){
      const x=(i-(count-1)*.5)*(width+.003),p=F.front(x,y,gap+.010),normal=V(x*.8,0,1).normalize();
      ctx.pouch(p,V(width,height,.024),w(p),normal,def.visual.style);
    }
    const a=F.front(-F.profile(y)[0]*.78,y-.035,gap),b=F.front(F.profile(y)[0]*.78,y-.035,gap);
    ctx.B.ribbon([a,F.front(0,y-.035,gap),b],.010,w,ctx.color('web'),0,Y);
  }
  function backpack(ctx,def){
    const {fit:F,B}=ctx,d=F.packDimensions,w=weights('spineUpper'),socket=F.socket('BACK_CENTER'),c=socket.position.clone().add(V(0,0,-d[2]/2));
    ctx.box(c,V(...d),w,ctx.color('cloth'));
    const back=c.clone().add(V(0,0,-d[2]*.5-.004));
    ctx.box(c.clone().add(V(0,d[1]*.39,-.002)),V(d[0]*1.03,d[1]*.20,d[2]*1.04),w,ctx.color('edge'));
    const sides=def.visual.style==='medical'?2:1;
    for(const sign of [-1,1])ctx.pouch(c.clone().add(V(sign*d[0]*.53,-d[1]*.12,-.006)),V(d[0]*.28,d[1]*.50,d[2]*.68),w,V(sign,0,-.3).normalize(),def.visual.style==='medical'?'medical':'utility');
    for(let i=0;i<sides;i++)ctx.pouch(back.clone().add(V((i-(sides-1)*.5)*d[0]*.45,-d[1]*.12,-.008)),V(d[0]*.72/sides,d[1]*.44,.021),w,V(0,0,-1),def.visual.style);
    for(const sign of [-1,1]){
      const x=sign*d[0]*.29;
      B.ribbon([c.clone().add(V(x,d[1]*.42,-d[2]*.54)),c.clone().add(V(x,0,-d[2]*.54)),c.clone().add(V(x,-d[1]*.42,-d[2]*.54))],.009,w,ctx.color('web'));
      ctx.box(c.clone().add(V(x,-d[1]*.16,-d[2]*.55)),V(.016,.013,.006),w,ctx.color('rubber'),1);
    }
    const handle=[c.clone().add(V(-.019,d[1]*.5,0)),c.clone().add(V(-.016,d[1]*.55,0)),c.clone().add(V(.016,d[1]*.55,0)),c.clone().add(V(.019,d[1]*.5,0))];B.tube(handle,.0022,w,ctx.color('web'));
    shoulderStraps(ctx,F.armorGap()+.011,.015);
    if(def.visual.roll){
      const yy=c.y-d[1]*.49-.028;B.cylinder(V(-d[0]*.6,yy,c.z),V(d[0]*.6,yy,c.z),.026,w,ctx.color('cloth',.80),0,16);
      for(const sign of [-1,1])B.cylinder(V(sign*d[0]*.34-.003,yy,c.z),V(sign*d[0]*.34+.003,yy,c.z),.027,w,ctx.color('web'),0,16);
    }
    if(def.visual.style==='radio'){
      ctx.box(c.clone().add(V(.012,d[1]*.5+.008,0)),V(d[0]*.72,.020,d[2]*.80),w,ctx.color('rubber'),1);
      const p=c.clone().add(V(-d[0]*.30,d[1]*.52,-d[2]*.12));
      B.cylinder(p,p.clone().add(V(.016,.23,-.012)),.0014,w,ctx.color('metal'),2);
      B.cylinder(p,p.clone().add(V(.002,.033,-.002)),.0035,w,ctx.color('rubber'),1);
      const cable=[p.clone().add(V(.033,0,0)),c.clone().add(V(d[0]*.55,d[1]*.4,.025)),F.front(F.profile(F.mapTorsoY(.77))[0]*.7,F.mapTorsoY(.77),.016+F.armorGap())];
      B.tube(cable,.0015,p=>F.torsoWeights(p),ctx.color('rubber'),1);
    }
    if(def.visual.style==='engineer'){
      const p=back.clone().add(V(-d[0]*.30,.005,-.03));
      B.cylinder(p.clone().add(V(0,-.05,0)),p.clone().add(V(0,.065,0)),.0038,w,ctx.color('edge'),1);
      ctx.box(p.clone().add(V(0,-.071,0)),V(.035,.045,.004),w,ctx.color('metal'),2);
      B.cylinder(p.clone().add(V(-.012,.079,0)),p.clone().add(V(.012,.079,0)),.0035,w,ctx.color('rubber'),1);
    }
  }
  function belt(ctx,def){
    const F=ctx.fit,y=F.A.hipY+.009,rx=F.profile(y)[0]+.004,rz=F.profile(y)[1]+.004,N=48,h=def.visual.style==='utility'?.014:.009,w=weights('hips');
    const a=ctx.B.ring(V(0,y-h/2,0),X,Z,rx,rz,N,w,ctx.color('web')),b=ctx.B.ring(V(0,y+h/2,0),X,Z,rx,rz,N,w,ctx.color('web'));ctx.B.bridge(a,b);
    ctx.box(V(0,y,rz+.002),V(.024,h*.85,.006),w,ctx.color('rubber'),1);
  }
  function accessory(ctx,def,slot){
    const F=ctx.fit,so=F.socket(R.EquipmentSlots[slot].socket),normal=so.normal,w=weights(so.bone),p=so.position.clone(),style=def.visual.style;
    const q=aroundNormal(normal),size=ctx.item.variant.size;
    if(style==='binoculars'){
      for(const sign of [-1,1]){
        const a=p.clone().add(V(sign*.012,0,0).applyQuaternion(q)),b=a.clone().add(V(0,-.030,.003).applyQuaternion(q));
        ctx.B.cylinder(a,b,.009,w,ctx.color('rubber'),1,12);
      }
      ctx.box(p,V(.028,.010,.012),w,ctx.color('metal'),1,q);return;
    }
    if(style==='radio'){
      ctx.box(p,V(.020,.037,.016),w,ctx.color('rubber'),1,q);
      const top=p.clone().add(V(-.007,.018,0).applyQuaternion(q));ctx.B.cylinder(top,top.clone().add(V(0,.045,0).applyQuaternion(q)),.0012,w,ctx.color('metal'),2);
      for(let i=0;i<4;i++)ctx.box(p.clone().add(V(0,.007-i*.004,.009).applyQuaternion(q)),V(.014,.0015,.001),w,ctx.color('edge'),1,q);return;
    }
    if(style==='canteen'){
      ctx.B.ellipsoid(p,V(.025,.037,.018),w,ctx.color('edge'),0,12,8,q);
      ctx.box(p.clone().add(V(0,.037,0).applyQuaternion(q)),V(.016,.013,.015),w,ctx.color('rubber'),1,q);
      ctx.line([p.clone().add(V(-.020,.02,.017).applyQuaternion(q)),p.clone().add(V(.020,.02,.017).applyQuaternion(q))],w,ctx.color('web'),.006);return;
    }
    const dims=style==='map'?V(.050,.057,.012):V(.043,.057,.025);dims.multiplyScalar(size);
    ctx.pouch(p,dims,w,normal,style);
    if(slot.includes('Thigh')){
      const s=slot==='leftThigh'?'L':'R',center=F.A.points['thigh.'+s].clone().multiplyScalar(1/F.H);center.y=p.y;
      const r=.055*F.body.legThicknessScale+.003,one=ctx.B.ring(center.clone().add(V(0,-.004,0)),X,Z,r,r,32,w,ctx.color('web')),two=ctx.B.ring(center.clone().add(V(0,.004,0)),X,Z,r,r,32,w,ctx.color('web'));ctx.B.bridge(one,two);
    }
  }
  function clothingDetails(ctx,slot,def){
    const F=ctx.fit,A=F.A;
    if(slot==='legs'&&def.visual.pads){
      for(const s of ['L','R']){
        const p=A.points['shin.'+s].clone().multiplyScalar(1/F.H).add(V(0,.009,.038*F.body.legThicknessScale));
        ctx.box(p,V(.047*F.body.legThicknessScale,.058,.009),weights('shin.'+s),ctx.color('rubber'),1);
        for(let i=-1;i<=1;i++)ctx.box(p.clone().add(V(0,i*.014,.006)),V(.036,.005,.003),weights('shin.'+s),ctx.color('edge'),1);
      }
    }
    // Gloves/boots/jacket are fitted by the base surface; no duplicate shell.
  }
  function generateSlot(ctx,slot,def){
    const B=ctx.B;
    switch(def.kind){
      case 'cap':case 'helmet':headgear(ctx,def);break;
      case 'eyewear':eyewear(ctx,def);break;
      case 'mask':mask(ctx,def);break;
      case 'neckwear':neckwear(ctx,def);break;
      case 'armor':armor(ctx,def);break;
      case 'rig':chestRig(ctx,def);break;
      case 'pack':backpack(ctx,def);break;
      case 'belt':belt(ctx,def);break;
      case 'pouch':accessory(ctx,def,slot);break;
      case 'clothing':clothingDetails(ctx,slot,def);break;
      case 'weapon':weapon(ctx,def);break;
      default:throw new Error('Brak generatora dla '+def.kind);
    }
  }
  function fitGeometryKey(rig,equipment){
    const fitSlots=['torsoBase','legs','feet','hands','torsoArmor','back','head'],inputs=[];
    for(const slot of fitSlots){const item=equipment.slots[slot];inputs.push(item?[item.definitionId,item.seed,item.variant]:null);}
    const A=rig.anatomy;return JSON.stringify([A.height,A.body,A.face,A.points,inputs]);
  }
  function partKey(rig,side,equipment,fit,detail,slot,item){
    return JSON.stringify([R.Config.GENERATOR_VERSION,detail,side.uniformColor,equipment.palette,equipment.wear,fitGeometryKey(rig,equipment),slot,item.definitionId,item.seed,item.variant]);
  }
  function partSize(part){let bytes=0;for(const name in part.geometry.attributes)bytes+=part.geometry.attributes[name].array.byteLength;bytes+=part.geometry.index.array.byteLength;return bytes;}
  function cachePart(key,part){
    if(partCacheLimit<=0){part.geometry.dispose();return;}
    const bytes=partSize(part);if(bytes>partCacheLimit){part.geometry.dispose();return;}
    const previous=PART_CACHE.get(key);if(previous){partCacheBytes-=previous.bytes;previous.part.geometry.dispose();PART_CACHE.delete(key);}
    PART_CACHE.set(key,{part,bytes});partCacheBytes+=bytes;
    while(partCacheBytes>partCacheLimit&&PART_CACHE.size){const first=PART_CACHE.keys().next().value,entry=PART_CACHE.get(first);PART_CACHE.delete(first);partCacheBytes-=entry.bytes;entry.part.geometry.dispose();}
  }
  function* composePartsSteps(parts){
    let vertexCount=0,indexCount=0,triangles=0;const attrNames=Object.keys(parts[0].part.geometry.attributes),materialCounts=[0,0,0];
    for(const entry of parts){const g=entry.part.geometry;vertexCount+=g.attributes.position.count;indexCount+=g.index.array.length;triangles+=entry.part.triangles;for(const group of g.groups)materialCounts[group.materialIndex]+=group.count;}
    // A scalar material uses one draw for the whole geometry, so retain each
    // former material region in a compact vertex attribute. Split only the
    // few vertices shared by different groups; their positions/skin data stay
    // identical and all triangle indices keep their original order.
    const vertexRegions=new Uint8Array(vertexCount);vertexRegions.fill(255);
    const regionRemap=new Map(),duplicateSources=[],duplicateRegions=[];let sourceOffset=0,regionWork=0;
    for(const entry of parts){const g=entry.part.geometry,source=g.index.array;
      for(const group of g.groups){const region=group.materialIndex;
        for(let i=group.start;i<group.start+group.count;i++){
          const vertex=source[i]+sourceOffset,previous=vertexRegions[vertex];
          if(previous===255)vertexRegions[vertex]=region;
          else if(previous!==region){const key=vertex*3+region;if(!regionRemap.has(key)){regionRemap.set(key,vertexCount+duplicateSources.length);duplicateSources.push(vertex);duplicateRegions.push(region);}}
          if((++regionWork&16383)===0)yield;
        }
      }
      sourceOffset+=g.attributes.position.count;
    }
    for(let i=0;i<vertexCount;i++)if(vertexRegions[i]===255)vertexRegions[i]=0;
    const outputVertexCount=vertexCount+duplicateSources.length,attrs={};
    for(const name of attrNames){const exemplar=parts[0].part.geometry.attributes[name];attrs[name]=new exemplar.array.constructor(outputVertexCount*exemplar.itemSize);}
    attrs.materialRegion=new Uint8Array(outputVertexCount);attrs.materialRegion.set(vertexRegions);
    for(let i=0;i<duplicateSources.length;i++)attrs.materialRegion[vertexCount+i]=duplicateRegions[i];
    const IndexArray=outputVertexCount>65535?Uint32Array:Uint16Array,index=new IndexArray(indexCount),geometry=new THREE.BufferGeometry();
    let vertexOffset=0;
    for(const entry of parts){const g=entry.part.geometry,vertices=g.attributes.position.count;
      for(const name of attrNames){const src=g.attributes[name].array,dst=attrs[name],offset=vertexOffset*g.attributes[name].itemSize;dst.set(src,offset);}
      const tags=entry.part.tags;for(const name in tags){let list=geometry._gearTags&&geometry._gearTags[name];if(!geometry._gearTags)geometry._gearTags={};list=geometry._gearTags[name]||(geometry._gearTags[name]=[]);for(const local of tags[name])list.push(local+vertexOffset);}
      entry.vertexOffset=vertexOffset;vertexOffset+=vertices;yield;
    }
    for(let i=0;i<duplicateSources.length;i++){
      const sourceVertex=duplicateSources[i],targetVertex=vertexCount+i;
      for(const name of attrNames){const exemplar=parts[0].part.geometry.attributes[name],itemSize=exemplar.itemSize,attribute=attrs[name];
        for(let channel=0;channel<itemSize;channel++)attribute[targetVertex*itemSize+channel]=attribute[sourceVertex*itemSize+channel];
      }
      if((i&4095)===4095)yield;
    }
    let indexOffset=0;
    for(let material=0;material<materialCounts.length;material++){
      const start=indexOffset;
      for(const entry of parts){const g=entry.part.geometry,base=entry.vertexOffset,source=g.index.array;
        for(const group of g.groups)if(group.materialIndex===material)for(let i=0;i<group.count;i++){
          const vertex=source[group.start+i]+base,remapped=regionRemap.get(vertex*3+material);index[indexOffset++]=remapped===undefined?vertex:remapped;
          if((indexOffset&16383)===0)yield;
        }
      }
      if(indexOffset>start)geometry.addGroup(start,indexOffset-start,material);
    }
    for(const name of attrNames){const exemplar=parts[0].part.geometry.attributes[name];geometry.setAttribute(name,new THREE.BufferAttribute(attrs[name],exemplar.itemSize,exemplar.normalized));}
    geometry.setAttribute('materialRegion',new THREE.BufferAttribute(attrs.materialRegion,1));
    geometry.setIndex(new THREE.BufferAttribute(index,1));geometry.computeBoundingBox();geometry.computeBoundingSphere();
    const tags=geometry._gearTags||{};delete geometry._gearTags;
    const records=[];for(const entry of parts)records.push(...entry.part.records);
    return {geometry,tags,records,vertices:vertexCount,triangles};
  }
  function weapon(ctx,def){
    const F=ctx.fit,style=def.visual.style;
    const name=style==='sidearm'?'WEAPON_HIP':style==='knife'?'HIP_R':style==='grenade'?'WAIST_FRONT':'WEAPON_BACK';
    const so=F.socket(name),w=weights(so.bone),p=so.position.clone();
    // Holders remain on the body; complete rigid props are owned by WeaponController.
    if(style==='sidearm')ctx.pouch(p,V(.030,.057,.018),w,so.normal,'utility');
    else if(style==='knife')ctx.box(p,V(.020,.052,.012),w,ctx.color('rubber'),1);
    else if(style==='grenade')ctx.pouch(p,V(.033,.029,.017),w,so.normal,'utility');
    else ctx.line([p.clone().add(V(-.016,.028,.003)),p.clone().add(V(.016,.028,.003))],w,ctx.color('web'),.009);
  }

  R.InfantryGear={
    palette,
    slotCacheStats(){return {entries:PART_CACHE.size,bytes:partCacheBytes,limit:partCacheLimit,hits:partCacheHits,misses:partCacheMisses};},
    setSlotCacheLimit(bytes){if(!Number.isFinite(bytes)||bytes<0)throw new RangeError('Gear slot cache limit must be nonnegative');partCacheLimit=bytes;while(partCacheBytes>partCacheLimit&&PART_CACHE.size){const first=PART_CACHE.keys().next().value,entry=PART_CACHE.get(first);PART_CACHE.delete(first);partCacheBytes-=entry.bytes;entry.part.geometry.dispose();}},
    clearSlotCache(){for(const entry of PART_CACHE.values())entry.part.geometry.dispose();PART_CACHE.clear();partCacheBytes=0;},
    build(rig,side,equipment,detail='high',fit=null){const steps=this.buildSteps(rig,side,equipment,detail,fit);let step=steps.next();while(!step.done)step=steps.next();return step.value;},
    async buildAsync(rig,side,equipment,detail='high',fit=null,yieldFrame,check=null,budgetMs=4){
      if(typeof yieldFrame!=='function')throw new TypeError('Async infantry gear generation requires a frame-yield callback.');
      const budget=Number.isFinite(budgetMs)?Math.max(0,budgetMs):4,now=()=>typeof performance!=='undefined'&&performance.now?performance.now():Date.now();
      const steps=this.buildSteps(rig,side,equipment,detail,fit);let step=steps.next(),sliceStart=budget>0?now():0,checkpointsSinceClock=0;
      while(!step.done){
        if(check)check();
        // Gear generation already yields from bounded geometry checkpoints.
        // Sample the clock every eight checkpoints, but keep cancellation
        // checks on every checkpoint and immediately after each frame yield.
        const timeDue=budget>0&&++checkpointsSinceClock>=8;
        if(budget===0||(timeDue&&(checkpointsSinceClock=0,now()-sliceStart>=budget))){await yieldFrame();sliceStart=budget>0?now():0;if(check)check();}
        step=steps.next();
      }
      return step.value;
    },
    *buildSteps(rig,side,equipment,detail='high',fit=null){
      fit=fit||new R.EquipmentFit(rig.anatomy,equipment);
      const parts=[];
      for(const [slot,item]of Object.entries(equipment.slots)){
        if(!item)continue;
        const key=partKey(rig,side,equipment,fit,detail,slot,item),cached=PART_CACHE.get(key);
        if(cached){PART_CACHE.delete(key);PART_CACHE.set(key,cached);partCacheHits++;parts.push({slot,part:cached.part});yield;continue;}
        partCacheMisses++;
        const B=new R.GearGeometry(rig,detail),ctx=new C(B,fit,side,equipment);ctx.item=item;B.currentTag='gear.'+slot;
        const def=R.EquipmentCatalog.get(item.definitionId),before=B.positions.length/3;generateSlot(ctx,slot,def);
        const geometryResult=yield* B.finishSteps();
        const part={geometry:geometryResult.geometry,tags:geometryResult.tags,triangles:geometryResult.triangles,
          records:[{slot,id:def.id,label:def.label,vertices:geometryResult.vertices-before}]};
        cachePart(key,part);parts.push({slot,part});
        yield;
      }
      let surface;
      if(parts.length)surface=yield* composePartsSteps(parts);
      else{const empty=new R.GearGeometry(rig,detail);surface=yield* empty.finishSteps();}
      const samples=new Set(),positions=surface.geometry.attributes.position.array;
      for(const list of Object.values(surface.tags)){
        const step=Math.max(1,Math.floor(list.length/10));for(let i=0;i<list.length;i+=step)samples.add(list[i]);
        // Include extreme points of each item, not just evenly spaced vertices.
        for(let axis=0;axis<3;axis++)for(const sign of [-1,1]){
          let best=list[0];for(const i of list)if(sign*positions[3*i+axis]>sign*positions[3*best+axis])best=i;
          if(best!==undefined)samples.add(best);
          yield;
        }
        yield;
      }
      const headVertices=new Set(['head','face','neck'].flatMap(slot=>surface.tags['gear.'+slot]||[]));
      const headContactIndices=Array.from(samples).filter(i=>headVertices.has(i));
      return {...surface,fit,contactIndices:Array.from(samples),headContactIndices};
    },
    materials(regionAware=false){
      if(regionAware)return R.InfantryMaterials._regionGear;
      if(!sharedMaterials)sharedMaterials=[
        new THREE.MeshStandardMaterial({color:0xffffff,vertexColors:true,skinning:true,roughness:.95,metalness:0,side:THREE.DoubleSide}),
        new THREE.MeshStandardMaterial({color:0xffffff,vertexColors:true,skinning:true,roughness:.72,metalness:0,side:THREE.DoubleSide}),
        new THREE.MeshStandardMaterial({color:0xffffff,vertexColors:true,skinning:true,roughness:.56,metalness:.35})
      ];
      return sharedMaterials;
    },
    dispose(){if(sharedMaterials)sharedMaterials.forEach(m=>m.dispose());sharedMaterials=null;}
  };
})();
