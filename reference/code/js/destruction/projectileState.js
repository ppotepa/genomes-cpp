(function(){
  'use strict';
  const R=globalThis.RTS,V=R.DestructionSolid.V,M=R.ProjectileMath||{normalize:v=>v,between:()=>[0,0,0,1]};
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  function create(base){
    const ammo=base.ammo,dir=V.unit(base.direction),speed=base.speed??ammo.velocity?.[base.weapon?.id]??0,profile=R.DestructionImpactProfiles.ammunition[ammo.kind]||R.DestructionImpactProfiles.ammunition.ball;
    const state={
      id:base.id===undefined?null:String(base.id),shotId:base.shotId===undefined?(base.id===undefined?null:String(base.id)):String(base.shotId),p:base.position.slice(),v:V.mul(dir,speed),axis:dir.slice(),orientation:(base.orientation||M.between([0,0,1],dir)).slice(),angularVelocity:(base.angularVelocity||[0,0,0]).slice(),bodyForward:[0,0,1],ammo,seed:base.seed>>>0,age:0,fragment:!!base.fragment,
      mass:base.mass??ammo.mass,diameter:base.diameter??ammo.diameter,dragDiameter:base.dragDiameter??ammo.dragDiameter??ammo.diameter,cd:base.cd??ammo.cd,
      integrity:clamp(base.integrity??profile.integrity,0,1),deformation:clamp(base.deformation??0,0,1),stability:clamp(base.stability??1,0,1),
      impactIndex:base.impactIndex||0,ricochetCount:base.ricochetCount||0,travel:base.travel||0
    };
    const radius=Math.max(1e-4,state.diameter/2),length=Math.max(state.diameter*2,radius*4),ix=state.mass*(3*radius*radius+length*length)/12,iy=state.mass*radius*radius/2;
    state.inertia=base.inertia?.slice()||[ix,ix,iy];state.inverseInertia=base.inverseInertia?.slice()||state.inertia.map(v=>1/Math.max(1e-9,v));
    state.axis=V.unit(M.rotate?M.rotate(state.orientation,state.bodyForward):dir);
    state.traceId=String(base.traceId??(state.fragment?state.id:state.shotId));state.parentTraceId=base.parentTraceId??null;
    state.initialEnergy=.5*state.mass*V.dot(state.v,state.v);state.initialRotationalEnergy=rotationalEnergy(state);
    state.energyLedger={materialWork:0,contactLoss:0,targetWork:0,flightWork:0,fragmentEnergy:0,unrepresentedEnergy:0,rotationWork:0,explosiveEnergy:0,blastEnergy:0};
    return state;
  }
  function snapshot(p){
    return {version:'projectile-state-3',id:p.id,shotId:p.shotId,traceId:p.traceId,parentTraceId:p.parentTraceId,energy:energy(p),rotationalEnergy:rotationalEnergy(p),velocity:p.v.slice(),age:p.age,travel:p.travel,energyLedger:{...p.energyLedger},balanceError:balanceError(p),axis:p.axis.slice(),bodyForward:p.bodyForward.slice(),orientation:p.orientation.slice(),angularVelocity:p.angularVelocity.slice(),inertia:p.inertia.slice(),inverseInertia:p.inverseInertia.slice(),integrity:p.integrity,deformation:p.deformation,stability:p.stability,mass:p.mass,diameter:p.diameter,impactIndex:p.impactIndex,ricochetCount:p.ricochetCount};
  }
  function transition(p,result){
    if(Array.isArray(result.orientation)&&result.orientation.length===4)p.orientation=M.normalize(result.orientation);
    const oldMass=p.mass;
    p.mass=Math.max(oldMass*.45,oldMass*(1-clamp(result.massLoss||0,0,.3)));
    p.diameter=Math.max(p.ammo.diameter*.75,p.diameter*(1+clamp(result.diameterGrowth||0,0,.35)));
    p.dragDiameter=Math.max(p.diameter,p.dragDiameter*(1+clamp(result.dragGrowth||0,0,.5)));
    p.integrity=clamp(p.integrity-(result.integrityLoss||0),0,1);
    p.deformation=clamp(p.deformation+(result.deformation||0),0,1);
    p.stability=clamp(p.stability-(result.stabilityLoss||0),0,1);
    p.axis=V.unit(M.rotate?M.rotate(p.orientation,p.bodyForward):(result.axis||result.direction||p.axis));
    p.impactIndex++;
    if(result.ricochet)p.ricochetCount++;
    const energy=Math.max(0,result.energy||0),speed=energy>0?Math.sqrt(2*energy/Math.max(1e-9,p.mass)):0;
    p.v=result.outgoingVelocity?result.outgoingVelocity.slice():V.mul(V.unit(result.direction||p.v),speed);
    if(result.angularVelocity)p.angularVelocity=result.angularVelocity.slice();
    const rotation=rotationalEnergy(p);refreshInertia(p);const after=rotationalEnergy(p);if(after>0)p.angularVelocity=V.mul(p.angularVelocity,Math.sqrt(rotation/after));
    return p;
  }
  function integrate(p,dt){
    const before=rotationalEnergy(p),speed=V.length(p.v);
    if(speed>1e-6&&M.rotate){
      const velocityAxis=V.unit(p.v),bodyAxis=V.unit(M.rotate(p.orientation,p.bodyForward)),error=V.cross(bodyAxis,velocityAxis),restore=clamp((p.stability??1)*8,0,8);
      p.angularVelocity=p.angularVelocity.map((v,i)=>(v+error[i]*restore*dt)*Math.max(0,1-dt*(.35+(1-(p.stability??1))*.45)));
    }
    if(M.integrate)p.orientation=M.integrate(p.orientation,p.angularVelocity,dt);
    if(M.rotate)p.axis=V.unit(M.rotate(p.orientation,p.bodyForward));
    p.energyLedger.rotationWork+=rotationalEnergy(p)-before;

    return p;
  }
  function breakupPlan(p){
    if(p.fragment||R.DestructionAmmunitionStrategies?.forAmmo(p.ammo).shouldDetonate(p))return null;
    const profile=R.DestructionImpactProfiles.ammunition[p.ammo.kind]||R.DestructionImpactProfiles.ammunition.ball,max=profile.maxSecondaryFragments||0;if(!max)return null;
    const integritySeverity=profile.breakupIntegrity>0?clamp((profile.breakupIntegrity-p.integrity)/profile.breakupIntegrity,0,1):0;
    const stabilitySeverity=profile.breakupStability>0?clamp((profile.breakupStability-p.stability)/profile.breakupStability,0,1):0;
    const deformationSeverity=clamp((p.deformation-.48)/.52,0,1),severity=Math.max(integritySeverity,stabilitySeverity*.7+deformationSeverity*.3);
    if(severity<=0)return null;
    const requested=Math.max(1,Math.min(max,1+Math.floor(severity*max))),catastrophic=severity>.82||p.integrity<.05;
    return {severity,requested,catastrophic,shedMassFraction:clamp(.06+severity*.28,.06,.34),energyFraction:clamp(.04+severity*.22,.04,.26)};
  }
  function fragment(base){
    return create({...base,fragment:true,integrity:.42,stability:.58,deformation:.18});
  }
  function refreshInertia(p){const r=Math.max(1e-4,p.diameter/2),l=p.diameter*2,i=p.mass*(3*r*r+l*l)/12;p.inertia=[i,i,p.mass*r*r/2];p.inverseInertia=p.inertia.map(x=>1/Math.max(1e-12,x));}
  const energy=p=>.5*p.mass*V.dot(p.v,p.v);
  function rotationalEnergy(p){const w=M.inverseRotate(p.orientation,p.angularVelocity);return .5*w.reduce((s,x,i)=>s+x*x*p.inertia[i],0);}
  function balanceError(p){const l=p.energyLedger;return p.initialEnergy+(p.initialRotationalEnergy||0)+l.flightWork+l.rotationWork+l.targetWork+l.explosiveEnergy-energy(p)-rotationalEnergy(p)-l.materialWork-l.contactLoss-l.fragmentEnergy-l.unrepresentedEnergy-l.blastEnergy;}
  R.ProjectileState={create,snapshot,transition,integrate,breakupPlan,fragment,refreshInertia,energy,rotationalEnergy,balanceError};
})();
