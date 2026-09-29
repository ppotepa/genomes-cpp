(function(){
  'use strict';
  const R=globalThis.RTS,V=R.DestructionSolid.V,STEP=1/60,State=R.ProjectileState,Impact=R.DestructionImpactSolver,MaterialImpact=R.MaterialImpact;
  const finite=v=>Array.isArray(v)&&v.length===3&&v.every(Number.isFinite);
  const props=p=>{const baseCd=p.cd??p.ammo?.cd??.28,strategy=R.DestructionAmmunitionStrategies?.forAmmo(p.ammo),aero=strategy?.aerodynamic?.(p);return {mass:p.mass??p.ammo?.mass??p.mass,diameter:p.diameter??p.ammo?.diameter,dragDiameter:p.dragDiameter??p.ammo?.dragDiameter??p.diameter??p.ammo?.diameter,cd:aero?.cd??baseCd};};
  function acceleration(v,p,environment){const q=props(p),relative=V.sub(v,environment.wind),speed=V.length(relative),mach=speed/environment.soundSpeed;
    const yaw=p.axis?1-Math.abs(V.dot(V.unit(relative),p.axis)):0,stateDrag=1+yaw*1.8+(p.deformation||0)*.35+(1-(p.stability??1))*.22,cd=q.cd*stateDrag*(1+.65*Math.exp(-Math.pow((mach-1.05)/.32,2))),area=Math.PI*q.dragDiameter**2/4;
    return V.add(environment.gravity,V.mul(relative,-.5*environment.density*cd*area*speed/q.mass));
  }
  function integrate(position,velocity,dt,p,env){const a=acceleration(velocity,p,env),v2=V.add(velocity,V.mul(a,dt/2)),b=acceleration(v2,p,env),v3=V.add(velocity,V.mul(b,dt/2)),c=acceleration(v3,p,env),v4=V.add(velocity,V.mul(c,dt)),d=acceleration(v4,p,env);
    return {p:V.add(position,V.mul(V.add(V.add(velocity,V.mul(v2,2)),V.add(V.mul(v3,2),v4)),dt/6)),v:V.add(velocity,V.mul(V.add(V.add(a,V.mul(b,2)),V.add(V.mul(c,2),d)),dt/6))};
  }
  function flightInterval(p,remaining,environment){
    const speed=Math.max(1,V.length(p.v)),dir=V.unit(p.v),a=acceleration(p.v,p,environment),lateral=V.sub(a,V.mul(dir,V.dot(a,dir))),curve=V.length(lateral),sagitta=Math.max(.00035,Math.min(.003,(p.diameter??.01)*.2)),curveDt=curve>1e-8?Math.sqrt(8*sagitta/curve):remaining,maxDistance=p.fragment?6:12;
    return Math.max(1e-7,Math.min(remaining,maxDistance/speed,curveDt));
  }
  class BallisticsWorld{
    constructor(options={}){this.model=options.model||new R.MaterialModel();this.worldBridge=options.worldBridge||null;this.traceRecorder=options.traceRecorder||null;this.groundPlane=options.ground===true?0:Number.isFinite(options.ground)?options.ground:null;this.maxTrackingTime=Number.isFinite(options.maxTrackingTime)&&options.maxTrackingTime>0?options.maxTrackingTime:(this.groundPlane===null?15:120);this.environment={wind:[0,0,0],density:1.225,gravity:[0,-9.80665,0],soundSpeed:343,...options.environment};this.tick=0;this.accumulator=0;this.projectiles=[];this.pending=[];this.listeners=new Set();this.maxProjectiles=256;this.maxFragments=2048;this.disposed=false;this.metrics={shots:0,hits:0,steps:0,queued:0,fragmentsDropped:0,ricochets:0,deflections:0,breakups:0,projectileFragments:0,traceSegments:0,flightEvents:0,worldProxyActivations:0};this.emitFlights=true;this.seen=new Set();this.nextShotId=0;}
    trace(method,...args){try{this.traceRecorder?.[method]?.(...args);}catch(_){/* keep diagnostic failures outside the solver */}}
    emit(e){for(const f of this.listeners)f(e);}
    fire(shot){if(this.disposed)throw Error('BallisticsWorld disposed');
      const weapon=R.DestructionWeapons[shot.weapon],ammo=R.DestructionAmmo[shot.ammo];
      if(!weapon||!ammo||!weapon.ammo.includes(shot.ammo))throw RangeError('Incompatible weapon/ammunition');
      if(!finite(shot.position)||!finite(shot.direction)||V.length(shot.direction)<1e-9||!Number.isInteger(shot.tick)||shot.tick<this.tick||!Number.isInteger(shot.seed))throw RangeError('Invalid shot');
      const key=shot.id===undefined?null:String(shot.id);if(key!==null&&this.seen.has(key))return false;
      if(this.projectiles.filter(p=>!p.fragment).length+this.pending.length>=this.maxProjectiles)return false;
      if(shot.camera&&this.model.trace(shot.camera,shot.position,ammo.diameter/2))return false;
      if(key!==null)this.seen.add(key);
      const event={...shot,id:shot.id??('auto-'+(++this.nextShotId)),position:shot.position.slice(),direction:V.unit(shot.direction),ammo,weapon};this.pending.push(event);this.metrics.shots++;this.emit({type:'shot',shot:event});return true;
    }
    spawn(shot){const p=State.create({id:shot.id,shotId:shot.id,position:shot.position,direction:shot.direction,ammo:shot.ammo,weapon:shot.weapon,seed:shot.seed,speed:shot.ammo.velocity[shot.weapon.id]});this.projectiles.push(p);this.trace('start',shot,p);}
    strategy(p){return R.DestructionAmmunitionStrategies?.forAmmo(p.ammo)||null;}
    shouldDetonate(p){return !!(this.strategy(p)?.shouldDetonate(p)||(p.ammo?.kind==='he'&&p.ammo.fuze?.armed&&p.ammo.fuze.mode==='contact'));}
    impactContext(p,normal,extra={}){const strategy=this.strategy(p);return {normal,ammoKind:p.ammo.kind,ammoId:p.ammo.id,strategyId:strategy?.id||p.ammo.strategyId,caliberId:p.ammo.caliberId,variantId:p.ammo.variantId,construction:p.ammo.construction,seed:p.seed,projectile:State.snapshot(p),resolvedImpulse:true,...extra};}
    hitEvent(p,part,point,direction,normal,details,damage){this.metrics.hits++;const te={component:part.id,layers:part.layers||[],orientation:p.orientation.slice(),angularVelocity:p.angularVelocity.slice(),age:p.age,travel:p.travel,point:point.slice(),direction:direction.slice(),normal:normal.slice(),material:part.material,...details};this.trace('contact',p,te);this.emit({type:'hit',projectileId:p.id,shotId:p.shotId,part:part.id,material:part.material,layers:part.layers||null,point,direction,diameter:p.diameter,fragment:p.fragment,...this.impactContext(p,normal),...details,response:damage?.response||'mark',geometryChanged:!!damage?.geometryChanged,radius:damage?.radius,scarDepth:damage?.depth,deformationProfile:damage?.deformationProfile,materialMode:damage?.materialMode||details.materialMode,frontRadius:damage?.frontRadius,channelRadius:damage?.channelRadius,stressRadius:damage?.stressRadius,rearRadius:damage?.rearRadius,frontChip:damage?.frontChip,rearChip:damage?.rearChip,exitRadius:damage?.exitRadius,channelExitRadius:damage?.channelExitRadius,exitProfile:damage?.exitProfile,masonry:damage?.masonry,stress:damage?.stress,removedVolume:damage?.localDamage?.removedVolume??damage?.removedVolume,rearDamage:damage?.localDamage?.rearDamage,localDamage:damage?.localDamage,damageEnergy:damage?.damageEnergy??details.damageEnergy,damageBudget:details.damageBudget,grainDirection:part.grainDirection||null});}
    breakup(p,point,normal,available=0){
      const plan=State.breakupPlan(p);if(!plan||p.dead)return null;
      const speed=V.length(p.v),energy=.5*p.mass*speed*speed;if(energy<1)return null;
      const requested=plan.requested,capacity=Math.max(0,this.maxFragments-this.projectiles.filter(q=>q.fragment).length),count=Math.min(requested,capacity),base=V.unit(p.v),random=R.DestructionSolid.stream(p.seed,(p.id||'projectile')+'/breakup/'+p.impactIndex);
      const ref=Math.abs(base[1])<.8?[0,1,0]:[1,0,0],u=V.unit(V.cross(base,ref)),v=V.cross(base,u),shedMass=p.mass*plan.shedMassFraction,totalFragmentEnergy=energy*plan.energyFraction,fragmentMass=Math.max(.00005,shedMass/Math.max(1,requested)),fragmentEnergy=totalFragmentEnergy/Math.max(1,requested),fragmentSpeed=Math.sqrt(2*fragmentEnergy/fragmentMass),fragmentDiameter=Math.cbrt(fragmentMass*6/(Math.PI*7850)),cone=.025+plan.severity*.16;
      this.metrics.fragmentsDropped+=requested-count;this.metrics.projectileFragments+=count;this.metrics.breakups++;
      for(let i=0;i<count;i++){const a=Math.PI*2*(i/requested+random()*.15),r=cone*(.35+.65*random()),dir=V.unit(V.add(base,V.add(V.mul(u,Math.cos(a)*r),V.mul(v,Math.sin(a)*r)))),ammo={id:p.ammo.id+'/secondary',mass:fragmentMass,diameter:fragmentDiameter,dragDiameter:fragmentDiameter,cd:.68,kind:'fragment'},frag=State.fragment({id:(p.id||'projectile')+'/secondary-'+p.impactIndex+'-'+i,shotId:p.shotId,parentTraceId:p.traceId,position:point,direction:dir,ammo,seed:(p.seed+Math.imul(p.impactIndex+1,131)+i)>>>0,speed:fragmentSpeed,mass:fragmentMass,diameter:fragmentDiameter,dragDiameter:fragmentDiameter,cd:.68});frag.available=available;this.projectiles.push(frag);this.trace('fragment',frag);}
      p.energyLedger.fragmentEnergy+=totalFragmentEnergy*count/requested;p.energyLedger.unrepresentedEnergy+=totalFragmentEnergy*(1-count/requested);const coreEnergy=Math.max(0,energy-totalFragmentEnergy);
      if(plan.catastrophic){p.energyLedger.unrepresentedEnergy+=coreEnergy;p.dead=true;p.v=[0,0,0];}
      else{p.mass=Math.max(.00005,p.mass-shedMass);p.integrity*=1-plan.severity*.18;p.stability*=1-plan.severity*.22;p.deformation=Math.min(1,p.deformation+plan.severity*.08);p.v=V.mul(base,Math.sqrt(2*coreEnergy/Math.max(1e-9,p.mass)));}
      const rotationBefore=State.rotationalEnergy(p);State.refreshInertia(p);p.energyLedger.unrepresentedEnergy+=Math.max(0,rotationBefore-State.rotationalEnergy(p));
      const event={type:'projectile-breakup',projectileId:p.id,shotId:p.shotId,point:point.slice(),normal:normal.slice(),requestedFragments:requested,fragments:count,catastrophic:plan.catastrophic,severity:plan.severity,energy:totalFragmentEnergy,projectile:State.snapshot(p)};this.emit(event);return event;
    }
    flushContact(p,penetrated=false,available=0){
      if(!p.contact)return null;const c=p.contact,impactDiameter=c.diameter??p.diameter;p.contact=null;
      const targetVelocity=c.targetVelocity,relativeVelocity=c.incomingVelocity,response=Impact.contact(p,c.part,{mode:penetrated?'penetration':'stop',normal:c.normal,relativeVelocity,targetVelocity,entryEnergy:c.entryEnergy,residualEnergy:c.residualEnergy,point:p.p,contactPoint:p.p,layerIntervals:c.layerIntervals,pathLength:c.distance});
      const outgoing=V.unit(response.outgoingVelocity),exitPoint=p.p.slice(),strategy=this.strategy(p),damageBudget=strategy?.damageBudget({part:c.part,ammo:p.ammo,mass:p.mass,ammoKind:p.ammo.kind,lost:c.energy,entryEnergy:c.entryEnergy,residualEnergy:c.residualEnergy,diameter:impactDiameter,penetrated,incidence:response.incidence??1,fragment:p.fragment})||MaterialImpact.budget({part:c.part,ammoKind:p.ammo.kind,lost:c.energy,entryEnergy:c.entryEnergy,residualEnergy:c.residualEnergy,diameter:impactDiameter,penetrated,incidence:response.incidence??1,fragment:p.fragment});
      this.contactImpulse(p,c.part,p.p,response);State.transition(p,response);if(!penetrated)p.energyLedger.contactLoss+=Math.max(0,-response.rotationChange);p.energyLedger.targetWork+=(response.targetWork||0)-(c.targetWork||0);
      if(penetrated&&V.dot(c.direction,outgoing)<.99995)this.metrics.deflections++;
      const breakup=penetrated?this.breakup(p,exitPoint,c.normal,available):null;
      let damage;for(const section of c.sections){const sectionBudget=strategy?.damageBudget({part:section.part,ammo:p.ammo,mass:p.mass,lost:section.energy,diameter:impactDiameter,penetrated,fragment:p.fragment})||MaterialImpact.budget({part:section.part,ammoKind:p.ammo.kind,lost:section.energy,diameter:impactDiameter,penetrated,fragment:p.fragment});damage=this.model.damage(section.part,section.energy,section.point,c.direction,impactDiameter,penetrated,section.distance,p.fragment,this.impactContext(p,c.normal,{incomingDirection:c.direction,outgoingDirection:outgoing,exitPoint,incidence:response.incidence,damageBudget:sectionBudget}));}
      this.hitEvent(p,c.part,c.point,c.direction,c.normal,{diameter:impactDiameter,result:penetrated?'penetrated':'stopped',energy:State.energy(p),relativeEntryEnergy:c.entryEnergy,relativeEnergy:c.residualEnergy,lost:c.energy,entryEnergy:c.worldEntryEnergy,thickness:c.thickness,depth:c.distance,exitPoint,outgoingDirection:outgoing,incidence:response.incidence,layerPath:c.layerIntervals,materialMode:damageBudget.mode,damageEnergy:damageBudget.structuralEnergy,breakup:breakup?{catastrophic:breakup.catastrophic,severity:breakup.severity,fragments:breakup.fragments}:null},damage);
      return response;
    }
    ricochet(p,hit,dir,energy,response,label='ricochet',available=0){
      const impactDiameter=p.diameter,point=hit.point.slice(),incoming=dir.slice(),outgoing=response.direction.slice(),damageBudget=MaterialImpact.budget({part:hit.part,ammoKind:p.ammo.kind,lost:response.lost,entryEnergy:energy,residualEnergy:response.energy,diameter:impactDiameter,penetrated:false,incidence:response.incidence??1,fragment:p.fragment});this.contactImpulse(p,hit.part,point,response);State.transition(p,response);p.energyLedger.contactLoss+=response.lost;p.energyLedger.targetWork+=response.targetWork||0;this.metrics.ricochets++;
      const breakup=this.breakup(p,point,response.normal,available);p.p=V.add(p.p,V.mul(response.normal,.00012));const damage=this.model.damage(hit.part,response.lost,point,incoming,impactDiameter,false,0,p.fragment,this.impactContext(p,response.normal,{incomingDirection:incoming,outgoingDirection:outgoing,incidence:response.incidence,damageBudget,breakup:breakup?{catastrophic:breakup.catastrophic,severity:breakup.severity}:null}));
      this.hitEvent(p,hit.part,point,incoming,response.normal,{diameter:impactDiameter,result:label,energy:response.energy,lost:response.lost,entryEnergy:energy,outgoingDirection:outgoing,incidence:response.incidence,materialMode:damageBudget.mode,damageEnergy:damageBudget.structuralEnergy,breakup:breakup?{catastrophic:breakup.catastrophic,severity:breakup.severity,fragments:breakup.fragments}:null},damage);return response;
    }
    step(dt){if(this.disposed)return 0;if(!Number.isFinite(dt)||dt<0)throw RangeError('Invalid dt');this.accumulator+=dt;let steps=0;
      while(this.accumulator+1e-10>=STEP&&steps<8){this.fixedStep();this.accumulator=Math.max(0,this.accumulator-STEP);steps++;}this.metrics.queued=this.accumulator/STEP;return steps;
    }
    contactImpulse(p,part,point,response){this.model.emit({type:'contact-impulse',part,point:point.slice(),impulse:response.targetImpulse,angularImpulse:response.targetAngularImpulse});}
    advance(p,position,velocity,dt,material=false){
      const from=p.p.slice(),before=State.energy(p);p.p=position;p.v=velocity;p.age+=dt;p.travel+=V.length(V.sub(position,from));State.integrate(p,dt);
      if(!material)p.energyLedger.flightWork+=State.energy(p)-before;
      this.trace('segment',p,from,p.p,p.v,State.energy(p),before,material);
      if(this.emitFlights)this.emit({type:'flight',projectileId:p.id,shotId:p.shotId,traceId:p.traceId,from,to:p.p,fragment:p.fragment,speed:V.length(p.v),axis:p.axis.slice(),stability:p.stability});
    }
    fixedStep(){
      const due=this.pending.filter(s=>s.tick<=this.tick);this.pending=this.pending.filter(s=>s.tick>this.tick);for(const s of due)this.spawn(s);
      const next=[];for(const p of this.projectiles){let remaining=p.available??STEP;delete p.available;let iterations=0;
        while(remaining>1e-9&&!p.dead){
          if(p.age>=this.maxTrackingTime){p.dead=true;p.endReason='tracking-limit';break;}
          if(++iterations>512)throw Error('Collision solver failed to progress');
          if(p.contact){
            const c=p.contact;if(!c.part.rubble&&!this.model.parts.has(c.part.id)){this.flushContact(p,true,remaining);continue;}const section=c.sections.at(-1),relative=V.sub(p.v,c.targetVelocity),speed=V.length(relative),dir=c.direction;
            const interval=c.intervals.find(x=>x.endDistance>c.progress+1e-8),left=interval?interval.endDistance-c.progress:0;
            if(!interval){
              // Adjacent technical sections are one physical contact; a real air gap ends it.
              p.exiting=p.exiting||new Set();p.exiting.add(c.part.id);
              const probe=this.model.trace(p.p,V.add(p.p,V.mul(dir,.00002)),0,0,p.exiting);
              if(probe&&(probe.part.physicalSolidId||probe.part.id)===c.physicalSolidId&&probe.enter<.05){
                c.part=probe.part;const through=R.DestructionSolid.ray(c.part.planes,p.p,V.add(p.p,V.mul(dir,10000)),0),thickness=through?through.exit*10000:0;
                if(thickness>1e-7){c.intervals=this.intervals(c.part,p.p,dir,thickness,p);c.layerIntervals.push(...c.intervals);c.progress=0;c.thickness+=thickness;c.sections.push({part:c.part,point:p.p.slice(),energy:0,distance:0});continue;}
              }
              this.flushContact(p,true,remaining);continue;
            }
            const force=interval.force,a=force/p.mass,stopTime=a>0?speed/a:Infinity,time=Math.min(remaining,this.maxTrackingTime-p.age,stopTime),possible=Math.max(0,speed*time-.5*a*time*time),distance=Math.min(left,possible),outSpeed=Math.sqrt(Math.max(0,speed*speed-2*a*distance)),used=distance>0?2*distance/Math.max(1e-12,speed+outSpeed):time;
            const lost=force*distance,targetWork=p.mass*V.dot(c.targetVelocity,V.mul(dir,outSpeed-speed));p.energyLedger.materialWork+=lost;p.energyLedger.targetWork+=targetWork;c.targetWork=(c.targetWork||0)+targetWork;c.energy+=lost;section.energy+=lost;section.distance+=distance;c.distance+=distance;c.progress+=distance;c.residualEnergy=.5*p.mass*outSpeed*outSpeed;
            this.advance(p,V.add(p.p,V.add(V.mul(dir,distance),V.mul(c.targetVelocity,used))),V.add(V.mul(dir,outSpeed),c.targetVelocity),used,true);remaining-=used;
            if(c.progress>=interval.endDistance-1e-8)this.trace('boundary',p);
            if(outSpeed<1e-6){this.flushContact(p,false,remaining);p.dead=true;p.endReason='stopped';}
            continue;
          }
          if(p.exiting)for(const id of p.exiting){const part=this.model.parts.get(id);if(!part||!R.DestructionSolid.sweptSphere(part.faces,part.planes,p.p,p.p,p.diameter/2))p.exiting.delete(id);}
          const h=Math.min(flightInterval(p,remaining,this.environment),this.maxTrackingTime-p.age),state=integrate(p.p,p.v,h,p,this.environment);this.metrics.traceSegments++;
          // A radius candidate must touch the actual convex surface. HE's armed
          // contact fuse uses that surface point; penetration follows the centreline.
          if(this.groundPlane!==null&&p.p[1]>=this.groundPlane&&state.p[1]<=this.groundPlane){const fraction=(p.p[1]-this.groundPlane)/Math.max(1e-12,p.p[1]-state.p[1]),groundTime=h*fraction,groundState=integrate(p.p,p.v,groundTime,p,this.environment),point=groundState.p.slice();point[1]=this.groundPlane;this.advance(p,point,groundState.v,groundTime);remaining-=groundTime;p.dead=true;p.endReason='ground-contact';const event={component:'ground',material:'ground',point:point.slice(),normal:[0,1,0],direction:V.unit(p.v),incidence:Math.abs(V.dot(V.unit(p.v),[0,1,0])),result:'ground-contact',energy:State.energy(p),entryEnergy:State.energy(p)};this.trace('contact',p,event);this.emit({type:'ground-contact',projectileId:p.id,shotId:p.shotId,traceId:p.traceId,point:point.slice(),velocity:p.v.slice(),speed:V.length(p.v),fragment:p.fragment});break;}
          let candidate=this.model.trace(p.p,state.p,p.diameter/2,h,p.exiting);
          // Battlefield buildings are logical until first contact. Materialize only
          // the indexed building chunk touched by this swept segment, then repeat
          // the exact material query against the shared MaterialModel.
          if(!candidate&&this.worldBridge){const proxy=this.worldBridge.materializeForSegment(p.p,state.p,p);if(proxy){this.worldBridge.flush();candidate=this.model.trace(p.p,state.p,p.diameter/2,h,p.exiting);this.metrics.worldProxyActivations++;}}
          const centreHit=candidate?this.model.trace(p.p,state.p,0,h,p.exiting):null;
          let hit=centreHit;
          if(candidate&&(!centreHit||this.shouldDetonate(p))&&!candidate.part.rubble){
            const surfacePoint=R.DestructionSolid.closestSurface(candidate.part.faces,candidate.part.planes,candidate.point);
            if(surfacePoint.distance2<=(p.diameter/2)**2*(1+1e-5))hit={...candidate,point:surfacePoint.point,center:candidate.point,edge:!centreHit,normal:surfacePoint.distance2>1e-16?surfacePoint.normal:candidate.normal};
          }
          if(!hit){this.advance(p,state.p,state.v,h);remaining-=h;continue;}
          const entryTime=h*hit.enter,entry=integrate(p.p,p.v,entryTime,p,this.environment);this.advance(p,(hit.center||hit.point).slice(),entry.v,entryTime);remaining-=entryTime;
          const targetVelocity=this.model.targetVelocity(hit.part,hit.point),relativeVelocity=V.sub(p.v,targetVelocity),speed=V.length(relativeVelocity),dir=V.unit(relativeVelocity),energy=.5*p.mass*speed*speed,worldEnergy=State.energy(p);
          const options={normal:hit.normal,relativeVelocity,targetVelocity,point:hit.point,contactPoint:hit.point,projectileCenter:hit.center||undefined,entryEnergy:energy};
          const surface=Impact.contact(p,hit.part,{...options,mode:'surface'});
          if(this.shouldDetonate(p)){
            // Armed contact fuse: finite nose work precedes release of explosive energy.
            const force=this.intervals(hit.part,hit.point,dir,p.diameter*.25,p)[0]?.force??0,work=Math.min(energy,force*p.diameter*.25),response=Impact.contact(p,hit.part,{...options,mode:'penetration',residualEnergy:energy-work});
            this.contactImpulse(p,hit.part,hit.point,response);State.transition(p,response);p.energyLedger.materialWork+=work;p.energyLedger.targetWork+=response.targetWork||0;
            const damage=this.model.damage(hit.part,work,hit.point,dir,p.diameter,false,0,false,this.impactContext(p,hit.normal));
            this.hitEvent(p,hit.part,hit.point,dir,hit.normal,{result:'detonated',entryEnergy:worldEnergy,energy:State.energy(p),lost:work,relativeEntryEnergy:energy,relativeEnergy:energy-work},damage);
            this.explode(p,hit,remaining);p.dead=true;p.endReason='exploded';break;
          }
          if(hit.edge){const edge=Impact.contact(p,hit.part,{...options,mode:'edge'});this.ricochet(p,hit,dir,worldEnergy,edge,'glance',remaining);p.exiting=p.exiting||new Set();p.exiting.add(hit.part.id);continue;}
          if(surface.ricochet){this.ricochet(p,hit,dir,worldEnergy,surface,'ricochet',remaining);continue;}
          const through=hit.part.rubble?null:R.DestructionSolid.ray(hit.part.planes,p.p,V.add(p.p,V.mul(dir,10000)),0),thickness=hit.part.rubble?Math.max(.001,hit.rubbleThickness||.001):through?through.exit*10000:0;
          if(thickness<1e-8){const edge=Impact.contact(p,hit.part,{...options,mode:'edge'});this.ricochet(p,hit,dir,worldEnergy,edge,'glance',remaining);p.exiting=p.exiting||new Set();p.exiting.add(hit.part.id);continue;}
          this.trace('boundary',p);const intervals=this.intervals(hit.part,p.p,dir,thickness,p);
          p.contact={part:hit.part,physicalSolidId:hit.part.physicalSolidId||hit.part.id,sections:[{part:hit.part,point:p.p.slice(),energy:0,distance:0}],energy:0,distance:0,progress:0,intervals,layerIntervals:intervals.slice(),point:p.p.slice(),direction:dir,normal:hit.normal,incomingVelocity:relativeVelocity,targetVelocity,entryEnergy:energy,worldEntryEnergy:worldEnergy,residualEnergy:energy,thickness,diameter:p.diameter};
        }
        if(!p.dead)next.push(p);else this.trace('finish',p.traceId,p.endReason||'catastrophic',p.p,p.v,p);
      }
      this.projectiles=next;this.tick++;this.metrics.steps++;
    }
    intervals(part,point,dir,thickness,p){
      const layers=MaterialImpact.layerIntervals(part,point,V.add(point,V.mul(dir,thickness))),area=Math.PI*p.diameter*p.diameter/4;
      if(!layers.length)layers.push({material:part.material,startDistance:0,endDistance:thickness,pathLength:thickness,isVoid:false});
      return layers.map(l=>{const sample=MaterialImpact.sampleLocal(part,V.add(point,V.mul(dir,l.startDistance)),'resistance'),weak=sample?.weakness??this.model.localWeakness(part,point),pressure=l.isVoid?0:MaterialImpact.resistance({...part,material:l.material,layers:null},p.ammo.kind);return {...l,force:pressure*area*(part.strengthScale??1)*(1-.65*Math.min(.95,part.damage))*(1-.32*weak)};});
    }
    explode(projectile,hit,available=STEP){projectile.energyLedger??={materialWork:0,targetWork:0,explosiveEnergy:0,fragmentEnergy:0,blastEnergy:0,unrepresentedEnergy:0,rotationWork:0,flightWork:0,contactLoss:0};projectile.v??=[0,0,0];projectile.angularVelocity??=[0,0,0];projectile.orientation??=[0,0,0,1];projectile.inertia??=[0,0,0];projectile.mass??=projectile.ammo.mass??.001;const mass=projectile.ammo.explosive,projectileDiameter=projectile.diameter??projectile.ammo.diameter,scale=Math.cbrt(mass),origin=V.add(hit.point,V.mul(hit.normal,.002)),power=mass*4e6,radius=scale*10;
      const affected=[...this.model.parts.values()],exposures=[];let blastDeposited=0,breaches=0,geometryChanges=0;projectile.energyLedger.explosiveEnergy+=power;
      for(const part of affected){if(part.indestructible)continue;const center=R.DestructionSolid.centroid(part.faces),delta=V.sub(center,origin),distance=V.length(delta);if(distance>radius)continue;const direction=V.unit(delta),faceSamples=part.faces.filter((_,i)=>i%Math.max(1,Math.ceil(part.faces.length/3))===0).slice(0,3).map(f=>R.DestructionSolid.centroid([f])),samples=[center,...faceSamples],ignore=new Set([part.id]);let exposed=0;
        for(const sample of samples){const obstacle=this.model.trace(origin,sample,0,0,ignore),material=obstacle?.part?.material;exposed+=obstacle?.part?.indestructible?0:({foliage:.9,glass:.65,wood:.3,tissue:.35,brick:.08,concrete:.04,steel:.08,rock:.03,armor:.06}[material]??1);}exposed/=samples.length;
        let projectedArea=0;for(let i=0;i<part.faces.length;i++){const f=part.faces[i];let area=0;for(let k=1;k<f.length-1;k++)area+=V.length(V.cross(V.sub(f[k],f[0]),V.sub(f[k+1],f[0])))/2;projectedArea+=area*Math.abs(V.dot(part.planes[i].n,direction))*.5;}
        const z=distance/Math.max(.01,scale),peakPressure=1e5*(.12/(z+.25)+.5/(z+.25)**2+1/(z+.25)**3),duration=.003*scale*(1+.2*z),weight=exposed*projectedArea/(4*Math.PI*Math.max(scale*.3,distance)**2);
        exposures.push({part,center,point:faceSamples.reduce((a,b)=>V.length(V.sub(a,origin))<V.length(V.sub(b,origin))?a:b,center),direction,weight,pressureImpulse:peakPressure*duration*projectedArea*exposed});
      }
      const normalization=Math.max(1,exposures.reduce((sum,e)=>sum+e.weight,0));
      this.model.batch(()=>{for(const e of exposures){const energy=power*.45*e.weight/normalization,bodyMass=e.part.volume*R.DestructionMaterials[e.part.material].density,impulse=Math.min(e.pressureImpulse,Math.sqrt(2*bodyMass*energy*.4));this.model.emit({type:'blast',part:e.part,point:e.point,direction:e.direction,impulse});blastDeposited+=energy;const damage=this.model.damage(e.part,energy*.6,e.point,e.direction,projectileDiameter,false,0,false,{blast:true,ammoKind:'he',ammoId:projectile.ammo.id,seed:projectile.seed,normal:V.mul(e.direction,-1),resolvedImpulse:true});if(damage?.geometryChanged)geometryChanges++;if(damage?.response==='blast-breach')breaches++;}});this.model.rubbleField?.blast(origin,radius,power*.18);
      const random=R.DestructionSolid.stream(projectile.seed,'fragments'),profile=projectile.ammo.fragmentation||{},requested=profile.fragmentCount??projectile.ammo.fragments,capacity=Math.max(0,this.maxFragments-this.projectiles.filter(p=>p.fragment).length),count=Math.floor(Math.min(requested,capacity)/2)*2,rotation=random()*Math.PI*2;this.metrics.fragmentsDropped+=requested-count;
      // The declared energy share belongs to the complete pattern. Capacity loss omits
      // complete opposite pairs and cannot raise the speed of the fragments we keep.
      const perFragmentEnergy=power*(profile.explosiveEnergyFraction??.25)/Math.max(1,requested),pairCount=Math.ceil(requested/2),massWeights=Array.from({length:pairCount},()=>1+(random()*2-1)*(profile.massSpread??0)),massWeightSum=massWeights.reduce((a,b)=>a+b,0),fragmentMasses=massWeights.map(w=>Math.max(1e-8,(projectile.mass-mass)*(profile.bodyMassFraction??.30)*.5*w/massWeightSum));
      let inheritedFragmentEnergy=0,radialFragmentEnergy=0;
      for(let i=0;i<count;i++){const pair=Math.floor(i/2),fragmentMass=fragmentMasses[pair],fragmentSpeed=Math.sqrt(2*perFragmentEnergy/fragmentMass),fragmentDiameter=Math.cbrt(fragmentMass*6/(Math.PI*7850)),sign=i%2?-1:1,y=1-2*(pair+.5)/Math.max(1,pairCount),r=Math.sqrt(Math.max(0,1-y*y)),angle=pair*2.399963229728653+rotation,dir=V.mul([Math.cos(angle)*r,y,Math.sin(angle)*r],sign),ammo={id:projectile.ammo.id+'/fragment',mass:fragmentMass,diameter:fragmentDiameter,dragDiameter:fragmentDiameter,cd:.7,kind:'fragment'};
        const fragment=State.fragment({parentTraceId:projectile.traceId,id:(projectile.id||'shot')+'/frag-'+i,shotId:projectile.shotId,position:origin,direction:dir,ammo,seed:(projectile.seed+i)>>>0,speed:fragmentSpeed,mass:fragmentMass,diameter:fragmentDiameter,dragDiameter:fragmentDiameter,cd:.7});fragment.v=V.add(projectile.v,V.mul(dir,fragmentSpeed));fragment.initialEnergy=State.energy(fragment);fragment.available=available;this.projectiles.push(fragment);this.trace('fragment',fragment);inheritedFragmentEnergy+=.5*fragmentMass*V.dot(projectile.v,projectile.v);radialFragmentEnergy+=perFragmentEnergy;}
      projectile.energyLedger.fragmentEnergy+=inheritedFragmentEnergy+radialFragmentEnergy;projectile.energyLedger.unrepresentedEnergy+=Math.max(0,State.energy(projectile)-inheritedFragmentEnergy);
      projectile.energyLedger.blastEnergy+=blastDeposited+power*.18;projectile.energyLedger.unrepresentedEnergy+=Math.max(0,power-blastDeposited-power*.18-radialFragmentEnergy);this.trace('blast',projectile,{breaches,geometryChanges,energy:power});projectile.energyLedger.unrepresentedEnergy+=State.rotationalEnergy(projectile);const bodyDirection=V.unit(projectile.v);projectile.v=[0,0,0];projectile.angularVelocity=[0,0,0];
      this.emit({type:'explosion',breaches,geometryChanges,projectileId:projectile.id,shotId:projectile.shotId,point:origin,radius,explosiveMass:mass,energy:power,fragments:count,requestedFragments:requested,material:hit.part.material,normal:hit.normal,direction:bodyDirection,diameter:projectileDiameter,ammoKind:projectile.ammo.kind,ammoId:projectile.ammo.id,seed:projectile.seed});
    }
    dispose(){this.trace('clear');this.disposed=true;this.projectiles.length=0;this.pending.length=0;this.listeners.clear();this.seen.clear();}
  }
  BallisticsWorld.integrate=integrate;BallisticsWorld.flightInterval=flightInterval;BallisticsWorld.STEP=STEP;R.BallisticsWorld=BallisticsWorld;
  R.connectWeaponBallistics=function(controller,world,makeShot){const previous=controller.onShot;const handler=event=>{if(previous)previous.call(controller,event);const shot=makeShot(event);if(shot)world.fire(shot);};controller.onShot=handler;return ()=>{if(controller.onShot===handler)controller.onShot=previous;};};
})();
