(function(){
  'use strict';
  const R=window.RTS;
  const PANEL_LENGTH=2.4;
  function roundKey(point){return Math.round(point.x*1000)+':'+Math.round(point.z*1000);}
  function point(x,z){return {x,z};}
  function split(a,b,roads,parcel,parcels){
    const length=Math.hypot(b.x-a.x,b.z-a.z),count=Math.max(1,Math.ceil(length/PANEL_LENGTH)),panels=[];
    for(let i=0;i<count;i++){
      const from=point(a.x+(b.x-a.x)*i/count,a.z+(b.z-a.z)*i/count),to=point(a.x+(b.x-a.x)*(i+1)/count,a.z+(b.z-a.z)*(i+1)/count);
      const W=R.WorldGeneration;
      const clear=!roads.some(road=>road.points.some((end,j)=>j&&W.segmentsDistance(from,to,road.points[j-1],end)<road.width*.5+.12))
        &&!parcels.some(other=>other!==parcel&&other.polygon&&(W.inside((from.x+to.x)*.5,(from.z+to.z)*.5,other.polygon)||other.polygon.some((p,j)=>W.segmentsDistance(from,to,p,other.polygon[(j+1)%other.polygon.length])<.15)))
        &&!(parcel.buildingFootprint&&(W.inside((from.x+to.x)*.5,(from.z+to.z)*.5,parcel.buildingFootprint)||parcel.buildingFootprint.some((p,j)=>W.segmentsDistance(from,to,p,parcel.buildingFootprint[(j+1)%parcel.buildingFootprint.length])<.8)));
      if(clear)panels.push({a:from,b:to});
    }
    return panels;
  }
  // Geometry only: this can be tested without Three.js or a WebGL context.
  function plan(parcel,roads=[],parcels=[]){
    if(!parcel.fenced)return {panels:[],posts:[],gate:null};
    const polygon=parcel.polygon||R.WorldGeneration.rectangle(parcel.x,parcel.z,parcel.width,parcel.depth,parcel.rotation||0);
    const edge=parcel.frontEdge||0,a=polygon[edge],b=polygon[(edge+1)%polygon.length],length=Math.hypot(b.x-a.x,b.z-a.z),gap=Math.min(parcel.gate.width||4.2,length*.35),mid=point((a.x+b.x)*.5,(a.z+b.z)*.5);
    const left=point(mid.x-(b.x-a.x)*gap/(2*length),mid.z-(b.z-a.z)*gap/(2*length)),right=point(mid.x+(b.x-a.x)*gap/(2*length),mid.z+(b.z-a.z)*gap/(2*length));
    const spans=[[a,left],[right,b]];for(let i=0;i<polygon.length;i++)if(i!==edge)spans.push([polygon[i],polygon[(i+1)%polygon.length]]);
    const panels=spans.flatMap(([p,q])=>split(p,q,roads,parcel,parcels)),expected=spans.reduce((sum,[p,q])=>sum+Math.max(1,Math.ceil(Math.hypot(p.x-q.x,p.z-q.z)/PANEL_LENGTH)),0),unique=new Map();
    if(panels.length!==expected)return {panels:[],posts:[],gate:null};
    for(const panel of panels){unique.set(roundKey(panel.a),panel.a);unique.set(roundKey(panel.b),panel.b);}
    const gate={left,right,width:gap};
    // Gate posts are retained even if a road crosses an adjacent panel.
    if(!panels.length)return {panels:[],posts:[],gate:null};
    unique.set(roundKey(gate.left),gate.left);unique.set(roundKey(gate.right),gate.right);
    return {panels,posts:[...unique.values()],gate,type:parcel.fenceType};
  }
  function create(terrain,parcels,roads){
    const root=new THREE.Group();root.name='Parcel fences';
    const materials={wood:new THREE.MeshStandardMaterial({color:0x765237,roughness:1}),wire:new THREE.MeshStandardMaterial({color:0x68736e,metalness:.35,roughness:.72}),wall:new THREE.MeshStandardMaterial({color:0x91877a,roughness:1}),cap:new THREE.MeshStandardMaterial({color:0xafa596,roughness:1})};
    const boxes={wood:[],wire:[],wall:[],cap:[]},cube=new THREE.BoxGeometry(1,1,1),up=new THREE.Vector3(1,0,0),direction=new THREE.Vector3(),quaternion=new THREE.Quaternion();
    function box(type,x,y,z,sx,sy,sz,rotation=null){boxes[type].push({x,y,z,sx,sy,sz,rotation});}
    function beam(type,a,b,offsetA,offsetB,thickness,depth){
      const ay=terrain.getHeightAt(a.x,a.z)+offsetA,by=terrain.getHeightAt(b.x,b.z)+offsetB;
      direction.set(b.x-a.x,by-ay,b.z-a.z);const length=direction.length();if(length<.001)return;
      quaternion.setFromUnitVectors(up,direction.multiplyScalar(1/length));
      box(type,(a.x+b.x)*.5,(ay+by)*.5,(a.z+b.z)*.5,length,thickness,depth,quaternion.clone());
    }
    for(const parcel of parcels){
      const fence=plan(parcel,roads,parcels);if(!fence.gate)continue;
      const type=fence.type,height=type==='wire'?1.45:type==='wall'?1.12:1.3;
      for(const post of fence.posts){const gate=Math.hypot(post.x-fence.gate.left.x,post.z-fence.gate.left.z)<.001||Math.hypot(post.x-fence.gate.right.x,post.z-fence.gate.right.z)<.001,width=type==='wall'?.3:gate?.22:.14,postHeight=height+(gate?.18:0);box(type,post.x,terrain.getHeightAt(post.x,post.z)+postHeight*.5-.08,post.z,width,postHeight+.16,width);if(type==='wall')box('cap',post.x,terrain.getHeightAt(post.x,post.z)+postHeight+.04,post.z,width+.12,.1,width+.12);}
      for(const panel of fence.panels){
        const a=panel.a,b=panel.b,length=Math.hypot(b.x-a.x,b.z-a.z);
        if(type==='wood'){
          for(const y of [.38,1.03])beam('wood',a,b,y,y,.1,.09);
          const count=Math.max(1,Math.floor(length/.23));for(let i=0;i<count;i++){const t=(i+.5)/count,x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t;box('wood',x,terrain.getHeightAt(x,z)+.65,z,.12,1.3,.055,new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0,1,0),-Math.atan2(b.z-a.z,b.x-a.x)));}
        }else if(type==='wire'){
          for(const y of [.18,1.3])beam('wire',a,b,y,y,.045,.045);
          const verticals=Math.max(1,Math.round(length/.33));for(let i=1;i<verticals;i++){const t=i/verticals,x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t;box('wire',x,terrain.getHeightAt(x,z)+.75,z,.018,1.1,.018);}
          for(const y of [.42,.68,.94,1.18])beam('wire',a,b,y,y,.018,.018);
        }else{
          // Short sections follow the sampled ground at both ends; the cap
          // slopes with the same endpoints instead of floating over the wall.
          const steps=Math.max(1,Math.ceil(length/1.2));for(let i=0;i<steps;i++){const p=point(a.x+(b.x-a.x)*i/steps,a.z+(b.z-a.z)*i/steps),q=point(a.x+(b.x-a.x)*(i+1)/steps,a.z+(b.z-a.z)*(i+1)/steps);beam('wall',p,q,.48,.48,1.08,.22);beam('cap',p,q,1.05,1.05,.13,.33);}
        }
      }
    }
    const dummy=new THREE.Object3D(),identity=new THREE.Quaternion();for(const [type,items] of Object.entries(boxes)){if(!items.length)continue;const chunks=new Map();for(const item of items){const key=Math.floor(item.x/64)+','+Math.floor(item.z/64),list=chunks.get(key)||[];list.push(item);chunks.set(key,list);}for(const [key,chunk] of chunks){const mesh=new THREE.InstancedMesh(cube,materials[type],chunk.length);mesh.name='Fence '+type+' chunk '+key;mesh.castShadow=false;mesh.receiveShadow=true;mesh.frustumCulled=true;for(let i=0;i<chunk.length;i++){const item=chunk[i];dummy.position.set(item.x,item.y,item.z);dummy.quaternion.copy(item.rotation||identity);dummy.scale.set(item.sx,item.sy,item.sz);dummy.updateMatrix();mesh.setMatrixAt(i,dummy.matrix);}mesh.instanceMatrix.needsUpdate=true;root.add(mesh);}}
    if(!root.children.length)cube.dispose();
    for(const [type,material] of Object.entries(materials))if(!boxes[type].length)material.dispose();
    return root;
  }
  R.FenceGenerator={plan,create};
})();
