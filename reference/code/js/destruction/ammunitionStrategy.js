(function(){
  'use strict';
  const R=globalThis.RTS=globalThis.RTS||{}, clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  const registry=new Map();
  class CaliberStrategy{
    constructor(spec={}){Object.assign(this,{id:'caliber',caliberId:'unknown',kind:'ball',construction:'fmj',dragScale:1,penetration:1,deformation:.35,fragmentation:0},spec);}
    registerVariant(variant){const ammo={...variant,caliberId:variant.caliberId||this.caliberId,strategyId:this.id,variantId:variant.variantId||variant.id,construction:variant.construction||this.construction,calibration:{contract:'ammunition-strategy-1',...(this.calibration||{}),...(variant.calibration||{})}};R.registerDestructionAmmo(ammo);return ammo;}
    aerodynamic(projectile){return {cd:(projectile.cd||.28)*this.dragScale,stabilityLoss:Math.max(0,1-projectile.stability)*.002};}
    contact(projectile,part,contact){return {kind:this.kind,penetration:this.penetration,incidence:contact.incidence??1,ricochetBias:this.ricochetBias||0};}
    damageBudget(args){const base=R.MaterialImpact.budget({...args,ammoKind:this.kind});const momentum=Math.sqrt(Math.max(0,2*(args.entryEnergy||0)*(args.mass||0))),area=Math.PI*Math.max(1e-9,args.diameter||.01)**2/4,section=clamp((this.referenceArea||area)/area,.55,2.3),factor=this.penetration*section;
      base.strategyId=this.id;base.variantId=args.ammo?.variantId;base.caliberId=this.caliberId;base.construction=args.ammo?.construction||this.construction;base.penetrationFactor=factor;base.localWork=(base.structuralEnergy||0)*factor;return base;}
    shouldDetonate(projectile){return false;}
    fragments(){return null;}
  }
  class KineticCaliberStrategy extends CaliberStrategy{}
  class ExplosiveCaliberStrategy extends CaliberStrategy{constructor(spec){super({...spec,kind:'he'});}shouldDetonate(p){return !!p.ammo.fuze?.armed&&p.ammo.fuze.mode==='contact';}}
  class FragmentStrategy extends CaliberStrategy{constructor(spec={}){super({id:'fragment',kind:'fragment',construction:'fragment',penetration:.55,dragScale:1.8,...spec});}}
  function register(strategy){registry.set(strategy.id,strategy);return strategy;}
  function forAmmo(ammo){return registry.get(ammo?.strategyId)||registry.get('legacy-'+ammo?.kind)||registry.get('fragment')||new KineticCaliberStrategy({id:'fallback',kind:ammo?.kind||'ball'});}
  register(new KineticCaliberStrategy({id:'legacy-ball',caliberId:'legacy',kind:'ball'}));register(new KineticCaliberStrategy({id:'legacy-ap',caliberId:'legacy',kind:'ap',construction:'penetrator',penetration:1.25}));register(new ExplosiveCaliberStrategy({id:'legacy-he',caliberId:'legacy'}));register(new FragmentStrategy());
  R.DestructionAmmunitionStrategies={contract:'ammunition-strategy-1',registry,register,forAmmo,CaliberStrategy,KineticCaliberStrategy,ExplosiveCaliberStrategy,FragmentStrategy};
})();
