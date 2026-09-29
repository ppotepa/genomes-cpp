(function(){
  'use strict';
  const R=globalThis.RTS,STRIDE=21;
  // Visual state only: life, lifetime, position[3], velocity[3], size, end size,
  // angle, spin, color[3], gravity, drag, kind, surface normal[3].
  const LIMITS={flash:[16,4],dust:[128,40],chips:[256,80],marks:[128,32]};
  const PALETTES={
    brick:{dust:[.64,.40,.29],chip:[.51,.27,.16]},
    concrete:{dust:[.64,.63,.58],chip:[.52,.52,.47]},
    rock:{dust:[.55,.54,.50],chip:[.39,.40,.37]},
    wood:{dust:[.62,.48,.30],chip:[.40,.27,.13]},
    steel:{dust:[.35,.36,.35],chip:[.34,.37,.39],metal:true},
    armor:{dust:[.30,.32,.31],chip:[.31,.35,.36],metal:true},
    glass:{dust:[.68,.79,.80],chip:[.78,.92,.95]},
    foliage:{dust:[.37,.44,.26],chip:[.20,.31,.12]},
    tissue:{dust:[.47,.38,.30],chip:[.37,.29,.23]}
  };
  const clamp=(value,min,max)=>Math.max(min,Math.min(max,value));
  function surfaceBasis(normal){const n=new THREE.Vector3(...normal).normalize(),ref=Math.abs(n.y)<.9?new THREE.Vector3(0,1,0):new THREE.Vector3(1,0,0),u=new THREE.Vector3().crossVectors(ref,n).normalize(),v=new THREE.Vector3().crossVectors(n,u).normalize();return {u:u.toArray(),v:v.toArray()};}
  function random(seed){let state=(seed>>>0)||0x6d2b79f5;return ()=>{state^=state<<13;state^=state>>>17;state^=state<<5;return (state>>>0)/4294967296;};}
  function opacityTexture(){
    const side=64,pixels=new Uint8Array(side*side*4);
    for(let y=0;y<side;y++)for(let x=0;x<side;x++){
      const u=(x+.5)/side*2-1,v=(y+.5)/side*2-1;
      const edge=Math.max(0,1-u*u-v*v);
      const cloud=.68+.14*Math.sin(u*9+v*5)+.10*Math.sin(u*17-v*11)+.08*Math.cos(u*25+v*19);
      const i=(y*side+x)*4;pixels[i]=pixels[i+1]=pixels[i+2]=255;
      pixels[i+3]=Math.round(255*edge*edge*clamp(cloud,.35,1));
    }
    const texture=new THREE.DataTexture(pixels,side,side,THREE.RGBAFormat);
    texture.magFilter=texture.minFilter=THREE.LinearFilter;texture.generateMipmaps=false;texture.needsUpdate=true;
    return texture;
  }
  const spriteVertex=`
    attribute vec3 iPosition;
    attribute vec2 iSize;
    attribute vec4 iTint;
    attribute float iAngle;
    attribute vec3 iNormal;
    varying vec2 vUv;
    varying vec4 vTint;
    void main(){
      vUv=uv;vTint=iTint;
      vec2 p=position.xy*iSize;
      float c=cos(iAngle),s=sin(iAngle);
      p=mat2(c,s,-s,c)*p;
      #ifdef SURFACE_MARK
        vec3 n=normalize(iNormal);
        vec3 u=normalize(cross(abs(n.y)<.9?vec3(0.,1.,0.):vec3(1.,0.,0.),n));
        vec3 v=cross(n,u);
        gl_Position=projectionMatrix*viewMatrix*vec4(iPosition+u*p.x+v*p.y,1.);
      #else
        vec4 center=viewMatrix*vec4(iPosition,1.);
        center.xy+=p;
        gl_Position=projectionMatrix*center;
      #endif
    }`;
  const spriteFragment=`
    uniform sampler2D opacityMap;
    varying vec2 vUv;
    varying vec4 vTint;
    void main(){
      float alpha=texture2D(opacityMap,vUv).a*vTint.a;
      if(alpha<.003)discard;
      gl_FragColor=vec4(vTint.rgb,alpha);
      #include <encodings_fragment>
    }`;
  const chipVertex=`
    attribute vec4 iTint;
    varying vec4 vTint;
    void main(){
      vec3 n=normalize(mat3(instanceMatrix)*normal);
      float light=.58+.42*abs(dot(n,normalize(vec3(.4,.9,.2))));
      vTint=vec4(iTint.rgb*light,iTint.a);
      gl_Position=projectionMatrix*modelViewMatrix*instanceMatrix*vec4(position,1.);
    }`;
  const chipFragment=`
    varying vec4 vTint;
    void main(){
      if(vTint.a<.003)discard;
      gl_FragColor=vTint;
      #include <encodings_fragment>
    }`;

  class Pool{
    constructor(name){this.name=name;this.capacity=LIMITS[name][0];this.count=0;this.cursor=0;this.data=new Float32Array(this.capacity*STRIDE);this.owners=new Array(this.capacity);}
    add(point,vx,vy,vz,life,size,endSize,color,angle=0,spin=0,gravity=0,drag=0,kind=0,normal=[0,0,1],owner=null,low=false){
      const limit=LIMITS[this.name][low?1:0];if(this.count>limit)this.count=limit;
      const id=this.count<limit?this.count++:this.cursor++%limit,o=id*STRIDE,d=this.data;
      d[o]=d[o+1]=life;d[o+2]=point[0];d[o+3]=point[1];d[o+4]=point[2];
      d[o+5]=vx;d[o+6]=vy;d[o+7]=vz;d[o+8]=size;d[o+9]=endSize;
      d[o+10]=angle;d[o+11]=spin;d[o+12]=color[0];d[o+13]=color[1];d[o+14]=color[2];
      d[o+15]=gravity;d[o+16]=drag;d[o+17]=kind;
      d[o+18]=normal[0];d[o+19]=normal[1];d[o+20]=normal[2];this.owners[id]=owner;
    }
    remove(id){const last=--this.count;if(id!==last){this.data.copyWithin(id*STRIDE,last*STRIDE,(last+1)*STRIDE);this.owners[id]=this.owners[last];}this.owners[last]=null;}
    advance(dt,low){
      const limit=LIMITS[this.name][low?1:0];while(this.count>limit)this.remove(this.count-1);
      for(let i=0;i<this.count;){
        const o=i*STRIDE,d=this.data;d[o]-=dt;if(d[o]<=0){this.remove(i);continue;}
        const drag=Math.exp(-d[o+16]*dt);d[o+5]*=drag;d[o+6]=d[o+6]*drag-d[o+15]*dt;d[o+7]*=drag;
        d[o+2]+=d[o+5]*dt;d[o+3]+=d[o+6]*dt;d[o+4]+=d[o+7]*dt;d[o+10]+=d[o+11]*dt;
        if(this.name==='chips'&&d[o+3]<.012&&d[o+6]<0){d[o+3]=.012;d[o+6]*=-.18;d[o+5]*=.6;d[o+7]*=.6;}
        i++;
      }
    }
    reset(){this.count=0;this.cursor=0;this.owners.fill(null);if(this.mesh.isInstancedMesh)this.mesh.count=0;else this.mesh.geometry.instanceCount=0;}
    dispose(){this.mesh.parent?.remove(this.mesh);this.mesh.geometry.dispose();this.mesh.material.dispose();this.reset();}
  }

  class SpritePool extends Pool{
    constructor(name,scene,texture){
      super(name);const plane=new THREE.PlaneGeometry(1,1),g=new THREE.InstancedBufferGeometry();
      g.setIndex(plane.index);g.setAttribute('position',plane.attributes.position);g.setAttribute('uv',plane.attributes.uv);
      this.position=new Float32Array(this.capacity*3);this.size=new Float32Array(this.capacity*2);
      this.tint=new Float32Array(this.capacity*4);this.angle=new Float32Array(this.capacity);this.normal=new Float32Array(this.capacity*3);
      for(const [key,array,itemSize] of [['iPosition',this.position,3],['iSize',this.size,2],['iTint',this.tint,4],['iAngle',this.angle,1],['iNormal',this.normal,3]])
        g.setAttribute(key,new THREE.InstancedBufferAttribute(array,itemSize).setUsage(THREE.DynamicDrawUsage));
      g.instanceCount=0;
      const material=new THREE.ShaderMaterial({uniforms:{opacityMap:{value:texture}},defines:name==='marks'?{SURFACE_MARK:1}:{},
        vertexShader:spriteVertex,fragmentShader:spriteFragment,transparent:true,depthWrite:false,depthTest:true,
        blending:name==='flash'?THREE.AdditiveBlending:THREE.NormalBlending,side:THREE.DoubleSide,toneMapped:false});
      this.mesh=new THREE.Mesh(g,material);this.mesh.frustumCulled=false;this.mesh.renderOrder=name==='flash'?4:name==='dust'?3:1;scene.add(this.mesh);
    }
    upload(){
      this.mesh.geometry.instanceCount=this.count;if(!this.count)return;
      const d=this.data;
      for(let i=0;i<this.count;i++){
        const o=i*STRIDE,age=1-d[o]/d[o+1],size=d[o+8]+(d[o+9]-d[o+8])*age;
        let alpha;
        if(this.name==='flash')alpha=Math.pow(1-age,1.5);
        else if(this.name==='marks')alpha=Math.min(1,d[o]/3)*.82;
        else alpha=(.25+.75*Math.min(1,age*8))*Math.pow(1-age,1.25)*.68;
        this.position[i*3]=d[o+2];this.position[i*3+1]=d[o+3];this.position[i*3+2]=d[o+4];
        this.size[i*2]=size;this.size[i*2+1]=size*(this.name==='dust'?1.12:1);
        this.tint[i*4]=d[o+12];this.tint[i*4+1]=d[o+13];this.tint[i*4+2]=d[o+14];this.tint[i*4+3]=alpha;
        this.angle[i]=d[o+10];this.normal[i*3]=d[o+18];this.normal[i*3+1]=d[o+19];this.normal[i*3+2]=d[o+20];
      }
      for(const name of ['iPosition','iSize','iTint','iAngle','iNormal'])this.mesh.geometry.attributes[name].needsUpdate=true;
    }
  }

  class ChipPool extends Pool{
    constructor(scene){
      super('chips');const geometry=new THREE.TetrahedronGeometry(1);this.tint=new Float32Array(this.capacity*4);
      geometry.setAttribute('iTint',new THREE.InstancedBufferAttribute(this.tint,4).setUsage(THREE.DynamicDrawUsage));
      const material=new THREE.ShaderMaterial({vertexShader:chipVertex,fragmentShader:chipFragment,transparent:true,depthWrite:false,toneMapped:false});
      this.mesh=new THREE.InstancedMesh(geometry,material,this.capacity);this.mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
      this.mesh.frustumCulled=false;this.mesh.count=0;this.mesh.renderOrder=2;scene.add(this.mesh);
      this.matrix=new THREE.Matrix4();this.position=new THREE.Vector3();this.scale=new THREE.Vector3();
      this.rotation=new THREE.Quaternion();this.axis=new THREE.Vector3(0,1,0);this.velocity=new THREE.Vector3();
    }
    upload(){
      this.mesh.count=this.count;if(!this.count)return;const d=this.data;
      for(let i=0;i<this.count;i++){
        const o=i*STRIDE,fade=Math.min(1,d[o]/.35),size=d[o+8]*(.55+.45*fade),spark=d[o+17]>0;
        this.position.set(d[o+2],d[o+3],d[o+4]);this.scale.set(size,spark?size*5:size*.7,size);
        if(spark){this.velocity.set(d[o+5],d[o+6],d[o+7]);if(this.velocity.lengthSq()<1e-8)this.velocity.copy(this.axis);else this.velocity.normalize();this.rotation.setFromUnitVectors(this.axis,this.velocity);}
        else this.rotation.setFromAxisAngle(this.axis,d[o+10]);
        this.matrix.compose(this.position,this.rotation,this.scale);this.mesh.setMatrixAt(i,this.matrix);
        this.tint[i*4]=d[o+12];this.tint[i*4+1]=d[o+13];this.tint[i*4+2]=d[o+14];this.tint[i*4+3]=fade;
      }
      this.mesh.instanceMatrix.needsUpdate=true;this.mesh.geometry.attributes.iTint.needsUpdate=true;
    }
  }

  R.DestructionEffects=class{
    constructor(scene){
      this.texture=opacityTexture();this.flash=new SpritePool('flash',scene,this.texture);this.dust=new SpritePool('dust',scene,this.texture);
      this.chips=new ChipPool(scene);this.marks=new SpritePool('marks',scene,this.texture);
      this.pools=[this.flash,this.dust,this.chips,this.marks];this.sequence=0;this.fragmentSequence=0;this.disposed=false;
    }
    rng(event){return random((event.seed??++this.sequence)^0xa341316c);}
    impact(event,low=false){
      if(this.disposed||!event?.point)return;
      if(event.fragment&&++this.fragmentSequence%4!==0)return;
      const energy=Math.max(0,event.lost??event.energy??0);if(energy<.1)return;
      const palette=PALETTES[event.material]||PALETTES.concrete,rng=this.rng(event),normal=event.normal||[0,1,0],point=event.point,mode=event.materialMode||event.deformationProfile||'',travel=(event.result==='ricochet'||event.result==='glance')&&event.outgoingDirection?event.outgoingDirection:normal;
      const visualEnergy=Math.max(.1,event.damageEnergy??energy),strength=clamp(Math.sqrt(visualEnergy/2000),.18,2),diameter=Math.max(.003,event.diameter||.008),brittleMode=['masonry-break','concrete-scab','mineral-scab','shatter'].includes(mode),woodSplit=mode==='grain-split',petal=mode==='petal',plug=mode==='plug';
      const masonryBonus=Math.min(6,event.masonry?.newlyBroken||0),count=event.fragment?2:low?3:palette.metal?(petal?13:plug?6:9):7+(brittleMode?3:0)+(woodSplit?3:0)+masonryBonus;
      for(let i=0;i<count;i++){
        const speed=(2+rng()*5)*strength,n=palette.metal?1.8:1,grain=woodSplit&&event.grainDirection?event.grainDirection:null,grainSign=rng()<.5?-1:1,grainPush=grain?speed*.65*grainSign:0;
        this.chips.add(point,(rng()-.5)*speed+travel[0]*speed*n+(grain?.[0]||0)*grainPush,(rng()-.25)*speed+travel[1]*speed*n+(grain?.[1]||0)*grainPush,
          (rng()-.5)*speed+travel[2]*speed*n+(grain?.[2]||0)*grainPush,.25+rng()*(palette.metal?.45:woodSplit?1.45:1.05),
          palette.metal?.009+rng()*.01:clamp(diameter*(.5+rng()),.008,.07),0,
          palette.metal?[2.4,1.15,.25]:palette.chip,rng()*6.28,rng()*8,7,.7,palette.metal?1:0,normal,null,low);
      }
      if(!event.fragment&&!palette.metal){
        const dustCount=low?1:3+(brittleMode?2:0)+(woodSplit?1:0),dustSize=clamp(event.frontRadius||diameter*2,.03,.28);
        for(let i=0;i<dustCount;i++)this.dust.add(point,normal[0]*(.4+rng())+(rng()-.5),normal[1]*.5+.3+rng()*.5,
          normal[2]*(.4+rng())+(rng()-.5),.65+rng()*.8,Math.max(.05,dustSize*.35),Math.max(.16+strength*.24,dustSize*1.8),palette.dust,rng()*6.28,(rng()-.5)*.7,-.12,2,0,normal,null,low);
      }
      if(palette.metal&&!event.fragment)this.flash.add(point,0,0,0,.055,.04,.10+strength*.06,[2.5,1.25,.35],0,0,0,0,0,normal,null,low);
      if(!event.fragment&&(brittleMode||woodSplit)&&event.stressRadius){
        const {u,v}=surfaceBasis(normal),cracks=low?1:Math.min(5,2+Math.round(clamp(event.stressRadius/.12,0,3))),spread=clamp(event.stressRadius,.04,.55);
        for(let i=0;i<cracks;i++){const a=rng()*Math.PI*2,r=spread*(.28+rng()*.62),p=[point[0]+u[0]*Math.cos(a)*r+v[0]*Math.sin(a)*r+normal[0]*.008,point[1]+u[1]*Math.cos(a)*r+v[1]*Math.sin(a)*r+normal[1]*.008,point[2]+u[2]*Math.cos(a)*r+v[2]*Math.sin(a)*r+normal[2]*.008],size=clamp((event.frontRadius||diameter*2)*(.35+rng()*.45),.025,.14);this.marks.add(p,0,0,0,7+rng()*7,size,size,[.12,.11,.095],a,0,0,0,0,normal,event.part||null,low);}
      }
      if(!event.fragment&&event.result==='penetrated'&&event.exitPoint){
        const raw=event.outgoingDirection||[-normal[0],-normal[1],-normal[2]],len=Math.hypot(raw[0],raw[1],raw[2])||1,out=[raw[0]/len,raw[1]/len,raw[2]/len],exitSize=clamp(event.exitRadius||diameter*2,.012,.34),exitCount=low?2:palette.metal?5:8;
        for(let i=0;i<exitCount;i++){const speed=(2.5+rng()*6)*strength;this.chips.add(event.exitPoint,(rng()-.5)*speed+out[0]*speed*1.4,(rng()-.25)*speed+out[1]*speed*1.4,(rng()-.5)*speed+out[2]*speed*1.4,.35+rng()*.8,clamp(exitSize*(.14+rng()*.22),.007,.065),0,palette.chip,rng()*6.28,rng()*10,8,.65,palette.metal?1:0,out,null,low);}
        if(plug&&!low)this.chips.add(event.exitPoint,out[0]*strength*5,out[1]*strength*5+1,out[2]*strength*5,1.1,clamp(exitSize*.48,.018,.09),0,palette.chip,rng()*6.28,rng()*5,8,.55,0,out,null,low);
        if(petal&&!low)for(let i=0;i<3;i++){const speed=(5+rng()*6)*strength;this.chips.add(event.exitPoint,(rng()-.5)*speed+out[0]*speed*1.8,(rng()-.4)*speed+out[1]*speed*1.8,(rng()-.5)*speed+out[2]*speed*1.8,.32,.008+rng()*.008,0,[2.4,1.2,.3],rng()*6.28,rng()*12,7,.5,1,out,null,low);}
        if(!palette.metal)for(let i=0;i<(low?1:2);i++)this.dust.add(event.exitPoint,out[0]*(.8+rng())+(rng()-.5),out[1]*(.7+rng())+.2,out[2]*(.8+rng())+(rng()-.5),.55+rng()*.75,exitSize*.45,exitSize*(1.6+rng()),palette.dust,rng()*6.28,(rng()-.5),-.1,1.8,0,out,null,low);
      }
      if(!event.fragment&&event.result!=='penetrated'&&!event.geometryChanged){
        const shifted=[point[0]+normal[0]*.012,point[1]+normal[1]*.012,point[2]+normal[2]*.012];
        this.marks.add(shifted,0,0,0,12+rng()*8,clamp(diameter*3+.012,.03,.22),clamp(diameter*3+.012,.03,.22),
          [.10,.095,.085],rng()*6.28,0,0,0,0,normal,event.part||null,low);
      }
    }
    explosion(event,low=false){
      if(this.disposed||!event?.point)return;
      const rng=this.rng(event),palette=PALETTES[event.material]||PALETTES.concrete;
      const scale=clamp(Math.cbrt(Math.max(.005,event.explosiveMass||.04)),.22,1.5),radius=.45+scale*1.65;
      const normal=event.normal||[0,1,0],point=[event.point[0]+normal[0]*.06,event.point[1]+normal[1]*.06,event.point[2]+normal[2]*.06];
      this.flash.add(point,0,0,0,.12+scale*.035,radius*.35,radius*1.6,[3,2.7,1.6],0,0,0,0,0,normal,null,low);
      this.flash.add(point,normal[0],normal[1],normal[2],.22+scale*.05,radius*.55,radius*1.8,[2.8,.85,.12],rng()*6.28,0,0,0,0,normal,null,low);
      const dustCount=low?8:12+Math.floor(scale*12),chipCount=low?18:24+Math.floor(scale*28);
      for(let i=0;i<dustCount;i++){
        const angle=rng()*Math.PI*2,up=rng(),radial=Math.sqrt(1-up*up),speed=(.6+rng()*2)*radius;
        this.dust.add(point,Math.cos(angle)*radial*speed+normal[0],up*speed*.5+.35,Math.sin(angle)*radial*speed+normal[2],
          1.6+rng()*1.9+scale*.35,.10+radius*.10,radius*(.8+rng()*.85),palette.dust,rng()*6.28,(rng()-.5)*.4,-.16,1.3,0,normal,null,low);
      }
      for(let i=0;i<chipCount;i++){
        const angle=rng()*Math.PI*2,up=rng()*.9+.1,speed=(4+rng()*10)*(.6+scale*.6),radial=Math.sqrt(1-up*up),spark=i%5===0;
        this.chips.add(point,Math.cos(angle)*radial*speed+normal[0]*2,up*speed,Math.sin(angle)*radial*speed+normal[2]*2,
          spark?.4+rng()*.4:1.1+rng()*1.6,spark?.012+scale*.008:.022+rng()*.055*(.5+scale),0,
          spark?[2.4,1.2,.3]:palette.chip,rng()*6.28,(rng()-.5)*14,8,spark?.4:.8,spark?1:0,normal,null,low);
      }
    }
    // Compatibility for editor integrations upgraded separately.
    burst(point,explosion,low){if(explosion)this.explosion({point,explosiveMass:.12},low);else this.impact({point,lost:200,result:'stopped'},low);}
    validateMarks(model){
      if(!model)return;const pool=this.marks;
      // Cosmetic marks disappear immediately with their section. No raycasts,
      // reacquisition or delayed work that could leave a mark hanging in space.
      for(let id=0;id<pool.count;id++){
        const owner=pool.owners[id];if(!owner)continue;const current=model.parts.get(owner);
        if(!current||current.detached)pool.data[id*STRIDE]=0;
      }
    }
    update(dt,low=false,camera=null,model=null){
      if(this.disposed||!Number.isFinite(dt)||dt<0)return;
      dt=Math.min(dt,.1);this.validateMarks(model);
      for(const pool of this.pools){pool.advance(dt,low);pool.upload();}
    }
    reset(){for(const pool of this.pools)pool.reset();this.sequence=0;this.fragmentSequence=0;}
    dispose(){if(this.disposed)return;this.disposed=true;for(const pool of this.pools)pool.dispose();this.texture.dispose();}
    get diagnostics(){return {flash:this.flash.count,dust:this.dust.count,chips:this.chips.count,marks:this.marks.count,
      drawCalls:this.pools.reduce((n,p)=>n+(p.count>0?1:0),0),capacity:528};}
  };
})();
