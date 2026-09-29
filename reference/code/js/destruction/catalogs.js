(function(){
  'use strict';
  const R=globalThis.RTS=globalThis.RTS||{};
  const freeze=o=>{for(const v of Object.values(o))if(v&&typeof v==='object')freeze(v);return Object.freeze(o);};
  // SI units. All numeric ammunition/strength values are GAME MODEL parameters;
  // manufacturer references establish weapon families, not a certified firing table.
  R.DestructionWeapons=freeze({
    minimi:{id:'minimi',name:'FN MINIMI · 5,56×45',caliber:.00556,rpm:800,capacity:200,reload:5,ammo:['556-ball'],length:.8},
    mag:{id:'mag',name:'FN MAG · 7,62×51',caliber:.00762,rpm:650,capacity:100,reload:6,ammo:['762-ball'],length:1.1},
    pistol:{id:'pistol',name:'Pistol · 9×19',caliber:.009,rpm:450,capacity:15,reload:2,ammo:['9mm-ball'],length:.2},
    m2:{id:'m2',name:'FN M2HB · 12,7×99',caliber:.0127,rpm:500,capacity:100,reload:7,ammo:['127-ball'],length:1.5},
    rh202:{id:'rh202',name:'Rheinmetall Rh202 · 20×139',caliber:.02,rpm:1000,capacity:60,reload:6,ammo:['20-ap','20-he'],length:1.7},
    mk44:{id:'mk44',name:'Mk44 Bushmaster II · 30×173',caliber:.03,rpm:200,capacity:40,reload:7,ammo:['30-ap','30-he'],length:1.8},
    bofors:{id:'bofors',name:'Bofors L/70 · 40 mm',caliber:.04,rpm:300,capacity:24,reload:7,ammo:['40-ap','40-he'],length:2},
    m68:{id:'m68',name:'M68 · 105 mm',caliber:.105,rpm:8,capacity:1,reload:5,ammo:['105-ap','105-he'],length:2.2}
  });
  const ammo={};
  // Kept mutable intentionally: caliber modules register variants after this
  // catalogue has loaded.  Old callers may still use every legacy field.
  R.registerDestructionAmmo=(entry)=>{if(!entry?.id)throw Error('ammunition requires id');ammo[entry.id]={...ammo[entry.id],...entry};return ammo[entry.id];};
  function add(id,mass,speed,diameter,kind,weapon,explosive=0){const fragments=explosive?({rh202:24,mk44:40,bofors:64,m68:128}[weapon]):0;R.registerDestructionAmmo({id,name:id.toUpperCase(),mass,diameter,dragDiameter:diameter,cd:.28,kind,caliberId:id.split('-')[0],strategyId:'legacy-'+kind,variantId:id.split('-').slice(1).join('-')||'ball',construction:kind==='ap'?'penetrator':kind==='he'?'he':'fmj',calibration:{contract:'ammunition-strategy-1',source:'legacy compatibility'},fuze:kind==='he'?{mode:'contact',armed:true}:null,velocity:{[weapon]:speed},explosive,fragments,fragmentation:explosive?{profile:'game-he-'+id,fragmentCount:fragments,bodyMassFraction:.30,explosiveEnergyFraction:.25,massSpread:.20,radialVelocityModel:'energy-budget'}:null});}
  add('9mm-ball',.008,360,.009,'ball','pistol');add('556-ball',.004,915,.00556,'ball','minimi');add('762-ball',.0095,840,.00762,'ball','mag');add('127-ball',.046,890,.0127,'ball','m2');
  add('20-ap',.10,1100,.014,'ap','rh202');add('20-he',.12,1050,.02,'he','rh202',.012);
  add('30-ap',.23,1385,.015,'ap','mk44');add('30-he',.36,1080,.03,'he','mk44',.04);
  add('40-ap',.5,1250,.022,'ap','bofors');add('40-he',.96,1000,.04,'he','bofors',.12);
  add('105-ap',3.5,1450,.028,'ap','m68');add('105-he',15,730,.105,'he','m68',2.1);
  // Narrow gameplay calibration for the published 4 g SS109 / hard steel case.
  // It does not apply to other ball calibres or other target materials.
  R.registerDestructionAmmo({id:'556-ball',ricochetProfile:{materials:{steel:{tangentRetention:.99}}}});
  R.DestructionAmmo=ammo;
  R.DestructionMaterials=freeze({
    concrete:{density:2400,strength:45e6,penetrationWork:600e6,toughness:80000,ricochet:.18,response:'brittle',spallThreshold:150},
    brick:{density:1800,strength:16e6,penetrationWork:160e6,toughness:40000,ricochet:.12,response:'brittle',spallThreshold:80},
    steel:{density:7850,strength:600e6,penetrationWork:10e9,toughness:250000,ricochet:.3,response:'ductile'},
    glass:{density:2500,strength:5e6,penetrationWork:20e6,toughness:700,ricochet:.04,response:'brittle',spallThreshold:0},
    wood:{density:650,strength:8e6,penetrationWork:40e6,toughness:12000,ricochet:.08,response:'fibrous'},
    foliage:{density:100,strength:1e4,penetrationWork:1e4,toughness:30,ricochet:0,response:'soft'},
    rock:{density:2700,strength:100e6,penetrationWork:800e6,toughness:180000,ricochet:.24,response:'brittle',spallThreshold:300},
    tissue:{density:1000,strength:.6e6,penetrationWork:.6e6,toughness:2200,ricochet:0,response:'soft'},
    armor:{density:3000,strength:350e6,penetrationWork:8e9,toughness:100000,ricochet:.2,response:'ductile'}
  });
})();
