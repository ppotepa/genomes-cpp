(function () {
  'use strict';
  const R=window.RTS;
  const base={torsoBase:'field_jacket',legs:'combat_pants',feet:'combat_boots',belt:'belt_utility',leftHip:'canteen',rightHip:'pouch_utility'};
  const profiles={};
  const put=(id,label,description,slots)=>profiles[id]=Object.freeze({id,label,description,slots:Object.freeze({...base,...slots})});
  put('RIFLEMAN','Strzelec','Standardowy hełm, kamizelka i oporządzenie.',{
    head:['helmet_standard','helmet_cover'],torsoArmor:['light_vest','plate_carrier'],chestRig:'chest_standard',back:[null,null,'pack_small'],hands:'gloves_light',primaryWeapon:'rifle'});
  put('ASSAULT','Szturmowiec','Zwarte wyposażenie bez dużego plecaka.',{
    head:'helmet_light',torsoBase:'combat_shirt',torsoArmor:'plate_carrier',chestRig:'chest_assault',back:[null,'pack_small'],face:[null,'goggles'],hands:'gloves_full',primaryWeapon:'carbine'});
  put('LIGHT_SUPPORT','Lekkie wsparcie','Duże ładownice i dodatkowe kieszenie.',{
    head:['helmet_standard','helmet_cover'],torsoArmor:'plate_carrier',chestRig:'chest_ammo',back:'pack_medium',leftThigh:'pouch_ammo',hands:'gloves_full',primaryWeapon:'support_gun'});
  put('HEAVY_SUPPORT','Ciężkie wsparcie','Ciężka kamizelka, hełm i plecak z rolką.',{
    head:'helmet_heavy',torsoArmor:'heavy_armor',chestRig:'chest_ammo',back:'pack_large',leftThigh:'pouch_ammo',rightThigh:'pouch_ammo',feet:'heavy_boots',hands:'gloves_full',primaryWeapon:'heavy_support_gun'});
  put('MARKSMAN','Strzelec wyborowy','Lekki zestaw i przyrząd obserwacyjny.',{
    head:['field_cap','helmet_light'],torsoArmor:'light_vest',chestRig:'webbing',back:'pack_small',hands:'gloves_light',utility3:'binoculars',primaryWeapon:'marksman_rifle'});
  put('SCOUT','Zwiadowca','Czapka lub kapelusz, bez ciężkiego pancerza.',{
    head:['boonie_hat','field_cap','patrol_cap','beanie'],torsoBase:'combat_shirt',legs:'field_pants',feet:'light_boots',belt:'belt_light',chestRig:'webbing',back:'pack_small',rightHip:'map_case',utility3:'binoculars',primaryWeapon:'carbine'});
  put('MEDIC','Medyk','Plecak medyczny i zestaw apteczek.',{
    head:'helmet_standard',torsoArmor:'light_vest',chestRig:'chest_medical',back:'pack_medical',rightHip:'pouch_medical',leftThigh:'pouch_medical',hands:'gloves_full',primaryWeapon:'carbine'});
  put('ENGINEER','Inżynier','Plecak, kieszenie narzędziowe i gogle.',{
    head:['helmet_standard','helmet_cover'],torsoArmor:'plate_carrier',face:'goggles',chestRig:'chest_tools',back:'pack_engineer',rightHip:'pouch_tools',rightThigh:'pouch_tools',feet:'heavy_boots',hands:'gloves_full',primaryWeapon:'carbine'});
  put('RADIO_OPERATOR','Radiooperator','Radiostacja plecakowa z anteną.',{
    head:'helmet_standard',torsoArmor:'light_vest',chestRig:'chest_standard',back:'pack_radio',utility3:'radio_handheld',hands:'gloves_light',primaryWeapon:'rifle'});
  put('SQUAD_LEADER','Dowódca drużyny','Radio, mapnik i wyposażenie obserwacyjne.',{
    head:['helmet_cover','beret'],torsoArmor:'plate_carrier',chestRig:'chest_standard',back:'pack_small',rightHip:'map_case',utility3:'radio_handheld',utility1:'binoculars',hands:'gloves_light',primaryWeapon:'carbine',secondaryWeapon:'sidearm'});
  R.InfantryLoadouts=Object.freeze(profiles);
})();
