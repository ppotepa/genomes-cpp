(function(){
  'use strict';
  const R=globalThis.RTS,copy=v=>v?.slice(),distance=(a,b)=>Math.hypot(...a.map((x,i)=>x-b[i]));
  const angle=(a,b)=>Math.acos(Math.max(-1,Math.min(1,a.reduce((s,x,i)=>s+x*b[i],0)/(Math.hypot(...a)*Math.hypot(...b)||1))));
  function projection(p,a,b){const d=b.map((x,i)=>x-a[i]),dd=d.reduce((s,x)=>s+x*x,0);return Math.max(0,Math.min(1,p.reduce((s,x,i)=>s+(x-a[i])*d[i],0)/(dd||1)));}
  class ProjectileTraceRecorder{
    constructor(limit=10,options={}){this.limit=limit;this.maxSegmentLength=options.maxSegmentLength??2;this.maxChordError=options.maxChordError??.08;this.maxAngularChange=options.maxAngularChange??Math.PI/180;this.maxEnergyError=options.maxEnergyError??.005;this.traces=[];this.groups=[];this.byId=new Map();this.revision=0;this.disposed=false;}
    start(shot,p){if(this.disposed)return;const id=String(p.traceId),shotId=String(p.shotId);if(this.byId.has(id))return;
      if(!p.fragment)this.groups.unshift(shotId);else if(!this.groups.includes(shotId))return;
      const t={version:'projectile-trace-3',traceId:id,shotId,parentTraceId:p.parentTraceId??null,fragment:!!p.fragment,tick:shot.tick,ammo:shot.ammo.id,kind:shot.ammo.kind,weapon:shot.weapon?.id,initialEnergy:p.initialEnergy??.5*p.mass*p.v.reduce((s,x)=>s+x*x,0),segments:[],contacts:[],fragments:[],state:'flight',position:copy(p.p),direction:copy(p.v),age:p.age,travel:p.travel,compression:{originalSegmentCount:0,renderedSegmentCount:0,maxChordError:0,boundaryCount:0},revision:++this.revision};
      this.traces.push(t);this.byId.set(id,t);while(this.groups.length>this.limit){const removed=this.groups.pop();this.traces=this.traces.filter(x=>{if(x.shotId!==removed)return true;this.byId.delete(x.traceId);return false;});}
      this.traces.sort((a,b)=>this.groups.indexOf(a.shotId)-this.groups.indexOf(b.shotId)||Number(a.fragment)-Number(b.fragment));
    }
    segment(p,from,to,velocity,energy,energyFrom,material=false){const t=this.byId.get(String(p.traceId));if(!t||t.finished)return;
      energyFrom=Number.isFinite(energyFrom)?energyFrom:t.segments.at(-1)?.energyTo??t.initialEnergy;
      const q=t.segments.at(-1),s={from:copy(from),to:copy(to),speed:Math.hypot(...velocity),energyFrom,energyTo:energy,energy,toDirection:copy(velocity),material,samples:[{point:copy(from),energy:energyFrom},{point:copy(to),energy}],turn:0};t.compression.originalSegmentCount++;
      let merged=false;if(q&&!q.boundary&&q.material===material&&Math.abs(q.energyTo-energyFrom)<1e-6&&distance(q.to,from)<1e-5&&distance(q.from,to)<=this.maxSegmentLength){
        const turn=q.turn+angle(q.toDirection,velocity),samples=q.samples.concat(s.samples.slice(1));let chord=0,error=0;
        for(const x of samples){const u=projection(x.point,q.from,to),projected=q.from.map((v,i)=>v+(to[i]-v)*u);chord=Math.max(chord,distance(x.point,projected));error=Math.max(error,Math.abs(x.energy-(q.energyFrom+(energy-q.energyFrom)*u)));}
        if(turn<=this.maxAngularChange&&chord<=this.maxChordError&&error<=Math.max(.01,t.initialEnergy*this.maxEnergyError)){Object.assign(q,{to:copy(to),energyTo:energy,energy,speed:s.speed,toDirection:s.toDirection,samples,turn});merged=true;t.compression.maxChordError=Math.max(t.compression.maxChordError,chord);}
      }
      if(!merged)t.segments.push(s);t.compression.renderedSegmentCount=t.segments.length;Object.assign(t,{position:copy(to),direction:copy(velocity),currentEnergy:energy,age:p.age,travel:p.travel,rotationalEnergy:R.ProjectileState.rotationalEnergy(p),balanceError:R.ProjectileState.balanceError(p),energyLedger:{...p.energyLedger},revision:++this.revision});
    }
    boundary(p){const t=this.byId.get(String(p.traceId));if(t?.segments.length)t.segments.at(-1).boundary=true;}
    contact(p,e){const t=this.byId.get(String(p.traceId));if(!t||t.finished)return;this.boundary(p);t.currentEnergy=e.energy;t.contacts.push({...e,point:copy(e.point),normal:copy(e.normal),residualEnergy:e.energy,age:p.age,travel:p.travel});t.compression.boundaryCount++;t.state=e.result;t.revision=++this.revision;}
    fragment(p){const parent=this.byId.get(String(p.parentTraceId));if(!parent)return;parent.fragments.push(p.traceId);this.start({tick:parent.tick,ammo:{id:p.ammo.id,kind:parent.kind},weapon:{id:parent.weapon}},p);parent.revision=++this.revision;}
    blast(p,event){const t=this.byId.get(p.traceId);if(t){t.blast=event;t.revision=++this.revision;}}
    finish(id,state,point,direction,p){const t=this.byId.get(String(id));if(!t||t.finished)return;this.boundary({traceId:id});Object.assign(t,{finished:true,state,balanceError:p?R.ProjectileState.balanceError(p):t.balanceError,currentEnergy:p?R.ProjectileState.energy(p):t.currentEnergy,rotationalEnergy:p?R.ProjectileState.rotationalEnergy(p):t.rotationalEnergy,position:copy(point),direction:copy(direction),age:p?.age??t.age,travel:p?.travel??t.travel,energyLedger:{...p?.energyLedger},revision:++this.revision});}
    clear(){this.traces=[];this.groups=[];this.byId.clear();this.revision++;}dispose(){this.clear();this.disposed=true;}
  }
  const vertexShader=`
    attribute vec3 other;attribute float side;attribute float energyValue;
    uniform vec2 resolution;uniform float initialEnergy;uniform float absoluteScale;
    varying vec3 lineColor;
    void main(){
      float scale=absoluteScale>.5?log(1.+max(0.,energyValue))/log(5000001.):energyValue/max(.000001,initialEnergy);
      float t=clamp(scale,0.,1.);lineColor=t<.5?mix(vec3(.1,.9,.3),vec3(1.,.5,0.),t*2.):mix(vec3(1.,.5,0.),vec3(1.,.08,.03),(t-.5)*2.);
      vec4 a=projectionMatrix*modelViewMatrix*vec4(position,1.);vec4 b=projectionMatrix*modelViewMatrix*vec4(other,1.);
      // Clip the opposite endpoint to the near plane before screen extrusion.
      float nearA=a.z+a.w,nearB=b.z+b.w;if(nearA<0.&&nearB<0.){gl_Position=vec4(2.,2.,2.,1.);return;}if(nearA<0.)a=mix(a,b,-nearA/(nearB-nearA));else if(nearB<0.)b=mix(b,a,-nearB/(nearA-nearB));
      vec2 delta=(b.xy/max(.001,b.w)-a.xy/max(.001,a.w))*resolution;
      vec2 normal=length(delta)>.0001?normalize(vec2(-delta.y,delta.x)):vec2(0.,1.);
      a.xy+=normal*side*(1.5+4.5*t)/resolution*a.w;gl_Position=a;
    }`;
  class ProjectileTraceRenderer{
    constructor(T,scene){this.T=T;this.scene=scene;this.items=new Map();this.mode='10';this.fragments=false;this.heHistory=1;this.scale='relative';this.selected=null;this.resolution=new T.Vector2(1,1);}
    visibleTraces(traces){if(this.mode==='OFF')return [];const roots=traces.filter(t=>!t.fragment),latest=roots[0]?.shotId,he=new Set(roots.filter(t=>t.kind==='he').slice(0,this.heHistory).map(t=>t.shotId));return traces.filter(t=>(!t.fragment||this.fragments)&&(this.mode!=='last'||t.shotId===latest)&&(t.kind!=='he'||he.has(t.shotId)));}
    viewport(width,height){this.resolution.set(Math.max(1,width),Math.max(1,height));}
    remove(id){const o=this.items.get(id);if(!o)return;this.scene.remove(o.group);o.group.traverse(x=>{x.geometry?.dispose();x.material?.dispose();});this.items.delete(id);}
    update(traces){const T=this.T,visible=this.visibleTraces(traces),keep=new Set(visible.map(t=>t.traceId));for(const id of this.items.keys())if(!keep.has(id))this.remove(id);
      for(const t of visible){const key=t.revision+'/'+this.scale+'/'+this.selected;if(this.items.get(t.traceId)?.key===key)continue;this.remove(t.traceId);const positions=[],others=[],sides=[],energies=[];
        for(const s of t.segments){for(const [end,side] of [[0,-1],[0,1],[1,-1],[1,-1],[0,1],[1,1]]){positions.push(...(end?s.to:s.from));others.push(...(end?s.from:s.to));sides.push(end?-side:side);energies.push(end?s.energyTo:s.energyFrom);}}
        const geometry=new T.BufferGeometry();geometry.setAttribute('position',new T.Float32BufferAttribute(positions,3));geometry.setAttribute('other',new T.Float32BufferAttribute(others,3));geometry.setAttribute('side',new T.Float32BufferAttribute(sides,1));geometry.setAttribute('energyValue',new T.Float32BufferAttribute(energies,1));
        const material=new T.ShaderMaterial({uniforms:{resolution:{value:this.resolution},initialEnergy:{value:t.initialEnergy},absoluteScale:{value:this.scale==='absolute'?1:0}},vertexShader,fragmentShader:'varying vec3 lineColor;void main(){gl_FragColor=vec4(lineColor,1.);}',side:T.DoubleSide,depthWrite:false}),mesh=new T.Mesh(geometry,material),group=new T.Group();mesh.frustumCulled=false;group.add(mesh);
        // Shape encodes contact type; white symbols leave line colour exclusively to energy.
        for(const c of t.contacts){const kind=c.result,g=kind==='detonated'?new T.OctahedronGeometry(.10):kind==='ground-contact'?new T.RingGeometry(.08,.12,12):kind==='ricochet'||kind==='glance'?new T.ConeGeometry(.07,.16,4):kind==='penetrated'?new T.TorusGeometry(.07,.016,4,12):new T.BoxGeometry(.09,.09,.09),mark=new T.Mesh(g,new T.MeshBasicMaterial({color:0xe8f4ff,wireframe:kind==='detonated'}));mark.position.fromArray(c.point);if(kind==='ground-contact')mark.rotation.x=-Math.PI/2;group.add(mark);}
        this.scene.add(group);this.items.set(t.traceId,{group,key});
      }
    }
    dispose(){for(const id of [...this.items.keys()])this.remove(id);}
  }
  R.ProjectileTraceRecorder=ProjectileTraceRecorder;R.ProjectileTraceRenderer=ProjectileTraceRenderer;
})();
