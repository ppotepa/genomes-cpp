(function(){
  'use strict';
  const R=globalThis.RTS,B=R.Buildings;
  R.ProceduralBuildingGenerator=class ProceduralBuildingGenerator{
    constructor(options={}){this.options={...options};}
    static get presets(){return B.BUILDING_PRESETS;} static get footprintFamilies(){return B.FOOTPRINT_FAMILIES;}
    static get roomPrograms(){return B.ROOM_PROGRAMS;} static get roofFamilies(){return B.ROOF_FAMILIES;}
    static get stairFamilies(){return B.STAIR_FAMILIES;} static get structuralSystems(){return B.STRUCTURAL_SYSTEMS;}
    createSpec(options={}){return B.createBuildingSpec({...this.options,...options});}
    createPlan(options={}){const value=options.spec||(options.schema==='rts.building-spec/6'?options:this.createSpec(options));if(!B.buildingBackendsReady())throw new B.BuildingGenerationError('BACKENDS_NOT_INITIALIZED','preload',value.schema==='rts.building-spec/6'?value:null);return B.createPlan(value);}
    generate(options={}){return B.buildGeometry(this.createPlan(options),options);}
    createWorldRepresentation(options={},renderOptions={}){const plan=options.plan||this.createPlan(options);const representation=new R.BuildingWorldRepresentation(plan,{...renderOptions,runtimeId:renderOptions.runtimeId});representation.plan=plan;return representation;}
  };
})();
