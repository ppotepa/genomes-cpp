(function () {
  'use strict';
  const R = globalThis.RTS = globalThis.RTS || {};
  const B = R.Buildings = R.Buildings || {};
  const STYLE_CATALOG = Object.freeze({
    STUCCO_VILLA: Object.freeze({
      id:'STUCCO_VILLA', label:'Dom tynkowany', material:'stucco',
      floors:[1,2], width:[7.4,12.5], depth:[6.6,11.0], floorHeight:[2.75,3.05],
      roofs:['HIP','GABLE'], window:'WIDE', door:'GLAZED', stair:'DOGLEG',
      palette:{wall:[0xd0c4a5,0xc8b99a,0xd8ceb5],trim:0xe8dfca,roof:0x765c4d,frame:0x52463b,door:0x695343,glass:0x91acb0}
    }),
    INDUSTRIAL_WORKSHOP: Object.freeze({
      id:'INDUSTRIAL_WORKSHOP', label:'Warsztat / magazyn', material:'concrete',
      floors:[1,2], width:[9.0,15.0], depth:[8.0,14.0], floorHeight:[3.15,3.65],
      roofs:['FLAT','SHED'], window:'INDUSTRIAL', door:'STEEL', stair:'DOGLEG',
      palette:{wall:[0x8d918b,0x9b9b91,0x787d79],trim:0x626966,roof:0x555b59,frame:0x333a3b,door:0x4a504f,glass:0x819ca0}
    }),
    RURAL_SHED: Object.freeze({
      id:'RURAL_SHED', label:'Szopka gospodarcza', material:'wood',
      floors:[1,1], width:[2,3], depth:[2,3], floorHeight:[2.35,2.55],
      roofs:['GABLE','SHED'], window:'SMALL', door:'PANEL', stair:'NONE',
      palette:{wall:[0x806343,0x94734e,0x6f5239],trim:0x4e3828,roof:0x58605d,frame:0x4b3323,door:0x6b492f,glass:0x86a7a8}
    }),
  });

  const STAIR_TYPES = Object.freeze(['STRAIGHT','QUARTER_TURN','DOGLEG','OPEN_WELL']);
  const ROOF_TYPES = Object.freeze(['GABLE','HIP','FLAT','SHED']);
  const WALL_MATERIALS = Object.freeze(['brick','wood','stucco','concrete','stone','metal']);
  const STRUCTURAL_SYSTEMS = Object.freeze({
    MASONRY:{label:'Mur nośny',loadBearingWalls:true,wallThickness:.30,wallHp:145,section:.18,maxBay:4.2,color:0x51443b,material:'structure-masonry'},
    TIMBER_LOG:{label:'Konstrukcja zrębowa',loadBearingWalls:true,wallThickness:.24,wallHp:110,section:.2,maxBay:3.6,color:0x594632,material:'wood'},
    TIMBER_FRAME:{label:'Szkielet drewniany',loadBearingWalls:false,wallThickness:.20,wallHp:85,section:.16,maxBay:3.6,color:0x594632,material:'wood'},
    REINFORCED_CONCRETE:{label:'Szkielet żelbetowy',loadBearingWalls:false,wallThickness:.24,wallHp:190,section:.24,maxBay:5.4,color:0x85877f,material:'structure-concrete'},
    STEEL_FRAME:{label:'Rama stalowa',loadBearingWalls:false,wallThickness:.18,wallHp:165,section:.16,maxBay:7.2,color:0x454e50,material:'metal'},
    PRECAST_CONCRETE:{label:'Wielkopłytowe ściany nośne',loadBearingWalls:true,wallThickness:.24,wallHp:190,section:.22,maxBay:5.4,panelWidth:3.6,color:0x9a9b91,material:'structure-precast'}
  });
  const DEFAULT_STRUCTURE={EURO_HOUSE:'MASONRY',EURO_APARTMENT:'MASONRY',EURO_OFFICE:'REINFORCED_CONCRETE',EURO_HALL:'STEEL_FRAME',EURO_FARM_SHED:'TIMBER_FRAME'};
  const WINDOW_TYPES = Object.freeze(['TALL','SASH','WIDE','SMALL','INDUSTRIAL']);
  const DOOR_TYPES = Object.freeze(['PANEL','GLAZED','STEEL','DOUBLE']);
  const ROOF_COVERINGS = Object.freeze({
    clay:{label:'Dachówka ceramiczna',color:0x985c46,roughness:.88,metalness:0},
    slate:{label:'Łupek',color:0x515653,roughness:.96,metalness:0},
    shingle:{label:'Gont drewniany',color:0x645b4c,roughness:.94,metalness:0},
    metal:{label:'Blacha na rąbek',color:0x525b5d,roughness:.62,metalness:.24},
    corrugated:{label:'Blacha profilowana',color:0x747a75,roughness:.72,metalness:.18},
    membrane:{label:'Papa / membrana',color:0x45494a,roughness:.93,metalness:0},
    thatch:{label:'Strzecha',color:0x9a8356,roughness:1,metalness:0}
  });
  const MATERIAL_PALETTES = Object.freeze({
    brick:{colors:[0x815448,0x945f4d,0x75493e],roughness:.92,wallHp:145,thickness:.28},
    wood:{colors:[0x806e55,0x947d5d,0x706149],roughness:.9,wallHp:85,thickness:.20},
    stucco:{colors:[0xd0c4a5,0xc8b99a,0xd8ceb5],roughness:.88,wallHp:105,thickness:.23},
    concrete:{colors:[0x92958e,0xa1a197,0x777e7c],roughness:.93,wallHp:175,thickness:.28},
    stone:{colors:[0x918a79,0x807a6d,0x9c9482],roughness:.96,wallHp:190,thickness:.34},
    metal:{colors:[0x737b79,0x89908c,0x606967],roughness:.7,wallHp:115,thickness:.14},
    glass:{colors:[0x7f9da0,0x91aaad,0x6f898e],roughness:.3,wallHp:35,thickness:.12}
  });

  const FOOTPRINTS = Object.freeze({
    RECT:{core:[0,0,1,1],parts:[]},
    L:{core:[0,0,.65,1],parts:[[.65,0,.35,.6,'RIGHT']]},
    ANNEX_LEFT:{core:[.35,0,.65,1],parts:[[-.35,0,.35,.6,'LEFT']]},
    ANNEX_REAR:{core:[0,0,.65,1],parts:[[.65,0,.35,.6,'RIGHT']]},
    HALL_ANNEX_LEFT:{core:[.27,0,.73,1],parts:[[-.27,0,.27,.6,'LEFT']]},
    HALL_ANNEX_RIGHT:{core:[0,0,.73,1],parts:[[.73,0,.27,.6,'RIGHT']]}
  });

  Object.assign(B, {STYLE_CATALOG, STAIR_TYPES, ROOF_TYPES, WALL_MATERIALS, STRUCTURAL_SYSTEMS, DEFAULT_STRUCTURE, WINDOW_TYPES, DOOR_TYPES, ROOF_COVERINGS, MATERIAL_PALETTES, FOOTPRINTS});
})();
