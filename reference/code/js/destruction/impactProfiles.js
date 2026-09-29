(function(){
  'use strict';
  const R=globalThis.RTS=globalThis.RTS||{};
  const freeze=o=>{for(const v of Object.values(o))if(v&&typeof v==='object'&&!Object.isFrozen(v))freeze(v);return Object.freeze(o);};
  // Gameplay contact profiles. They intentionally describe material response classes,
  // not certified armour/munition performance.
  R.DestructionImpactProfiles=freeze({
    materials:{
      concrete:{roughness:.26,normalRestitution:.10,tangentRetention:.72,ricochetBias:-.015,deflection:.045,projectileDamage:.20},
      brick:{roughness:.34,normalRestitution:.08,tangentRetention:.66,ricochetBias:-.025,deflection:.065,projectileDamage:.24},
      steel:{roughness:.08,normalRestitution:.34,tangentRetention:.88,ricochetBias:.035,deflection:.030,projectileDamage:.14},
      glass:{roughness:.04,normalRestitution:.06,tangentRetention:.76,ricochetBias:-.045,deflection:.020,projectileDamage:.05},
      wood:{roughness:.22,normalRestitution:.06,tangentRetention:.64,ricochetBias:-.035,deflection:.055,projectileDamage:.16},
      foliage:{roughness:.5,normalRestitution:.02,tangentRetention:.70,ricochetBias:-.12,deflection:.10,projectileDamage:.02},
      rock:{roughness:.31,normalRestitution:.16,tangentRetention:.69,ricochetBias:.005,deflection:.060,projectileDamage:.28},
      tissue:{roughness:.12,normalRestitution:.01,tangentRetention:.55,ricochetBias:-.15,deflection:.035,projectileDamage:.02},
      armor:{roughness:.06,normalRestitution:.30,tangentRetention:.90,ricochetBias:.045,deflection:.025,projectileDamage:.18}
    },
    ammunition:{
      ball:{ricochetScale:1.08,integrity:1,hardness:.55,deflectionScale:1.10,deformationScale:1.0,breakupIntegrity:.16,breakupStability:.08,maxSecondaryFragments:2},
      ap:{ricochetScale:.88,integrity:1,hardness:1,deflectionScale:.72,deformationScale:.55,breakupIntegrity:.20,breakupStability:.13,maxSecondaryFragments:4},
      he:{ricochetScale:.95,integrity:.72,hardness:.45,deflectionScale:1.0,deformationScale:1.25,breakupIntegrity:0,breakupStability:0,maxSecondaryFragments:0},
      fragment:{ricochetScale:1.18,integrity:.45,hardness:.35,deflectionScale:1.35,deformationScale:1.4,breakupIntegrity:0,breakupStability:0,maxSecondaryFragments:0}
    },
    transitionWidth:.055,
    minRicochetEnergy:75,
    maxRicochets:6
  });
})();