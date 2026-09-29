(function(){
  'use strict';
  const R=globalThis.RTS,V=R.DestructionSolid.V,P=R.DestructionImpactProfiles,MaterialImpact=R.MaterialImpact;
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  function hash(seed,text){
    let h=(2166136261^(seed>>>0))>>>0;for(const c of String(text))h=Math.imul(h^c.charCodeAt(0),16777619)>>>0;
    h^=h>>>16;h=Math.imul(h,0x7feb352d);h^=h>>>15;h=Math.imul(h,0x846ca68b);h^=h>>>16;return (h>>>0)/4294967296;
  }
  function orientedNormal(direction,normal){return V.dot(direction,normal)>0?V.mul(normal,-1):normal.slice();}
  function perpendicular(a,b){
    let p=V.cross(a,b);if(V.length(p)<1e-8)p=V.cross(a,Math.abs(a[1])<.8?[0,1,0]:[1,0,0]);return V.unit(p);
  }
  function perturb(direction,normal,amount,seed,key){
    if(amount<=0)return V.unit(direction);
    const side=perpendicular(direction,normal),other=V.unit(V.cross(direction,side)),a=(hash(seed,key+'/a')*2-1)*amount,b=(hash(seed,key+'/b')*2-1)*amount*.55;
    return V.unit(V.add(direction,V.add(V.mul(side,a),V.mul(other,b))));
  }
  function profile(projectile,part){const materialId=MaterialImpact?.dominantMaterial(part)||part.material;return {materialId,material:P.materials[materialId]||P.materials[part.material]||P.materials.concrete,ammo:P.ammunition[projectile.ammo.kind]||P.ammunition.ball};}
  function grainBias(direction,part,normal,amount){
    if(!Array.isArray(part?.grainDirection)||amount<=0)return V.unit(direction);
    const g=V.sub(part.grainDirection,V.mul(normal,V.dot(part.grainDirection,normal)));if(V.length(g)<1e-8)return V.unit(direction);
    const grain=V.unit(g),sign=V.dot(direction,grain)>=0?1:-1;return V.unit(V.add(direction,V.mul(grain,sign*amount)));
  }
  function surface(projectile,part,normal,direction,energy){
    const n=orientedNormal(direction,normal),incidence=clamp(-V.dot(direction,n),0,1),{material,materialId,ammo}=profile(projectile,part),rough=(hash(projectile.seed,part.id+'/'+projectile.impactIndex)-.5)*2*material.roughness*.035;
    const threshold=clamp((R.DestructionMaterials[part.material]?.ricochet||.1)*ammo.ricochetScale+material.ricochetBias+rough,.015,.72),zone=P.transitionWidth;
    if(energy<P.minRicochetEnergy||projectile.ricochetCount>=P.maxRicochets||incidence>threshold+zone)return {ricochet:false,incidence,threshold,normal:n};
    const tangent=V.sub(direction,V.mul(n,V.dot(direction,n))),tangentDir=V.length(tangent)>1e-8?V.unit(tangent):perpendicular(direction,n);
    const blend=clamp((threshold+zone-incidence)/(zone*2),0,1),normalRest=material.normalRestitution*(.55+.45*blend),calibratedTangent=projectile.ammo.ricochetProfile?.materials?.[materialId]?.tangentRetention,tangentRest=(calibratedTangent??material.tangentRetention)*(.82+.18*projectile.integrity);
    const out=V.unit(V.add(V.mul(tangentDir,tangentRest),V.mul(n,Math.max(.02,incidence*normalRest))));
    const retention=clamp(tangentRest*tangentRest*(1-incidence*incidence)+normalRest*normalRest*incidence*incidence,.04,.92);
    const retainedEnergy=energy*retention,absorbed=1-retention,damage=material.projectileDamage*ammo.deformationScale*absorbed;
    const jitter=material.roughness*(.012+.045*(1-projectile.stability))*(1+.4*projectile.deformation);
    let directionOut=perturb(out,n,jitter,projectile.seed,part.id+'/'+projectile.impactIndex+'/ric');
    directionOut=grainBias(directionOut,part,n,Math.min(.05,(1-incidence)*.035+absorbed*.012));
    const axis=perturb(directionOut,n,jitter*(1.8+ammo.deflectionScale),projectile.seed,part.id+'/'+projectile.impactIndex+'/axis');
    return {ricochet:true,incidence,threshold,normal:n,direction:directionOut,axis,energy:retainedEnergy,lost:energy-retainedEnergy,
      integrityLoss:damage,deformation:damage*.75,stabilityLoss:clamp(.10+absorbed*.38+material.roughness*.12,0,.65),diameterGrowth:damage*.12,dragGrowth:damage*.25,massLoss:damage*.035};
  }
  function edge(projectile,part,normal,direction,energy){
    const n=orientedNormal(direction,normal),reflected=V.unit(V.sub(direction,V.mul(n,2*V.dot(direction,n)))),{material,ammo}=profile(projectile,part);
    const retained=energy*clamp(.68+material.tangentRetention*.20,0,.9),jitter=material.roughness*(.018+.025*(1-projectile.stability));
    let directionOut=perturb(reflected,n,jitter,projectile.seed,part.id+'/'+projectile.impactIndex+'/edge');directionOut=grainBias(directionOut,part,n,.035);const damage=material.projectileDamage*ammo.deformationScale*(1-retained/Math.max(1e-9,energy))*.65;
    return {ricochet:true,edge:true,normal:n,direction:directionOut,axis:perturb(directionOut,n,jitter*1.8,projectile.seed,part.id+'/'+projectile.impactIndex+'/edge-axis'),energy:retained,lost:energy-retained,
      integrityLoss:damage,deformation:damage*.55,stabilityLoss:.14+material.roughness*.10,diameterGrowth:damage*.08,dragGrowth:damage*.18,massLoss:damage*.02};
  }
  function penetration(projectile,part,normal,direction,entryEnergy,residualEnergy){
    const n=orientedNormal(direction,normal),incidence=clamp(-V.dot(direction,n),0,1),loss=clamp(1-residualEnergy/Math.max(1e-9,entryEnergy),0,1),{material,ammo}=profile(projectile,part);
    const oblique=1-incidence,baseAngle=material.deflection*ammo.deflectionScale*oblique*Math.sqrt(loss)*(1+.65*projectile.deformation),key=part.id+'/'+projectile.impactIndex+'/pen';
    let directionOut=perturb(direction,n,baseAngle,projectile.seed,key);
    const grain=MaterialImpact?.grainAxis(part,directionOut);if(grain){const sign=hash(projectile.seed,key+'/grain')<.5?-1:1,bias=Math.min(.065,.008+baseAngle*.72+loss*.012);directionOut=V.unit(V.add(directionOut,V.mul(grain,sign*bias)));}
    const axis=perturb(directionOut,n,baseAngle*(1.7+(1-projectile.stability)),projectile.seed,key+'/axis');
    const damage=material.projectileDamage*ammo.deformationScale*loss*(.45+.55*oblique);
    return {direction:directionOut,axis,energy:residualEnergy,incidence,normal:n,integrityLoss:damage*.55,deformation:damage*.65,
      stabilityLoss:clamp(loss*.16+baseAngle*2.8,0,.45),diameterGrowth:damage*.08,dragGrowth:damage*.18,massLoss:damage*.025};
  }
  function stop(projectile,part,normal,direction,entryEnergy){
    const {material,ammo}=profile(projectile,part),damage=material.projectileDamage*ammo.deformationScale*.55;
    return {direction:[0,0,0],axis:projectile.axis,energy:0,integrityLoss:damage,deformation:damage,stabilityLoss:.5,diameterGrowth:damage*.15,dragGrowth:damage*.2,massLoss:damage*.04,normal:orientedNormal(direction,normal)};
  }
  function contact(projectile,part,options={}){
    const velocity=options.relativeVelocity||projectile.v,direction=V.unit(velocity),energy=Math.max(0,options.entryEnergy??.5*projectile.mass*V.dot(velocity,velocity)),residual=Math.max(0,options.residualEnergy??energy),mode=options.mode||(residual<=1e-9?'stop':options.edge?'edge':options.penetrated?'penetration':'surface');
    const response=mode==='stop'?stop(projectile,part,options.normal,direction,energy):mode==='edge'?edge(projectile,part,options.normal,direction,energy):mode==='penetration'?penetration(projectile,part,options.normal,direction,energy,residual):surface(projectile,part,options.normal,direction,energy);
    if(mode==='surface'&&!response.ricochet)return response;
    // Deformation is represented without silently deleting projectile mass.
    response.massLoss=0;
    const M=R.ProjectileMath,target=options.targetVelocity||[0,0,0],point=options.contactPoint||options.point||projectile.p;
    let out=V.mul(response.direction||direction,Math.sqrt(2*(response.energy??residual)/projectile.mass));
    const impulse=V.mul(V.sub(out,velocity),projectile.mass),lever=V.sub(point,options.projectileCenter||V.sub(point,V.mul(projectile.axis,projectile.diameter))),torque=V.cross(lever,impulse),localTorque=M.inverseRotate(projectile.orientation,torque),delta=M.rotate(projectile.orientation,localTorque.map((x,i)=>x*projectile.inverseInertia[i]));
    const oldW=projectile.angularVelocity,oldRotation=R.ProjectileState.rotationalEnergy(projectile),newW=V.add(oldW,delta),localW=M.inverseRotate(projectile.orientation,newW),newRotation=.5*localW.reduce((sum,x,i)=>sum+x*x*projectile.inertia[i],0),budget=Math.max(0,mode==='penetration'?residual:energy-(response.energy??residual));
    const scale=newRotation>oldRotation+budget?Math.sqrt((oldRotation+budget)/Math.max(1e-12,newRotation)):1;
    response.angularVelocity=V.mul(newW,scale);response.rotationChange=newRotation*scale*scale-oldRotation;if(mode==='stop'){response.angularVelocity=[0,0,0];response.rotationChange=-oldRotation;}
    if(mode==='penetration'){response.energy=Math.max(0,residual-response.rotationChange);out=V.mul(response.direction,Math.sqrt(2*response.energy/projectile.mass));}
    const actualImpulse=V.mul(V.sub(out,velocity),projectile.mass);
    response.lost=Math.max(0,energy-(response.energy??residual)-response.rotationChange);
    response.relativeEnergy=response.energy??residual;response.outgoingVelocity=V.add(out,target);response.energy=.5*projectile.mass*V.dot(response.outgoingVelocity,response.outgoingVelocity);
    response.incomingVelocity=V.add(velocity,target);response.linearImpulse=actualImpulse;response.angularImpulse=torque;
    response.targetImpulse=V.mul(actualImpulse,-1);response.targetAngularImpulse=V.cross(V.sub(point,part.centerOfMass||point),response.targetImpulse);
    response.targetWork=V.dot(target,actualImpulse);response.orientation=projectile.orientation.slice();response.layerPath=options.layerIntervals||[];response.pathLength=options.pathLength||0;
    response.result=response.ricochet?'ricochet':mode==='penetration'?'penetrated':mode==='stop'?'stopped':'contact';return response;
  }
  R.DestructionImpactSolver={surface,edge,penetration,stop,contact,orientedNormal,perturb};
})();
