(function () {
  'use strict';
  BuildingLab.createForms = function (getBuilding) {
    const $ = id => document.getElementById(id), G = RTS.ProceduralBuildingGenerator;
    const dependent = {roomProgram:'programs', shape:'footprints', roof:'roofs', stairType:'stairs', structure:'structures'};
    const groups = {
      lockTopology: {fields:['presetId','footprintFamily','roomProgram'], seeds:['preset','footprint','program']},
      lockEnvelope: {fields:['width','depth'], seeds:['storeys','footprint']},
      lockFacade: {fields:['facade','material'], seeds:['facade']},
      lockConstruction: {fields:['structuralSystem','stairFamily'], seeds:['structure','stairs']},
      lockRoof: {fields:['roofFamily','roofCovering','features'], seeds:['roof']}
    };
    const locks = new Map();
    const lockControls = {
      lockTopology:['presetId','roomProgram','shape'], lockEnvelope:['width','depth','floorHeight','floors'],
      lockFacade:['style','windowType','doorType','material'], lockConstruction:['structure','stairType'],
      lockRoof:['roof','roofCovering','featureChimney','featurePorch','featureBalcony','featureDormer']
    };
    function fill(id, values) {
      const el = $(id), previous = el.value;
      el.replaceChildren(new Option('AUTO', 'AUTO'), ...values.map(v => new Option(v, v)));
      el.value = values.includes(previous) ? previous : 'AUTO';
    }
    fill('presetId', Object.keys(G.presets)); fill('floors', ['1','2','3','4','5']);
    fill('style', ['CLASSIC','MODERN','RUSTIC','INDUSTRIAL']);
    fill('windowType', ['TALL','SASH','WIDE','SMALL','INDUSTRIAL']);
    fill('doorType', ['PANEL','GLAZED','STEEL','DOUBLE']);
    fill('roofCovering', ['clay','slate','shingle','metal','corrugated','membrane','thatch']);
    fill('material', ['stucco','brick','concrete','metal','wood','stone']);
    function presetId() {
      if (locks.has('lockTopology')) return locks.get('lockTopology').presetId;
      if ($('presetId').value !== 'AUTO') return $('presetId').value;
      const fields = {roomProgram:'roomProgram',shape:'footprintFamily',roof:'roofFamily',stairType:'stairFamily',structure:'structuralSystem'};
      const required = {};
      for (const [id,field] of Object.entries(fields)) if ($(id).value && $(id).value !== 'AUTO') required[field] = $(id).value;
      for (const [id,spec] of locks) for (const field of groups[id].fields) if (spec[field] !== undefined) required[field] = spec[field];
      const compatible = Object.keys(G.presets).filter(id => Object.entries(dependent).every(([control,key]) => !required[fields[control]] || G.presets[id][key].includes(required[fields[control]])));
      const seed = Number($('seed').value), chosen = new G().createSpec({seed}).presetId;
      return compatible.includes(chosen) || !compatible.length ? chosen : compatible[(seed >>> 0) % compatible.length];
    }
    function constrain() {
      const preset = G.presets[presetId()];
      for (const [id, key] of Object.entries(dependent)) fill(id, preset[key].filter(v => v !== 'EXPLICIT'));
      $('presetHint').textContent = 'Resolved preset: ' + presetId();
    }
    for (const id of ['presetId','seed']) $(id).addEventListener('change', constrain);
    for (const id of Object.keys(groups)) $(id).addEventListener('change', () => {
      if ($(id).checked && getBuilding()) locks.set(id, getBuilding().plan.spec);
      else locks.delete(id);
      for (const control of lockControls[id]) $(control).disabled = $(id).checked;
      constrain();
    });
    constrain();
    return {
      constrain,
      captureLocks(spec) { for (const id of Object.keys(groups)) if ($(id).checked && !locks.has(id)) locks.set(id, spec); },
      options() {
        const options = {seed:Number($('seed').value), presetId:presetId(), features:{}, facade:{}};
        if (!Number.isInteger(options.seed) || options.seed < 0 || options.seed > 4294967295) throw new TypeError('Seed must be an integer from 0 to 4294967295');
        const names = {presetId:'presetId',roomProgram:'roomProgram',shape:'footprintFamily',roof:'roofFamily',roofCovering:'roofCovering',stairType:'stairFamily',structure:'structuralSystem',material:'material'};
        for (const [id, key] of Object.entries(names)) if ($(id).value !== 'AUTO') options[key] = $(id).value;
        for (const [id,key] of Object.entries({floors:'storeyCount',width:'width',depth:'depth',floorHeight:'floorHeight',damagePercent:'damagePercent'})) {
          const el = $(id);
          if (!el.checkValidity()) throw new TypeError(el.previousElementSibling.textContent + ': ' + el.validationMessage);
          if (el.value && el.value !== 'AUTO') options[key] = Number(el.value);
        }
        for (const [id,key] of Object.entries({style:'style',windowType:'windowType',doorType:'doorType'})) if ($(id).value !== 'AUTO') options.facade[key] = $(id).value;
        options.furniture = $('showFurniture').checked;
        for (const key of ['chimney','porch','balcony','dormer']) options.features[key] = $('feature' + key[0].toUpperCase() + key.slice(1)).checked;
        for (const [id, spec] of locks) {
          const group = groups[id];
          for (const key of group.fields) if (spec[key] !== undefined) options[key] = spec[key];
          options.seeds ||= {};
          for (const key of group.seeds) options.seeds[key] = spec.seeds[key];
          if (id === 'lockEnvelope') { options.storeyCount = spec.storeys.count; options.floorHeight = spec.storeys.floorHeight; }
        }
        return options;
      }
    };
  };
})();
