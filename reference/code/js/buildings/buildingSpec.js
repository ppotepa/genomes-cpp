(function(){
'use strict';
const B=globalThis.RTS.Buildings;
const freeze=o=>Object.freeze(o),range=(min,max)=>freeze([min,max]);
const PRESETS=freeze({
  FAMILY_HOUSE:freeze({label:'Family house',footprints:['RECT','L','T','U'],programs:['FAMILY_HOME'],roofs:['GABLE','HIP','HALF_HIP','COMPOUND_GABLE'],stairs:['STRAIGHT','QUARTER_TURN','DOGLEG'],structures:['MASONRY','TIMBER_FRAME'],width:range(8,14),depth:range(7,13),storeys:range(1,3),floorHeight:range(2.7,3.15),material:'stucco'}),
  FARMHOUSE:freeze({label:'Farmhouse',footprints:['RECT','L','T','COMPOSITE','IRREGULAR_ORTHO'],programs:['FARM_SERVICE_HOME','FAMILY_HOME'],roofs:['GABLE','HALF_HIP','COMPOUND_GABLE'],stairs:['STRAIGHT','DOGLEG'],structures:['MASONRY','TIMBER_FRAME'],width:range(9,17),depth:range(8,14),storeys:range(1,3),floorHeight:range(2.65,3.1),material:'brick'}),
  MULTI_FAMILY:freeze({label:'Multi-family housing',footprints:['RECT','L','U','COURTYARD'],programs:['CORRIDOR_APARTMENT_FLOOR','APARTMENT_UNIT'],roofs:['FLAT','HIP','MANSARD'],stairs:['DOGLEG','OPEN_WELL'],structures:['REINFORCED_CONCRETE','MASONRY','PRECAST_CONCRETE'],width:range(14,28),depth:range(11,22),storeys:range(2,5),floorHeight:range(2.75,3.2),material:'brick'}),
  OFFICE:freeze({label:'Office',footprints:['RECT','L','U','H'],programs:['SMALL_OFFICE','OPEN_OFFICE'],roofs:['FLAT','SHED'],stairs:['DOGLEG','OPEN_WELL','QUARTER_TURN'],structures:['REINFORCED_CONCRETE','STEEL_FRAME'],width:range(12,30),depth:range(10,22),storeys:range(1,5),floorHeight:range(3,3.6),material:'concrete'}),
  WORKSHOP_HALL:freeze({label:'Workshop / hall',footprints:['RECT','L','T','COMPOSITE'],programs:['WORKSHOP'],roofs:['GABLE','SHED','FLAT'],stairs:['STRAIGHT','QUARTER_TURN'],structures:['STEEL_FRAME','REINFORCED_CONCRETE'],width:range(12,34),depth:range(10,26),storeys:range(1,2),floorHeight:range(3.5,5.8),material:'metal'}),
  BARN:freeze({label:'Barn',footprints:['RECT','T','COMPOSITE'],programs:['BARN_STORAGE'],roofs:['GABLE','HALF_HIP'],stairs:['STRAIGHT'],structures:['TIMBER_FRAME','STEEL_FRAME'],width:range(8,24),depth:range(7,20),storeys:range(1,2),floorHeight:range(3.2,5.5),material:'wood'}),
  RURAL_SHED:freeze({label:'Rural shed',footprints:['RECT','L'],programs:['BARN_STORAGE'],roofs:['GABLE','SHED'],stairs:['STRAIGHT'],structures:['TIMBER_FRAME'],width:range(2.5,7),depth:range(2.5,6),storeys:range(1,1),floorHeight:range(2.3,3),material:'wood'})
});
const FAMILIES=freeze(['RECT','L','T','U','H','Z','COURTYARD','COMPOSITE','IRREGULAR_ORTHO','EXPLICIT']);
const ROOFS=freeze(['FLAT','GABLE','HIP','SHED','COMPOUND_GABLE','HALF_HIP','MANSARD']);
const STAIRS=freeze(['STRAIGHT','QUARTER_TURN','DOGLEG','OPEN_WELL']);
const SEED_DOMAINS=freeze(['preset','footprint','program','storeys','structure','stairs','facade','roof','furniture']);
const explicit=v=>v!==undefined&&v!==null&&v!=='AUTO';
const pick=(rng,values)=>values[Math.min(values.length-1,Math.floor(rng.next()*values.length))];
const number=(value,fallback,name,min,max,integer=false)=>{const v=explicit(value)?Number(value):fallback;if(!Number.isFinite(v)||v<min||v>max||(integer&&!Number.isInteger(v)))throw new TypeError(`${name} must be ${integer?'an integer ':''}between ${min} and ${max}`);return v;};
function resolveSeeds(seed,input={}){const result={};for(const domain of SEED_DOMAINS)result[domain]=Number.isFinite(input[domain])?input[domain]>>>0:B.deriveSeed(seed,domain);return result;}
function choose(options,key,values,rng){if(explicit(options[key])){if(!values.includes(options[key]))throw new TypeError(`${key} must be one of ${values.join(', ')}`);return options[key];}return pick(rng,values);}
B.createBuildingSpec=function(options={}){
  const seed=(Number.isFinite(options.seed)?options.seed:1337)>>>0,seeds=resolveSeeds(seed,options.seeds),presetIds=Object.keys(PRESETS),presetId=choose(options,'presetId',presetIds,new B.RNG(seeds.preset)),preset=PRESETS[presetId];
  const rng={};for(const domain of SEED_DOMAINS)rng[domain]=new B.RNG(seeds[domain]);
  const width=number(options.width,+rng.footprint.range(...preset.width).toFixed(3),'width',2,80),depth=number(options.depth,+rng.footprint.range(...preset.depth).toFixed(3),'depth',2,80);
  const requestedStoreys=options.storeyCount??(Number.isFinite(options.storeys)?options.storeys:options.storeys?.count),requestedFloorHeight=options.floorHeight??options.storeys?.floorHeight;
  const storeyCount=number(requestedStoreys,rng.storeys.int(...preset.storeys),'storeyCount',1,5,true),floorHeight=number(requestedFloorHeight,+rng.storeys.range(...preset.floorHeight).toFixed(3),'floorHeight',2.3,6);
  const footprintFamily=explicit(options.footprint)?'EXPLICIT':choose(options,'footprintFamily',preset.footprints,rng.footprint);
  const roomProgram=choose(options,'roomProgram',preset.programs,rng.program),roofFamily=choose(options,'roofFamily',preset.roofs,rng.roof);
  const stairFamily=storeyCount>1?choose(options,'stairFamily',preset.stairs,rng.stairs):null,structuralSystem=choose(options,'structuralSystem',preset.structures,rng.structure);
  const features=freeze({...options.features}),material=explicit(options.material)?options.material:preset.material;
  const rural=/FARM|BARN|SHED/.test(presetId),industrial=/OFFICE|WORKSHOP/.test(presetId);
  const facadeInput=options.facade||{},facade=freeze({
    style:explicit(facadeInput.style)?choose(facadeInput,'style',['CLASSIC','MODERN','RUSTIC','INDUSTRIAL'],rng.facade):rural?'RUSTIC':industrial?'INDUSTRIAL':'CLASSIC',
    windowType:explicit(facadeInput.windowType)?choose(facadeInput,'windowType',B.WINDOW_TYPES,rng.facade):industrial?'INDUSTRIAL':rural?'SMALL':'SASH',
    doorType:explicit(facadeInput.doorType)?choose(facadeInput,'doorType',B.DOOR_TYPES,rng.facade):industrial?'STEEL':'PANEL',
    bayWidth:number(facadeInput.bayWidth,+rng.facade.range(2.4,3.6).toFixed(3),'facade.bayWidth',1.5,6)
  });
  const roofCovering=explicit(options.roofCovering)?choose(options,'roofCovering',Object.keys(B.ROOF_COVERINGS),rng.roof):roofFamily==='FLAT'?'membrane':industrial?'metal':rural?'shingle':'clay';
  return freeze({schema:'rts.building-spec/6',seed,seeds,presetId,footprintFamily,explicitFootprint:options.footprint?B.Polygon.canonical(options.footprint):null,width,depth,roomProgram,roofFamily,roofCovering,stairFamily,storeys:freeze({count:storeyCount,floorHeight,setbacks:freeze((options.setbacks||[]).slice()),partialTop:options.partialTop??1,attic:!!options.attic}),structuralSystem,features,material,furniture:options.furniture!==false,facade,agentProfile:B.AGENT_PROFILE});
};
B.BUILDING_PRESETS=PRESETS;B.FOOTPRINT_FAMILIES=FAMILIES;B.ROOF_FAMILIES=ROOFS;B.STAIR_FAMILIES=STAIRS;B.BUILDING_SEED_DOMAINS=SEED_DOMAINS;
})();
