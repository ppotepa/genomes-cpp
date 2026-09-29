(function(){
  'use strict';
  const R=globalThis.RTS,S=R.DestructionSolid,V=S.V,D=R.DestructionSurfaceDamage,MaterialImpact=R.MaterialImpact,clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  function overlap(a,b,pad=0){return a.min.every((x,i)=>x<=b.max[i]+pad&&a.max[i]+pad>=b.min[i]);}
  function rayBounds(bounds,a,b,radius){let enter=0,exit=1;for(let i=0;i<3;i++){const d=b[i]-a[i],min=bounds.min[i]-radius,max=bounds.max[i]+radius;if(Math.abs(d)<1e-12){if(a[i]<min||a[i]>max)return false;continue;}let near=(min-a[i])/d,far=(max-a[i])/d;if(near>far){const swap=near;near=far;far=swap;}enter=Math.max(enter,near);exit=Math.min(exit,far);if(enter>exit)return false;}return true;}
  function connectionCenter(a,b){const p=[];for(let i=0;i<3;i++){const lo=Math.max(a.min[i],b.min[i]),hi=Math.min(a.max[i],b.max[i]);p[i]=lo<=hi?(lo+hi)/2:(a.max[i]<b.min[i]?(a.max[i]+b.min[i])/2:(b.max[i]+a.min[i])/2);}return p;}
  // Balanced AABB tree. Rebuilt only after geometry/support changes.
  function tree(parts){if(!parts.length)return null;const bounds={min:[Infinity,Infinity,Infinity],max:[-Infinity,-Infinity,-Infinity]};for(const p of parts){const b=p.queryBounds||p.bounds;for(let i=0;i<3;i++){bounds.min[i]=Math.min(bounds.min[i],b.min[i]);bounds.max[i]=Math.max(bounds.max[i],b.max[i]);}}if(parts.length<8)return {bounds,parts};const spans=V.sub(bounds.max,bounds.min),axis=spans.indexOf(Math.max(...spans));parts.sort((a,b)=>{const x=a.queryBounds||a.bounds,y=b.queryBounds||b.bounds;return x.min[axis]+x.max[axis]-y.min[axis]-y.max[axis];});const mid=parts.length>>1;return {bounds,left:tree(parts.slice(0,mid)),right:tree(parts.slice(mid))};}
  class MaterialModel{
    constructor(parts=[],seed=1){this.parts=new Map();this.rubbleField=null;this.seed=seed;this.revision=0;this.movingRevision=0;this.listeners=new Set();this.channels=[];this.maxParts=4096;this.maxChannels=8192;this.sweepDuration=1/60;this.movingParts=[];this.staticIndex=null;this.movingIndex=null;this.supportDependents=new Map();this._supportIndexDirty=true;this.metrics={geometryBudgetHits:0,channelsDropped:0,staticRebuilds:0,movingRebuilds:0,supportFailures:0,representationLimited:0,removedForBudget:0};for(const p of parts)this.add(p);this.reindex();}
    add(p){const part={material:'concrete',health:1,anchored:false,supports:[],velocity:[0,0,0],...p};part.supportIntegrity={...(p.supportIntegrity||{})};part.damageZones=(p.damageZones||[]).map(z=>({...z,point:z.point.slice()}));part.damageField={contract:'material-damage-field-1',cells:(p.damageField?.cells||[]).map(c=>({...c,point:c.point.slice()})),holes:(p.damageField?.holes||[]).map(h=>({...h,point:h.point.slice(),direction:h.direction.slice()}))};part.masonryCells=p.masonryCells instanceof Map?new Map(p.masonryCells):new Map(Object.entries(p.masonryCells||{}));part.masonryBroken=p.masonryBroken||0;part.grainDirection=Array.isArray(p.grainDirection)?p.grainDirection.slice():p.grainDirection;this._supportIndexDirty=true;part.faces=part.faces||S.box(part.min,part.max);part.planes=S.planes(part.faces);part.bounds=S.bounds(part.faces);part.volume=S.volume(part.faces);part.initialVolume=part.initialVolume||part.volume;part.damage=part.damage||0;this.parts.set(part.id,part);return part;}
    rebuildSupportIndex(){const map=new Map();for(const p of this.parts.values())for(const id of p.supports||[]){if(!map.has(id))map.set(id,[]);map.get(id).push(p.id);}this.supportDependents=map;this._supportIndexDirty=false;}
    ensureSupportIndex(){if(this._supportIndexDirty)this.rebuildSupportIndex();return this.supportDependents;}
    reindex(){if(this._supporting||this._batchDepth){this._reindexPending=true;return;}const staticParts=[],moving=[];this.movingParts=[];
      for(const p of this.parts.values()){if(!p.faces.length)continue;const dynamic=!!p.dynamic||p.velocity.some(v=>v!==0);if(dynamic){this.movingParts.push(p);p.queryBounds={min:p.bounds.min.map((x,i)=>x+Math.min(0,p.velocity[i]*this.sweepDuration)),max:p.bounds.max.map((x,i)=>x+Math.max(0,p.velocity[i]*this.sweepDuration))};moving.push(p);}else{p.queryBounds=p.bounds;staticParts.push(p);}}
      this.staticIndex=tree(staticParts);this.movingIndex=tree(moving);this.index=this.staticIndex;if(this._supportIndexDirty)this.rebuildSupportIndex();this.revision++;this.movingRevision++;this.metrics.staticRebuilds++;this.metrics.movingRebuilds++;
    }
    reindexMoving(){if(this._supporting||this._batchDepth){this._reindexPending=true;return;}const moving=[];this.movingParts=[];
      for(const p of this.parts.values()){if((!p.dynamic&&!p.velocity.some(v=>v!==0))||!p.faces.length)continue;this.movingParts.push(p);p.queryBounds={min:p.bounds.min.map((x,i)=>x+Math.min(0,p.velocity[i]*this.sweepDuration)),max:p.bounds.max.map((x,i)=>x+Math.max(0,p.velocity[i]*this.sweepDuration))};moving.push(p);}
      this.movingIndex=tree(moving);this.movingRevision++;this.metrics.movingRebuilds++;
    }
    // Geometry/support callbacks remain synchronous. Collision queries follow
    // the batch, when the spatial index is rebuilt once from the final parts.
    batch(work){this._batchDepth=(this._batchDepth||0)+1;try{return work();}finally{this._batchDepth--;if(!this._batchDepth&&this._reindexPending){this._reindexPending=false;this.reindex();}}}
    candidates(bounds){const out=[];function visit(n){if(!n||!overlap(n.bounds,bounds))return;if(n.parts)out.push(...n.parts);else{visit(n.left);visit(n.right);}}visit(this.staticIndex);visit(this.movingIndex);return out;}
    rayCandidates(a,b,radius){const out=[];function visit(n){if(!n||!rayBounds(n.bounds,a,b,radius))return;if(n.parts)out.push(...n.parts);else{visit(n.left);visit(n.right);}}visit(this.staticIndex);visit(this.movingIndex);return out;}
    trace(a,b,radius=0,dt=0,ignore){const bounds={min:a.map((x,i)=>Math.min(x,b[i])-radius),max:a.map((x,i)=>Math.max(x,b[i])+radius)};let best=null;
      // The BVH already covers linear motion for a complete 1/60 s tick,
      // including detached dynamic rubble. Longer external queries conservatively
      // add moving volumes; ordinary ballistic substeps need no all-parts scan.
      const candidates=new Set(this.rayCandidates(a,b,radius));if(dt>this.sweepDuration)for(const p of this.movingParts){const moved={min:p.bounds.min.map((x,i)=>x+Math.min(0,p.velocity[i]*dt)),max:p.bounds.max.map((x,i)=>x+Math.max(0,p.velocity[i]*dt))};if(overlap(moved,bounds))candidates.add(p);}
      for(const p of candidates){if(ignore&&ignore.has(p.id))continue;const moving=dt&&(p.velocity[0]||p.velocity[1]||p.velocity[2]),target=moving?V.sub(b,V.mul(p.velocity,dt)):b,hit=S.sweptSphere(p.faces,p.planes,a,target,radius);if(hit&&(!best||hit.enter<best.enter)){
        const points=[V.mix(a,b,hit.enter),V.mix(a,b,hit.exit)],inside=(c)=>points.every(point=>{const delta=V.sub(point,c.point),radial=V.sub(delta,V.mul(c.direction,V.dot(delta,c.direction))),along=V.dot(delta,c.direction);return along>=-.002&&along<=((c.depth??Infinity)+.002)&&V.length(radial)+radius<=(c.radius??c.diameter/2)+1e-7;}),channel=!p.dynamic&&((p.damageField?.holes||[]).some(inside)||this.channels.some(c=>c.part===(p.originalId||p.id)&&c.diameter/2>=radius&&inside(c)));if(!channel)best={...hit,part:p,point:points[0]};}}const rubble=this.rubbleField?.trace(a,b,radius);if(rubble&&!(ignore&&ignore.has(rubble.part.id))&&(!best||rubble.enter<best.enter))best=rubble;return best;
    }
    emit(event){for(const fn of this.listeners)fn(event);}
    targetVelocity(part,point){const linear=part?.linearVelocity||part?.velocity||[0,0,0],angular=part?.angularVelocity||[0,0,0],center=part?.centerOfMass||part?.center||[0,0,0],r=V.sub(point||center,center);return V.add(linear,V.cross(angular,r));}
    localWeakness(part,point){
      let weakness=0;
      if(part.masonryCells?.size&&MaterialImpact.dominantMaterial(part)==='brick'){const ext=V.sub(part.bounds.max,part.bounds.min),thin=ext.indexOf(Math.min(...ext)),axes=[0,1,2].filter(i=>i!==thin),vertical=axes.includes(1)?1:axes[1],horizontal=axes.find(i=>i!==vertical)??axes[0],u=Math.round(point[horizontal]/.28),v=Math.round(point[vertical]/.10);let local=0,weight=0;for(let du=-1;du<=1;du++)for(let dv=-1;dv<=1;dv++){const w=1/(1+Math.abs(du)+Math.abs(dv)),value=part.masonryCells.get((u+du)+':'+(v+dv))||0;local+=Math.min(1.5,value)*w;weight+=w;}weakness+=clamp(local/Math.max(1e-9,weight)*.28+(part.masonryBroken||0)*.004,0,.48);}
      for(const zone of part.damageZones||[]){const d=V.length(V.sub(point,zone.point));if(d>=zone.radius)continue;const fall=1-d/Math.max(1e-6,zone.radius);weakness+=zone.strength*fall*fall;}
      for(const cell of part.damageField?.cells||[]){const distance=V.length(V.sub(point,cell.point));if(distance<cell.radius)weakness+=cell.weakness*(1-distance/cell.radius)**2;}return clamp(weakness,0,.92);
    }
    recordDamageZone(part,point,radius,strength,kind='stress'){
      if(!part||!Number.isFinite(radius)||radius<=0||!Number.isFinite(strength)||strength<=0)return null;const zones=part.damageZones||(part.damageZones=[]),near=zones.find(z=>z.kind===kind&&V.length(V.sub(z.point,point))<Math.max(z.radius,radius)*.45);
      if(near){const total=near.strength+strength,weight=strength/Math.max(1e-9,total);near.point=V.mix(near.point,point,weight);near.radius=Math.max(near.radius,radius);near.strength=clamp(Math.max(near.strength,strength)+Math.min(near.strength,strength)*.25,0,.95);return near;}
      const zone={point:point.slice(),radius,strength:clamp(strength,0,.95),kind};if(zones.length>=16){let weakest=0;for(let i=1;i<zones.length;i++)if(zones[i].strength<zones[weakest].strength)weakest=i;if(zones[weakest].strength>=zone.strength)return zones[weakest];zones[weakest]=zone;}else zones.push(zone);return zone;
    }
    masonryDamage(part,point,budget){
      if(budget.damageMaterial!=='brick'||!part.buildingPanel||budget.collateral<=0)return null;
      const ext=V.sub(part.bounds.max,part.bounds.min),thin=ext.indexOf(Math.min(...ext)),axes=[0,1,2].filter(i=>i!==thin),vertical=axes.includes(1)?1:axes[1],horizontal=axes.find(i=>i!==vertical)??axes[0],cellU=.28,cellV=.10,centerU=Math.round(point[horizontal]/cellU),centerV=Math.round(point[vertical]/cellV),weights=[];let weightSum=0;
      const rangeU=Math.max(1,Math.min(2,Math.ceil((budget.stressRadius||cellU)/cellU))),rangeV=Math.max(1,Math.min(3,Math.ceil((budget.stressRadius||cellV)/cellV)));
      for(let du=-rangeU;du<=rangeU;du++)for(let dv=-rangeV;dv<=rangeV;dv++){const normalized=Math.hypot(du/Math.max(1,rangeU),dv/Math.max(1,rangeV)),w=1/(1+normalized*2.2);weights.push([du,dv,w]);weightSum+=w;}
      const thickness=Math.max(.04,ext[thin]),capacity=Math.max(90,(R.DestructionMaterials.brick.toughness||40000)*cellU*cellV*thickness*.5),available=budget.energies.front+budget.energies.radial+budget.energies.rear;let newlyBroken=0,damaged=0;
      for(const [du,dv,w] of weights){const key=(centerU+du)+':'+(centerV+dv),before=part.masonryCells.get(key)||0,after=before+available*(w/weightSum)/capacity;part.masonryCells.set(key,after);if(after>before)damaged++;if(before<1&&after>=1)newlyBroken++;}
      part.masonryBroken+=newlyBroken;if(newlyBroken)part.damage+=newlyBroken*.012;
      const state={damagedCells:damaged,newlyBroken,totalBroken:part.masonryBroken,weakness:clamp(part.masonryBroken*.018,0,.35)};this.emit({type:'masonry-damage',part,point:point.slice(),radius:budget.stressRadius,...state});return state;
    }
    stressZone(part,point,direction,budget,diameter,fragment=false){
      if(!part?.buildingPanel||part.detached||budget.energies.radial<=0||budget.stressRadius<=0)return {affected:0,detached:0};
      this.recordDamageZone(part,point,budget.stressRadius,budget.zoneStrength,budget.mode);
      const radius=budget.stressRadius,bounds={min:point.map(x=>x-radius),max:point.map(x=>x+radius)},neighbors=this.candidates(bounds),toDetach=[],eligible=[];let affected=0,weightSum=0;
      for(const q of neighbors){if(q===part||!this.parts.has(q.id)||!q.buildingPanel||q.detached||q.indestructible)continue;if(part.wallId&&q.wallId&&part.wallId!==q.wallId&&part.sourceId!==q.sourceId)continue;const d=MaterialImpact.closestBoundsDistance(q.bounds,point);if(d>=radius)continue;const weight=(1-d/radius)**2;if(weight>0){eligible.push([q,weight]);weightSum+=weight;}}
      let allocated=0;this.batch(()=>{for(const [q,weight] of eligible){const stress=budget.energies.radial*weight/Math.max(1e-9,weightSum);allocated+=stress;const mat=R.DestructionMaterials[MaterialImpact.dominantMaterial(q)]||R.DestructionMaterials[q.material],capacity=mat.toughness*(q.strengthScale||1)*Math.max(.002,q.initialVolume);q.damage+=stress/capacity*.7;this.recordDamageZone(q,point,radius*.55,budget.zoneStrength*(weight**.5)*.7,budget.mode);this.weakenConnections(q,stress*.45,point,diameter,fragment);this.emit({type:'stress-damage',part:q,source:part,point:point.slice(),energy:stress,radius,damage:q.damage});affected++;if(q.damage>=1&&!q.anchored)toDetach.push([q,stress]);}for(const [q,stress] of toDetach)if(this.parts.has(q.id)&&!q.detached)this.detach(q,point,direction,stress);});
      return {affected,detached:toDetach.length,allocatedEnergy:allocated,normalized:weightSum>0};
    }
    localizedCut(facesList,cutter,maxPieces,key){
      if(!facesList.length)return facesList;const rng=S.stream(this.seed,key),start=Math.floor(rng()*facesList.length);
      for(let attempt=0;attempt<facesList.length;attempt++){const index=(start+attempt)%facesList.length,before=S.volume(facesList[index]),split=S.subtract(facesList[index],cutter.planes).filter(f=>S.volume(f)>1e-8),after=split.reduce((sum,f)=>sum+S.volume(f),0);if(!split.length||after>=before-1e-8)continue;const next=facesList.slice();next.splice(index,1,...split);if(next.length<=maxPieces)return next;}
      return facesList;
    }
    localizedErode(facesList,cutter,key){
      if(!facesList.length)return {faces:facesList,changed:false};const rng=S.stream(this.seed,key),start=Math.floor(rng()*facesList.length);
      for(let attempt=0;attempt<facesList.length;attempt++){const index=(start+attempt)%facesList.length,before=S.volume(facesList[index]),split=S.subtract(facesList[index],cutter.planes).filter(f=>S.volume(f)>1e-8);if(!split.length)continue;let survivor=split[0],volume=S.volume(survivor);for(let i=1;i<split.length;i++){const v=S.volume(split[i]);if(v>volume){survivor=split[i];volume=v;}}if(volume>=before-1e-8)continue;let removed=facesList[index];for(const plane of cutter.planes)removed=S.clip(removed,plane.n,plane.d);if(!removed.length||S.volume(removed)<1e-8)removed=null;const next=facesList.slice();next[index]=survivor;return {faces:next,changed:true,removed};}
      return {faces:facesList,changed:false,removed:null};
    }
    weakenConnections(part,energy,point,diameter,fragment=false){
      if(!part?.structuralGraph||!Number.isFinite(energy)||energy<=0)return false;this.ensureSupportIndex();let broken=false;
      const links=[];for(const id of part.supports||[]){const provider=this.parts.get(id);if(provider)links.push([part,provider]);}
      for(const id of this.supportDependents.get(part.id)||[]){const child=this.parts.get(id);if(child&&child!==part&&child.structuralGraph)links.push([child,part]);}
      const seen=new Set(),influence=Math.max(.28,(diameter||.01)*(fragment?5:14));
      for(const [child,provider] of links){const key=child.id+'>'+provider.id;if(seen.has(key))continue;seen.add(key);const c=connectionCenter(child.bounds,provider.bounds),distance=V.length(V.sub(point,c)),proximity=Math.max(0,1-distance/influence);if(proximity<=0)continue;
        const material=R.DestructionMaterials[child.material]||R.DestructionMaterials.concrete,capacity=Math.max(1800,material.toughness*(child.strengthScale||1)*.16),delta=energy*(fragment?.12:1)*proximity/capacity,previous=child.supportIntegrity[provider.id]??1,next=Math.max(0,previous-delta);child.supportIntegrity[provider.id]=next;
        if(next<previous)this.emit({type:'support-damage',part:child,support:provider,point:c,previous,integrity:next,energy:energy*proximity});
        if(previous>.02&&next<=.02){broken=true;this.metrics.supportFailures++;this.emit({type:'support-break',part:child,support:provider,point:c,energy:energy*proximity});}
      }
      if(broken)this.support();return broken;
    }
    damage(part,energy,point,direction,diameter,penetrated=false,penetrationDepth=0,fragment=false,context={}){
      if(context.blast&&part)part.preferHeroDebris=true;
      if(part?.rubble&&this.rubbleField){const result=this.rubbleField.damageAt(part,energy,point,direction,diameter,context);this.emit({...result,type:'impact',part});return result;}
      if(!this.parts.has(part.id)||part.indestructible||(part.detached&&!part.dynamic))return;
      const surfaceMat=R.DestructionMaterials[part.material]||R.DestructionMaterials.concrete,damageMaterial=MaterialImpact.dominantMaterial(part),damageMat=R.DestructionMaterials[damageMaterial]||surfaceMat,
        blastBudget=context.blast?{damageMaterial,mode:'blast',collateral:energy,structuralEnergy:energy,frontRadius:diameter,stressRadius:Math.max(diameter*4,.25),rearRadius:0,channelRadius:diameter,frontDepth:0,rearDepth:0,zoneStrength:.65,energies:{front:0,crush:energy*.28,radial:energy*.62,rear:0,plastic:0,ejecta:energy*.10}}:null,
        budget=context.damageBudget||blastBudget||MaterialImpact.budget({part,ammoKind:context.ammoKind||'ball',lost:energy,entryEnergy:energy,residualEnergy:0,diameter,penetrated,fragment}),
        structuralEnergy=Math.max(0,budget.structuralEnergy??energy),capacity=damageMat.toughness*(part.strengthScale||1)*Math.max(.002,part.initialVolume),
        result={resolvedImpulse:!!context.resolvedImpulse,material:part.material,damageMaterial,materialMode:budget.mode,point,direction,normal:context.normal||V.mul(V.unit(direction),-1),diameter,energy,lostEnergy:energy,damageEnergy:structuralEnergy,damageBudget:{collateral:budget.collateral??energy,front:budget.energies?.front||0,crush:budget.energies?.crush||0,radial:budget.energies?.radial||0,rear:budget.energies?.rear||0,plastic:budget.energies?.plastic||0,ejecta:budget.energies?.ejecta||0,allocated:budget.allocatedEnergy??Math.min(energy,Object.values(budget.energies||{}).reduce((s,v)=>s+Math.max(0,v),0)),conserved:budget.energyConserved??true},frontRadius:budget.frontRadius||diameter,stressRadius:budget.stressRadius||0,rearRadius:budget.rearRadius||0,fragment,ammoKind:context.ammoKind,ammoId:context.ammoId,seed:context.seed,exitPoint:context.exitPoint?.slice?.()||null,outgoingDirection:context.outgoingDirection?.slice?.()||null,response:'mark',geometryChanged:false},
        finish=()=>{result.logicalDamage=part.damage;result.damageScore=budget.damageScore??result.damageEnergy;result.geometryWork=budget.geometryWork??0;result.supportWork=budget.supportWork??0;result.representationLimited=this.metrics.geometryBudgetHits>0;result.geometryBudgetHits=this.metrics.geometryBudgetHits;result.removedForBudget=this.metrics.removedForBudget;this.emit({...result,type:'impact',part});return result;};

      part.damage+=structuralEnergy/capacity;const field=part.damageField||(part.damageField={contract:'material-damage-field-1',cells:[],holes:[]}),radius=Math.max(.001,budget.frontRadius||diameter),cell={point:point.slice(),radius,crush:budget.energies?.crush||0,cracks:budget.energies?.radial||0,weakness:Math.min(.95,structuralEnergy/capacity),rearDamage:budget.energies?.rear||0,removedVolume:0};field.cells.push(cell);if(field.cells.length>64)field.cells.shift();if(penetrated){const hole={point:point.slice(),direction:V.unit(direction),radius:Math.max(.001,budget.channelRadius||diameter),depth:Math.max(.001,penetrationDepth),removedVolume:Math.PI*Math.max(.001,budget.channelRadius||diameter)**2*Math.max(.001,penetrationDepth)};field.holes.push(hole);if(field.holes.length>64)field.holes.shift();cell.removedVolume=hole.removedVolume;}result.localDamage={after:cell,removedVolume:cell.removedVolume,rearDamage:cell.rearDamage};
      const connectionEnergy=(budget.energies?.radial||0)+(budget.energies?.rear||0)+(budget.energies?.crush||0)*.45;
      if(part.buildingPanel&&!part.detached&&connectionEnergy>0)this.weakenConnections(part,connectionEnergy,point,diameter,fragment);
      const masonry=this.masonryDamage(part,point,budget);if(masonry)result.masonry=masonry;
      const stress=this.stressZone(part,point,direction,budget,diameter,fragment);if(stress.affected)result.stress=stress;

      if(part.material==='tissue'||part.material==='armor'){if(part.damage>=1)this.emit({type:'ragdoll',part,energy:structuralEnergy,point,direction});return finish();}
      // Building rubble has one physical lifetime. It never creates another
      // generation of rigid bodies; sufficient further work crushes it away.
      if(part.buildingPanel&&part.terminalDebris){if(part.damage>=1){this.remove(part);result.response='crushed';result.geometryChanged=true;}return finish();}
      if(penetrated){const source=part.originalId||part.id,perPart=this.channels.reduce((n,c)=>n+(c.part===source),0);if(this.channels.length<this.maxChannels&&perPart<64)this.channels.push({part:source,point:point.slice(),direction:direction.slice(),diameter:diameter*2,exitPoint:context.exitPoint?.slice?.()||null,outgoingDirection:context.outgoingDirection?.slice?.()||direction.slice()});else this.metrics.channelsDropped++;}
      if(((part.damage>=1||part.material==='rock'&&structuralEnergy>1e4)&&!part.buildingPanel)||(part.material==='glass'&&!part.buildingPanel)){
        if(part.category==='plant'){this.detachBranch(part,point,direction,structuralEnergy);result.response='splinter';result.geometryChanged=true;return finish();}
        this.fracture(part,point,direction,structuralEnergy);result.response='fracture';result.geometryChanged=!this.parts.has(part.id)||part.detached;return finish();
      }
      if(part.category==='plant')return finish();
      if(fragment&&!context.blast)return finish();
      if(context.blast&&structuralEnergy>0){
        const axis=V.unit(direction),span=V.length(V.sub(part.bounds.max,part.bounds.min))+1,start=V.sub(point,V.mul(axis,.0001)),through=S.ray(part.planes,start,V.add(start,V.mul(axis,span)));
        const thickness=through?(through.exit-through.enter)*span:Infinity,radius=Math.min(.6,Math.cbrt(structuralEnergy/Math.max(1,damageMat.toughness)*.12));
        if(radius>thickness&&radius>.015){
          const cutter=D.bore(point,axis,radius,thickness+.002,damageMaterial,this.seed,part.id+'/blast');this.replace(part,S.subtract(part.faces,cutter));
          result.geometryChanged=!this.parts.has(part.id);result.response=result.geometryChanged?'blast-breach':'blast-damage';result.radius=radius;result.depth=thickness;this.support();return finish();
        }
      }
 // Fragments accumulate stress/damage but do not recursively remesh solids.

      const normalFactor=Math.min(1,V.dot(V.unit(direction),result.normal)**2),normalWork=((budget.energies?.plastic||0)+(budget.energies?.front||0)+(budget.energies?.crush||0))*normalFactor,
        surfaceYield=surfaceMat.strength*Math.PI*diameter**3/4,grain=MaterialImpact.grainAxis(part,direction),
        surface=!penetrated&&(surfaceMat.response==='ductile'||surfaceMat.response==='fibrous')&&normalWork>=surfaceYield?this.surfaceShape(part,point,direction,result.normal,diameter,normalWork,penetrationDepth):null,
        brittleEnergy=(budget.energies?.front||0)+(budget.energies?.radial||0),cutsGeometry=penetrated||!!surface||damageMat.response==='brittle'&&brittleEnergy>=damageMat.spallThreshold,shallow=surface&&penetrationDepth<=diameter;

      if(part.buildingPanel&&cutsGeometry&&(part.cutDepth||0)>=(part.maxCutDepth??2)){this.metrics.geometryBudgetHits++;this.metrics.representationLimited++;this.detach(part,point,direction,0);result.response='local-field';return finish();}
      if(part.buildingPanel&&cutsGeometry&&this.parts.size>=this.maxParts){this.metrics.geometryBudgetHits++;this.metrics.representationLimited++;this.detach(part,point,direction,0);result.response='local-field';return finish();}

      if(!penetrated&&damageMat.response==='brittle'&&brittleEnergy>=damageMat.spallThreshold&&this.parts.size<this.maxParts){
        const shape=this.spall(part,brittleEnergy,point,direction,Math.max(diameter,budget.frontRadius||diameter),penetrationDepth);Object.assign(result,shape||{});result.response='crater';result.radius=Math.max(result.radius||0,budget.frontRadius||diameter);result.geometryChanged=!this.parts.has(part.id)||part.detached;return finish();
      }

      if(surface){
        const profile=D.crater(point,surface.axis,surface.radius,surface.depth,part.material,this.seed,part.id+'/surface',Infinity,{preferred:grain}),pieces=this.replace(part,S.subtract(part.faces,profile.planes));for(const p of pieces)if(p.buildingPanel)p.cutDepth=(part.cutDepth||0)+1;
        Object.assign(result,{response:surfaceMat.response==='ductile'?'dent':'splinter',radius:Math.max(surface.radius,budget.frontRadius||0),depth:surface.depth,deformationProfile:profile.profile,geometryChanged:!this.parts.has(part.id)});if(result.geometryChanged&&part.buildingPanel)this.support();return finish();
      }

      if(penetrated&&diameter>=.005){
        if(this.parts.size>=this.maxParts){this.metrics.geometryBudgetHits++;this.metrics.representationLimited++;result.response='virtual-hole';return finish();}
        let exitScale=budget.exitScale||1;if(budget.mode==='plug')exitScale=Math.min(exitScale,1.12);else if(budget.mode==='petal')exitScale=Math.max(exitScale,1.28);else if(budget.mode==='grain-split')exitScale=Math.max(exitScale,1.15);
        const options={exitScale,preferred:grain,mode:budget.mode,profile:budget.mode};
        if(budget.mode==='grain-split'){options.stretchU=2.0;options.stretchV=.62;options.irregular=.10;}
        if(budget.mode==='petal'){options.exitScale=Math.min(1.65,exitScale);options.irregular=.07;}
        const r=Math.max(budget.channelRadius||diameter*1.08,.003),frontRadius=Math.max(r,budget.frontRadius||r),rearRadius=Math.max(r,budget.rearRadius||r),pieceBudget=part.buildingPanel?8:12,channelOptions={...options,exitScale:budget.mode==='plug'?1.06:budget.mode==='petal'?1.16:budget.mode==='grain-split'?1.10:1.12},perforation=D.perforation(point,direction,r,penetrationDepth,damageMaterial,this.seed,part.id+'/perforation',channelOptions);
        let candidate=S.subtract(part.faces,perforation.planes).filter(f=>S.volume(f)>1e-8),frontChip=false,rearChip=false;
        const brittle=['masonry-break','concrete-scab','mineral-scab','shatter'].includes(budget.mode),frontDepth=Math.min(penetrationDepth*.28,Math.max(diameter,budget.frontDepth||diameter)),rearDepth=Math.min(penetrationDepth*.26,Math.max(diameter,budget.rearDepth||diameter));
        if(candidate.length&&frontRadius>r*1.18&&(brittle||budget.mode==='grain-split')){const cutter=D.frustum(point,direction,frontRadius,r,frontDepth,damageMaterial,this.seed,part.id+'/front-chip',{...options,sides:3,exitScale:1});if(part.buildingPanel){const eroded=this.localizedErode(candidate,cutter,part.id+'/front-pick');candidate=eroded.faces;frontChip=eroded.changed;}else{const before=candidate.length;candidate=this.localizedCut(candidate,cutter,pieceBudget,part.id+'/front-pick');frontChip=candidate.length!==before;}}
        if(candidate.length&&context.exitPoint&&rearRadius>r*1.18&&(brittle||budget.mode==='grain-split'||budget.mode==='plug'||budget.mode==='petal')){const back=V.mul(V.unit(direction),-1),cutter=D.frustum(context.exitPoint,back,rearRadius,r,rearDepth,damageMaterial,this.seed,part.id+'/rear-chip',{...options,sides:3,exitScale:1});if(part.buildingPanel){const eroded=this.localizedErode(candidate,cutter,part.id+'/rear-pick');candidate=eroded.faces;rearChip=eroded.changed;}else{const before=candidate.length;candidate=this.localizedCut(candidate,cutter,pieceBudget,part.id+'/rear-pick');rearChip=candidate.length!==before;}}
        let maxEject=0;if(part.buildingPanel){if(budget.mode==='masonry-break')maxEject=Math.min(2,1+(masonry?.newlyBroken>=3?1:0));else if(['concrete-scab','mineral-scab'].includes(budget.mode)&&budget.severity>.55)maxEject=1;else if(['grain-split','plug','petal'].includes(budget.mode))maxEject=1;}
        const pieces=this.replace(part,candidate);for(const p of pieces)if(p.buildingPanel)p.cutDepth=(part.cutDepth||0)+1;if(part.buildingPanel&&structuralEnergy>1e5)for(const p of pieces)if(MaterialImpact.closestBoundsDistance(p.bounds,point)<=Math.max(frontRadius,r)*.92)this.remove(p);
        const chipRadius=Math.max(frontRadius,rearRadius,r)*2.4,exitPoint=context.exitPoint||V.add(point,V.mul(V.unit(direction),penetrationDepth)),debris=pieces.filter(p=>{const volume=S.volume(p.faces);if(volume>part.volume*.10)return false;return Math.min(MaterialImpact.closestBoundsDistance(p.bounds,point),MaterialImpact.closestBoundsDistance(p.bounds,exitPoint))<=chipRadius;}).sort((a,b)=>S.volume(a.faces)-S.volume(b.faces)).slice(0,maxEject);
        if(debris.length)this.batch(()=>{const ejectEnergy=Math.max(50,budget.energies?.ejecta||structuralEnergy*.08)/debris.length;for(let i=0;i<debris.length;i++){const p=debris[i];p.damage=0;p.detached=false;p.dynamic=false;p.physicsStarted=false;const side=rearChip&&i===0?1:-1,impulseDir=V.mul(V.unit(direction),side);this.detach(p,S.centroid(p.faces),impulseDir,ejectEnergy);}});
        if(part.buildingPanel)this.support();
        Object.assign(result,{response:'perforation',radius:frontRadius,frontRadius,channelRadius:r,depth:penetrationDepth,frontDepth,rearDepth,deformationProfile:perforation.profile,channelExitRadius:perforation.exitRadius,exitRadius:rearRadius,exitProfile:perforation.profile,materialMode:budget.mode,frontChip,rearChip,geometryChanged:!this.parts.has(part.id)||part.detached});
      }
      return finish();
    }
    surfaceShape(part,point,direction,normal,diameter,work,penetrationDepth){const mat=R.DestructionMaterials[part.material]||R.DestructionMaterials.concrete,axis=penetrationDepth>diameter?V.unit(direction):V.mul(normal,-1),length=V.length(V.sub(part.bounds.max,part.bounds.min))+1,start=V.add(point,V.mul(axis,-.00001)),through=S.ray(part.planes,start,V.add(start,V.mul(axis,length)));if(!through)return null;const thickness=(through.exit-through.enter)*length,radius=Math.max(diameter*.75,Math.min(.15,diameter*2.5,Math.cbrt(work/(6*mat.strength)))),plasticDepth=Math.min(diameter*(mat.response==='fibrous'?2:.6),work/(mat.strength*4*radius*radius)),depth=Math.min(thickness*.85,Math.max(plasticDepth,penetrationDepth));return depth>=.0002?{axis,radius,depth}:null;}
    craterPlanes(point,direction,radius,depth,material='concrete',key='crater'){return D.crater(point,direction,radius,depth,material,this.seed,key).planes;}
    borePlanes(point,direction,radius,depth,material='concrete',key='bore'){return D.bore(point,direction,radius,depth,material,this.seed,key);}
    replace(part,faces){const valid=faces.filter(f=>S.volume(f)>1e-8);if(!valid.length){this.remove(part);return [];}if(this.parts.size-1+valid.length>this.maxParts){this.metrics.geometryBudgetHits++;return [];}this.parts.delete(part.id);const pieces=valid.map((f,i)=>this.add({...part,id:part.id+'.'+i,faces:f,initialVolume:S.volume(f),damage:part.damage,originalId:part.originalId||part.id,anchored:part.anchored&&S.bounds(f).min[1]<=.03}));
      for(const p of this.parts.values())if(p.supports.includes(part.id)){const oldIntegrity=p.supportIntegrity?.[part.id]??1,newSupports=pieces.filter(q=>overlap(p.bounds,q.bounds,.12));p.supports=p.supports.flatMap(id=>id===part.id?newSupports.map(q=>q.id):[id]);delete p.supportIntegrity[part.id];for(const q of newSupports)p.supportIntegrity[q.id]=oldIntegrity;}
      for(const p of pieces)if(p.structuralGraph){p.supports=[...new Set([...p.supports,...pieces.filter(q=>q!==p).map(q=>q.id)])].filter(id=>{const q=this.parts.get(id);return q&&overlap(p.bounds,q.bounds,.12);});for(const id of p.supports)if(p.supportIntegrity[id]===undefined)p.supportIntegrity[id]=1;}
      for(const p of pieces)if(part.blastImpulse)p.blastImpulse=V.mul(part.blastImpulse,p.volume/Math.max(1e-9,part.volume));
      this._supportIndexDirty=true;this.reindex();this.emit({type:'replace',part,pieces});return pieces;
    }
    remove(part){if(!this.parts.has(part.id))return false;this.parts.delete(part.id);for(const p of this.parts.values())if(p.supports.includes(part.id)){p.supports=p.supports.filter(id=>id!==part.id);if(p.supportIntegrity)delete p.supportIntegrity[part.id];}this._supportIndexDirty=true;const source=part.originalId||part.id;if(![...this.parts.values()].some(p=>(p.originalId||p.id)===source))this.channels=this.channels.filter(c=>c.part!==source);this.reindex();this.emit({type:'remove',part});this.support();return true;}
    fracture(part,point,direction,energy){return this.batch(()=>{if(part.buildingPanel&&part.terminalDebris){this.remove(part);return;}const pieces=this.replace(part,S.splitThree(part.faces,this.seed,part.id,part.layering||0));if(!pieces.length){if(part.buildingPanel&&this.parts.has(part.id)){this.detach(part,point,direction,energy);this.support();}return;}
      for(const p of pieces){p.damage=0;p.detached=false;p.dynamic=false;p.physicsStarted=false;this.detach(p,point,direction,energy/pieces.length);}this.support();
    });}
    spall(part,energy,point,direction,diameter,penetrationDepth=0){const mat=R.DestructionMaterials[part.material]||R.DestructionMaterials.concrete,radius=Math.max(diameter,Math.min(.3,.35*Math.cbrt(energy/mat.toughness))),depth=radius*.45,axis=V.unit(direction),u=V.unit(V.cross(axis,Math.abs(axis[1])<.8?[0,1,0]:[1,0,0])),v=V.cross(axis,u),cutter=[...([u,V.mul(u,-1),v,V.mul(v,-1)].map(n=>({n,d:V.dot(n,point)+radius}))),{n:axis,d:V.dot(axis,point)+depth},{n:V.mul(axis,-1),d:-V.dot(axis,point)+diameter}];
      if(part.buildingPanel){const span=V.length(V.sub(part.bounds.max,part.bounds.min))+1,start=V.add(point,V.mul(axis,-.00001)),through=S.ray(part.planes,start,V.add(start,V.mul(axis,span))),thickness=through?(through.exit-through.enter)*span:depth,totalDepth=Math.min(thickness*.95,Math.max(depth,penetrationDepth)),pieces=this.replace(part,S.subtract(part.faces,this.craterPlanes(point,axis,radius,totalDepth,part.material,part.id+'/spall-crater')));for(const p of pieces)p.cutDepth=(part.cutDepth||0)+1;if(energy>1e5)for(const p of pieces)if(MaterialImpact.closestBoundsDistance(p.bounds,point)<=radius*.92)this.remove(p);if(this.parts.has(part.id)){this.detach(part,point,direction,energy);}this.support();return {radius,depth:totalDepth};}
      let chip=part.faces;for(const plane of cutter)chip=S.clip(chip,plane.n,plane.d);if(S.volume(chip)<1e-7)return;let outside=S.subtract(part.faces,cutter);if(penetrationDepth>depth&&diameter>=.005){const bore=this.borePlanes(point,axis,diameter*1.2,penetrationDepth,part.material,part.id+'/spall-bore');outside=outside.flatMap(f=>S.subtract(f,bore));}outside=outside.filter(f=>S.volume(f)>1e-8);const chips=S.splitThree(chip,this.seed,part.id+'/spall',part.layering||0),pieces=this.replace(part,[...outside,...chips]);if(!pieces.length){if(part.buildingPanel&&this.parts.has(part.id)){this.detach(part,point,direction,energy);this.support();}return;}
      for(const p of pieces)if(p.buildingPanel)p.cutDepth=(part.cutDepth||0)+1;for(let i=outside.length;i<pieces.length;i++){const p=pieces[i];p.damage=0;p.detached=false;p.dynamic=false;p.physicsStarted=false;this.detach(p,point,V.mul(axis,-1),energy*.1/Math.max(1,chips.length));}this.support();return {radius,depth};
    }
    detach(p,point,direction,energy){if(p.detached)return;p.detached=true;if(p.buildingPanel)p.terminalDebris=true;this.emit({type:'detach',part:p,point,direction,energy});this.reindex();}
    detachBranch(part,point,direction,energy){const ids=new Set([part.id]);let changed=true;while(changed){changed=false;for(const p of this.parts.values())if(p.parent&&ids.has(p.parent)&&!ids.has(p.id)){ids.add(p.id);changed=true;}}const parts=[...ids].map(id=>this.parts.get(id)).filter(Boolean);part.parent=null;for(const p of parts)p.detached=true;this.reindex();this.emit({type:'branch',parts,part,point,direction,energy});}
    support(){if(this._supporting){this._supportAgain=true;return;}this._supporting=true;
      try{do{this._supportAgain=false;const supported=new Set(),spans=new Map(),queue=[],dependents=this.ensureSupportIndex();for(const p of this.parts.values()){if(p.detached)continue;if(p.anchored){supported.add(p.id);spans.set(p.id,0);queue.push(p.id);}}
        for(let i=0;i<queue.length;i++){const provider=this.parts.get(queue[i]);if(!provider)continue;for(const id of dependents.get(provider.id)||[]){const part=this.parts.get(id);if(!part||part.detached)continue;const integrity=part.supportIntegrity?.[provider.id]??1;if(integrity<=.02)continue;const link=this.supportPolicy?this.supportPolicy(part,provider):{cost:0,limit:Infinity};if(!link)continue;const distance=link.cost>0||link.carry?(spans.get(provider.id)||0)+link.cost:0;if(distance>(link.limit??Infinity)||distance>=(spans.get(id)??Infinity)-1e-9)continue;spans.set(id,distance);supported.add(id);queue.push(id);}}
        for(const p of this.parts.values())if(p.structuralGraph&&!p.detached&&!supported.has(p.id))this.detach(p,V.mul(V.add(p.bounds.min,p.bounds.max),.5),[0,-1,0],100);
      }while(this._supportAgain);}finally{this._supporting=false;if(this._reindexPending){this._reindexPending=false;this.reindex();}}
    }
    dispose(){this.parts.clear();this.listeners.clear();this.channels.length=0;this.movingParts.length=0;this.index=null;this.staticIndex=null;this.movingIndex=null;this.supportDependents.clear();if(this.rubbleField?.model===this)this.rubbleField.model=null;this.rubbleField=null;}
  }
  const traceLiveOnly=MaterialModel.prototype.trace;MaterialModel.prototype.trace=function(a,b,radius=0,dt,ignore){const blocked=new Set(ignore||[]);for(const part of this.parts.values())if(part.terminalDebris&&(!part.dynamic||(part.buildingPanel&&part.physicsStarted)))blocked.add(part.id);return traceLiveOnly.call(this,a,b,radius,dt,blocked);};
  R.MaterialModel=MaterialModel;
})();
