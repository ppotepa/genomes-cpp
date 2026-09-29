(function(){
  'use strict';
  const R=window.RTS=window.RTS||{};
  // These are art-directed habitat-scale presets, not botanical growth measurements.
  const rows=[
    ['oak','Dąb','Quercus robur','broad',18, .68,.30,.52,0x66503b,0x537634,0xb68b36,'acorn'],
    ['beech','Buk','Fagus sylvatica','broad',21,.50,.28,.72,0x858078,0x47752e,0xb96a29,'nut'],
    ['birch','Brzoza','Betula pendula','airy',18,.36,.30,.85,0xd7d1ba,0x729648,0xd9b735,'none'],
    ['maple','Klon','Acer platanoides','broad',16,.62,.30,.68,0x716150,0x638539,0xcf782b,'none'],
    ['linden','Lipa','Tilia cordata','broad',20,.55,.25,.68,0x71634a,0x628636,0xd0ad39,'none'],
    ['ash','Jesion','Fraxinus excelsior','airy',22,.48,.38,.78,0x807767,0x688543,0xbba641,'none'],
    ['willow','Wierzba płacząca','Salix × sepulcralis','weeping',15,.76,.28,.25,0x776348,0x829750,0xc2ad49,'none'],
    ['poplar','Topola kolumnowa','Populus nigra italica','column',25,.19,.16,1.35,0x807c67,0x66864a,0xc7ab38,'none'],
    ['rowan','Jarząb','Sorbus aucuparia','airy',9,.52,.28,.65,0x887b6e,0x658840,0xc06b31,'cluster'],
    ['apple','Jabłoń','Malus domestica','orchard',6,.83,.24,.45,0x78614b,0x56803c,0xc5a746,'apple'],
    ['pear','Grusza','Pyrus communis','orchard',8,.49,.22,.90,0x6a5846,0x527338,0xc39b40,'pear'],
    ['cherry','Czereśnia','Prunus avium','orchard',11,.58,.28,.68,0x7b5146,0x597b3b,0xbd713c,'cherry'],
    ['pine','Sosna','Pinus sylvestris','pine',22,.49,.56,.35,0x996746,0x4d7150,0x4d7150,'cone'],
    ['spruce','Świerk','Picea abies','conifer',22,.34,.08,.03,0x715644,0x345c46,0x345c46,'cone'],
    ['fir','Jodła','Abies alba','conifer',25,.32,.10,.16,0x837d6b,0x3c6954,0x3c6954,'cone'],
    ['larch','Modrzew','Larix decidua','conifer',23,.36,.16,.08,0x79624b,0x7c9a49,0xd4ab37,'cone'],
    ['hazel','Leszczyna','Corylus avellana','shrub',4.5,.86,.10,.85,0x84705a,0x668238,0xbbaa41,'nut'],
    ['hawthorn','Głóg','Crataegus monogyna','shrub',4,.85,.10,.55,0x766453,0x517b37,0xac862e,'haw'],
    ['rose','Dzika róża','Rosa canina','shrub',2.1,1.12,.08,.20,0x6b7144,0x607a3d,0xb7a340,'hip'],
    ['elder','Bez czarny','Sambucus nigra','shrub',4.7,.93,.08,.65,0x8a7e66,0x56833f,0xb0a046,'elder']
  ];
  const entries={};
  rows.forEach(a=>{const [id,name,latin,form,height,width,crownBase,rise,bark,leaf,autumn,fruit]=a;entries[id]=Object.freeze({id,name,latin,form,height,width,crownBase,rise,bark,leaf,autumn,fruit,evergreen:['pine','spruce','fir'].includes(id),needle:['pine','spruce','fir','larch'].includes(id)});});
  R.EnvironmentCatalog=Object.freeze({categories:Object.freeze([{id:'plants',name:'Rośliny',available:true},{id:'rocks',name:'Kamienie i skały',available:true},{id:'props',name:'Znaki i obiekty',available:false}]),plants:Object.freeze(entries)});
  const clamp=(n,a,b)=>Math.max(a,Math.min(b,n));
  R.PlantGenome={version:'plants-1.0.0',create(species,seed,overrides={}){
    if(!entries[species])throw Error('Unknown plant species: '+species);
    const rng=new R.SeededRandom(Number(seed)>>>0),genes={};
    for(const key of ['stature','spread','branching','crookedness','foliage','leafSize','fruiting'])genes[key]=clamp(Number.isFinite(overrides[key])?overrides[key]:.25+rng.next()*.5,0,1);
    // Additive genes: old DNA retains its original trunk and complete branches.
    for(const [key,fallback] of [['girth',.5],['damage',0]])genes[key]=clamp(Number.isFinite(overrides[key])?overrides[key]:fallback,0,1);
    return {version:this.version,category:'plants',species,seed:Number(seed)>>>0,genes};
  }};
})();
