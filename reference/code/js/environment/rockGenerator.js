(function(){
  'use strict';
  const R=window.RTS,clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  const rows=[
    ['pebble','Otoczak',.35,.65,.85,0x898681],['fieldstone','Kamień polny',1.1,.85,.45,0x838078],
    ['granite','Głaz granitowy',3,1,.25,0x898486],['basalt','Bazalt',2.5,1.25,.12,0x505654],
    ['limestone','Wapień',3.5,.85,.35,0xada891],['sandstone','Piaskowiec',3,1,.2,0xa28a68],
    ['slate','Łupek',2,.35,.08,0x68747b],['outcrop','Wychodnia skalna',6,1.15,.1,0x868579]
  ];
  const catalog={};for(const [id,name,size,height,roundness,color] of rows)catalog[id]=Object.freeze({id,name,size,height,roundness,color});
  R.RockCatalog=Object.freeze(catalog);
  const defaults={size:.5,elongation:.5,flattening:.5,roughness:.5,roundness:.5,layering:.5,moss:.25};
  R.RockGenome={version:'rocks-1.0.0',create(species,seed,overrides={}){
    if(!catalog[species])throw Error('Unknown rock type: '+species);
    const rng=new R.SeededRandom(Number(seed)>>>0),genes={};
    for(const key of Object.keys(defaults)){const random=.2+rng.next()*.6;genes[key]=clamp(Number.isFinite(overrides[key])?overrides[key]:random,0,1);}
    return {version:this.version,category:'rocks',species,seed:Number(seed)>>>0,genes};
  }};
  const cache=new Map();
  // A sparse icosahedral cage samples broad asymmetric masses and fracture
  // planes. All LODs share the same field, orientation and vertical anchor.
  function geometry(dna,detail){
    const s=catalog[dna.species],g=dna.genes,rng=new R.SeededRandom(dna.seed),phase=Array.from({length:6},()=>rng.next()*Math.PI*2);
    const size=s.size*(.55+g.size*.9),sx=size*(.65+g.elongation*.7),sy=size*s.height*(1.25-g.flattening*.65),sz=size*(1.2-g.elongation*.4);
    const round=clamp(s.roundness*.65+g.roundness*.35,0,1),power=.85+round*.15;
    const masses=Array.from({length:5},()=>{
      const y=rng.next()*2-1,angle=rng.next()*Math.PI*2,r=Math.sqrt(1-y*y);
      return {x:Math.cos(angle)*r,y,z:Math.sin(angle)*r,amount:(rng.next()-.48)*(.65+g.roughness*1.1)};
    });
    const fractures=Array.from({length:4},()=>{
      const y=rng.next()*2-1,angle=rng.next()*Math.PI*2,r=Math.sqrt(1-y*y);
      return {x:Math.cos(angle)*r,y,z:Math.sin(angle)*r,d:.64+rng.next()*.26};
    });
    const leanX=(rng.next()-.5)*.28,leanZ=(rng.next()-.5)*.28;
    const point=v=>{
      const length=Math.hypot(...v),x=v[0]/length,y=v[1]/length,z=v[2]/length;
      let mass=1;
      for(const lobe of masses){const proximity=Math.max(0,x*lobe.x+y*lobe.y+z*lobe.z);mass+=lobe.amount*proximity*proximity*proximity;}
      const layered=['slate','sandstone','limestone','outcrop'].includes(s.id)?g.layering:0;
      let radius=clamp(mass+layered*.06*Math.sin(y*7+x*1.4+phase[5]),.42,1.45);
      for(const plane of fractures){const dot=x*plane.x+y*plane.y+z*plane.z;if(dot>0){const cut=plane.d/dot;if(cut<radius)radius+=(cut-radius)*(1-round)*(.55+g.roughness*.45);}}
      const shape=n=>Math.sign(n)*Math.pow(Math.abs(n),power);
      return [(shape(x)*radius+leanX*y)*sx*.5,shape(y)*sy*.5*radius,(shape(z)*radius+leanZ*y)*sz*.5];
    };
    const phi=(1+Math.sqrt(5))/2,angle=phase[3],cos=Math.cos(angle),sin=Math.sin(angle);
    const vertices=[[-1,phi,0],[1,phi,0],[-1,-phi,0],[1,-phi,0],[0,-1,phi],[0,1,phi],[0,-1,-phi],[0,1,-phi],[phi,0,-1],[phi,0,1],[-phi,0,-1],[-phi,0,1]].map(([x,y,z])=>{const length=Math.hypot(x,y,z);return [(x*cos+z*sin)/length,y/length,(z*cos-x*sin)/length];});
    const topology=[[0,11,5],[0,5,1],[0,1,7],[0,7,10],[0,10,11],[1,5,9],[5,11,4],[11,10,2],[10,7,6],[7,1,8],[3,9,4],[3,4,2],[3,2,6],[3,6,8],[3,8,9],[4,9,5],[2,4,11],[6,2,10],[8,6,7],[9,8,1]];
    let faces=topology.map(face=>face.map(i=>vertices[i]));
    const levels=detail==='high'?2:detail==='world'?1:0;
    const midpoint=(a,b)=>{const v=a.map((n,i)=>n+b[i]),length=Math.hypot(...v);return v.map(n=>n/length);};
    for(let level=0;level<levels;level++){
      const next=[];for(const [a,b,c] of faces){const ab=midpoint(a,b),bc=midpoint(b,c),ca=midpoint(c,a);next.push([a,ab,ca],[ab,b,bc],[ca,bc,c],[ab,bc,ca]);}faces=next;
    }
    const positions=new Float32Array(faces.length*9),colors=new Float32Array(positions.length),normals=new Float32Array(positions.length);
    const base=new THREE.Color(s.color).convertSRGBToLinear(),moss=new THREE.Color(0x566444).convertSRGBToLinear();let minY=Infinity;
    for(let i=0;i<faces.length;i++){
      const p=faces[i].map(point),a=p[0],b=p[1],c=p[2],u=b.map((v,j)=>v-a[j]),v=c.map((n,j)=>n-a[j]);
      let nx=u[1]*v[2]-u[2]*v[1],ny=u[2]*v[0]-u[0]*v[2],nz=u[0]*v[1]-u[1]*v[0],length=Math.hypot(nx,ny,nz)||1;nx/=length;ny/=length;nz/=length;
      for(let j=0;j<3;j++){
        const q=p[j],k=i*9+j*3,band=Math.sin(q[1]/sy*35+phase[5]);
        const shade=.88+.10*Math.sin(q[0]*7+q[2]*5+phase[0])+g.layering*.06*band;
        const coverage=clamp((Math.sin(q[0]*2+phase[1])*Math.cos(q[2]*3+phase[2])+.4)*g.moss*(.3+.7*Math.max(0,ny)),0,.8);
        positions.set(q,k);normals.set([nx,ny,nz],k);minY=Math.min(minY,q[1]);
        colors[k]=(base.r*(1-coverage)+moss.r*coverage)*shade;colors[k+1]=(base.g*(1-coverage)+moss.g*coverage)*shade;colors[k+2]=(base.b*(1-coverage)+moss.b*coverage)*shade;
      }
    }
    // Anchor from the common cage, so switching detail never translates the rock.
    const bottom=Math.min(...vertices.map(v=>point(v)[1]));for(let i=1;i<positions.length;i+=3)positions[i]-=bottom;
    const geo=new THREE.BufferGeometry();geo.setAttribute('position',new THREE.BufferAttribute(positions,3));geo.setAttribute('normal',new THREE.BufferAttribute(normals,3));geo.setAttribute('color',new THREE.BufferAttribute(colors,3));geo.computeBoundingSphere();
    return geo;
  }
  R.RockGenerator={
    create(input,state={}){
      const genome=R.RockGenome.create(input.species,input.seed,input.genes),detail=['high','world','distant','far'].includes(state.detail)?state.detail:'high';
      const key=JSON.stringify([genome,detail]);let entry=cache.get(key);
      if(!entry){entry={refs:0,geometry:geometry(genome,detail),material:new THREE.MeshStandardMaterial({vertexColors:true,roughness:.96})};cache.set(key,entry);}entry.refs++;
      const root=new THREE.Group(),mesh=new THREE.Mesh(entry.geometry,entry.material);root.name=catalog[genome.species].name;mesh.name='rock';mesh.castShadow=true;mesh.receiveShadow=true;root.add(mesh);let disposed=false;
      return {root,genome,state:{detail},stats:{triangles:entry.geometry.attributes.position.count/3,drawCalls:1},dispose(){if(disposed)return;disposed=true;if(--entry.refs===0){entry.geometry.dispose();entry.material.dispose();cache.delete(key);}if(root.parent)root.parent.remove(root);}};
    },
    async createAsync(input,state,yieldFrame,check){if(check)check();await yieldFrame();if(check)check();return this.create(input,state);},
    async createLodsAsync(input,state,yieldFrame,check){const lods=[];try{for(const detail of ['world','distant','far'])lods.push(await this.createAsync(input,{...state,detail},yieldFrame,check));return lods;}catch(error){lods.forEach(l=>l.dispose());throw error;}},
    cacheSize(){return cache.size;}
  };
})();
