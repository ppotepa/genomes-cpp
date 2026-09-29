(function () {
  'use strict';
  const R = window.RTS;

  // Inventory is simulation data. No THREE objects or generator functions are
  // stored here. Dimensions/weights are design values, not real item specs.
  R.EquipmentSlots = Object.freeze({
    head: {label:'Nakrycie głowy', group:'GŁOWA', socket:'HEAD'},
    face: {label:'Twarz', group:'GŁOWA', socket:'HEAD_FRONT'},
    neck: {label:'Szyja', group:'GŁOWA', socket:'NECK'},
    torsoBase: {label:'Bluza', group:'UBRANIE', required:'field_jacket'},
    legs: {label:'Spodnie', group:'UBRANIE', required:'field_pants'},
    feet: {label:'Buty', group:'UBRANIE', required:'combat_boots'},
    hands: {label:'Rękawiczki', group:'UBRANIE'},
    torsoArmor: {label:'Ochrona tułowia', group:'TUŁÓW', socket:'CHEST_CENTER'},
    chestRig: {label:'Oporządzenie', group:'TUŁÓW', socket:'CHEST_CENTER'},
    back: {label:'Plecak', group:'TUŁÓW', socket:'BACK_CENTER'},
    belt: {label:'Pas', group:'PAS I KIESZENIE', socket:'WAIST'},
    leftHip: {label:'Lewe biodro', group:'PAS I KIESZENIE', socket:'HIP_L'},
    rightHip: {label:'Prawe biodro', group:'PAS I KIESZENIE', socket:'HIP_R'},
    leftThigh: {label:'Lewe udo', group:'PAS I KIESZENIE', socket:'THIGH_L'},
    rightThigh: {label:'Prawe udo', group:'PAS I KIESZENIE', socket:'THIGH_R'},
    utility1: {label:'Przód pasa', group:'AKCESORIA', socket:'WAIST_FRONT'},
    utility2: {label:'Tył pasa', group:'AKCESORIA', socket:'WAIST_BACK'},
    utility3: {label:'Klatka / akcesorium', group:'AKCESORIA', socket:'CHEST_LEFT'},
    meleeWeapon: {label:'Nóż', group:'BROŃ', socket:'HIP_R'},
    throwable: {label:'Granat — rekwizyt', group:'BROŃ', socket:'WAIST_FRONT'},
    primaryWeapon: {label:'Broń główna', group:'BROŃ', socket:'WEAPON_BACK'},
    secondaryWeapon: {label:'Broń boczna', group:'BROŃ', socket:'WEAPON_HIP'}
  });
  const items = {};
  function add(id,label,slots,kind,weightKg,visual={}) {
    items[id]=Object.freeze({id,label,slots:Object.freeze(Array.isArray(slots)?slots:[slots]),kind,weightKg,visual:Object.freeze(visual)});
  }
  add('field_cap','Czapka polowa','head','cap',.16,{style:'cap',coverage:'cap'});
  add('patrol_cap','Patrolówka','head','cap',.19,{style:'patrol',coverage:'cap'});
  add('beanie','Czapka dzianinowa','head','cap',.12,{style:'beanie',coverage:'cap'});
  add('beret','Beret','head','cap',.13,{style:'beret',coverage:'cap'});
  add('boonie_hat','Kapelusz polowy','head','cap',.22,{style:'boonie',coverage:'cap'});
  add('helmet_light','Hełm lekki','head','helmet',.95,{style:'light',coverage:'helmet'});
  add('helmet_standard','Hełm standardowy','head','helmet',1.3,{style:'standard',coverage:'helmet'});
  add('helmet_heavy','Hełm z osłoną boków','head','helmet',1.7,{style:'heavy',coverage:'helmet'});
  add('helmet_cover','Hełm z pokrowcem','head','helmet',1.4,{style:'cover',coverage:'helmet'});
  add('glasses','Okulary','face','eyewear',.06,{style:'glasses'});
  add('goggles','Gogle','face','eyewear',.18,{style:'goggles'});
  add('balaclava','Kominiarka','face','mask',.10,{style:'balaclava'});
  add('respirator','Maska ochronna','face','mask',.36,{style:'respirator'});
  add('scarf','Szalik','neck','neckwear',.2,{style:'scarf'});
  add('neck_gaiter','Komin','neck','neckwear',.1,{style:'gaiter'});
  add('field_jacket','Bluza polowa','torsoBase','clothing',.7,{ease:1,style:'field'});
  add('combat_shirt','Bluza lekka','torsoBase','clothing',.45,{ease:.97,style:'combat'});
  add('winter_jacket','Kurtka ocieplana','torsoBase','clothing',1.35,{ease:1.08,style:'winter'});
  add('field_pants','Spodnie polowe','legs','clothing',.65,{ease:1});
  add('combat_pants','Spodnie z nakolannikami','legs','clothing',.8,{ease:1.025,pads:true});
  add('winter_pants','Spodnie ocieplane','legs','clothing',1.0,{ease:1.07});
  add('combat_boots','Buty standardowe','feet','clothing',1.2,{width:1,shaft:1});
  add('light_boots','Buty lekkie','feet','clothing',.9,{width:.96,shaft:.87});
  add('heavy_boots','Buty wzmocnione','feet','clothing',1.65,{width:1.07,shaft:1.08});
  add('winter_boots','Buty ocieplane','feet','clothing',1.5,{width:1.1,shaft:1.04});
  add('gloves_light','Rękawiczki bez palców','hands','clothing',.08,{style:'fingerless'});
  add('gloves_full','Rękawiczki pełne','hands','clothing',.12,{style:'full'});
  add('gloves_winter','Rękawiczki ocieplane','hands','clothing',.2,{style:'winter'});
  add('light_vest','Kamizelka lekka','torsoArmor','armor',2.5,{style:'light',thickness:.010});
  add('plate_carrier','Kamizelka płytowa','torsoArmor','armor',6.0,{style:'plate',thickness:.020});
  add('heavy_armor','Kamizelka ciężka','torsoArmor','armor',9.5,{style:'heavy',thickness:.029});
  add('webbing','Szelki i dwie kieszenie','chestRig','rig',.7,{count:2,style:'webbing'});
  add('chest_standard','Oporządzenie standardowe','chestRig','rig',1.0,{count:3,style:'standard'});
  add('chest_assault','Oporządzenie zwarte','chestRig','rig',1.2,{count:4,style:'assault'});
  add('chest_ammo','Duże ładownice','chestRig','rig',2.0,{count:3,style:'ammo'});
  add('chest_medical','Kieszenie medyczne','chestRig','rig',1.6,{count:2,style:'medical'});
  add('chest_tools','Kieszenie narzędziowe','chestRig','rig',1.9,{count:3,style:'tools'});
  add('pack_small','Plecak mały','back','pack',.7,{size:[.26,.32,.13],style:'small'});
  add('pack_medium','Plecak średni','back','pack',1.2,{size:[.32,.43,.19],style:'medium'});
  add('pack_large','Plecak duży','back','pack',1.8,{size:[.37,.53,.22],style:'large',roll:true});
  add('pack_medical','Plecak medyczny','back','pack',3.5,{size:[.35,.42,.19],style:'medical'});
  add('pack_radio','Radiostacja plecakowa','back','pack',5.1,{size:[.31,.39,.18],style:'radio'});
  add('pack_engineer','Plecak narzędziowy','back','pack',3.8,{size:[.33,.43,.2],style:'engineer'});
  add('belt_light','Pas lekki','belt','belt',.24,{style:'light'});
  add('belt_utility','Pas oporządzenia','belt','belt',.5,{style:'utility'});
  const hips=['leftHip','rightHip','utility1','utility2'];
  const pouches=['leftHip','rightHip','leftThigh','rightThigh','utility1','utility2','utility3'];
  add('canteen','Manierka',hips,'pouch',1.0,{style:'canteen'});
  add('pouch_utility','Kieszeń ogólna',pouches,'pouch',.25,{style:'utility'});
  add('pouch_ammo','Ładownica',pouches,'pouch',.7,{style:'ammo'});
  add('pouch_medical','Apteczka',pouches,'pouch',.55,{style:'medical'});
  add('pouch_tools','Kieszeń narzędziowa',pouches,'pouch',.9,{style:'tools'});
  add('radio_handheld','Radio podręczne',['utility3','leftHip','rightHip'],'pouch',.3,{style:'radio'});
  add('binoculars','Lornetka',['utility3','utility1'],'pouch',.5,{style:'binoculars'});
  add('map_case','Mapnik',['leftHip','rightHip','utility1'],'pouch',.2,{style:'map'});
  add('rifle','Karabin','primaryWeapon','weapon',3.1,{style:'rifle'});
  add('carbine','Karabinek','primaryWeapon','weapon',2.6,{style:'carbine'});
  add('support_gun','Broń wsparcia','primaryWeapon','weapon',5.2,{style:'support'});
  add('heavy_support_gun','Ciężka broń wsparcia','primaryWeapon','weapon',7.0,{style:'heavy'});
  add('marksman_rifle','Karabin wyborowy','primaryWeapon','weapon',4.0,{style:'marksman'});
  add('knife','Nóż — rekwizyt','meleeWeapon','weapon',.25,{style:'knife'});
  add('grenade','Granat — rekwizyt','throwable','weapon',.40,{style:'grenade'});
  add('sidearm','Pistolet','secondaryWeapon','weapon',.85,{style:'sidearm'});
  R.EquipmentCatalog=Object.freeze({
    items:Object.freeze(items),
    get(id){if(!items[id])throw new Error('Nieznany element wyposażenia: '+id);return items[id];},
    forSlot(slot){return Object.values(items).filter(item=>item.slots.includes(slot));}
  });
})();
