(async function(){
  const R=RTS,lab=environmentLab,$=id=>document.getElementById(id),assert=(ok,message)=>{if(!ok)throw Error(message);},results=[];
  assert($('species').options.length===8,'Eight rock types');
  assert($('season').closest('details').hidden,'No seasons for rocks');
  assert(document.querySelectorAll('#categories button[aria-current]').length===1,'One active category');
  for(const species of Object.keys(R.RockCatalog)){
    $('species').value=species;$('species').dispatchEvent(new Event('change'));await lab.rebuild(true);
    assert(lab.model.genome.category==='rocks'&&lab.model.genome.species===species,'Rock preview');
    assert(lab.model.root.children[0].geometry.boundingSphere.radius>0,'Valid bounds');
    results.push({species,...lab.model.stats});
  }
  $('species').value='granite';$('species').dispatchEvent(new Event('change'));await lab.rebuild(true);
  const before=lab.model.root.children[0].geometry;
  $('roughness').value='1';$('roughness').dispatchEvent(new Event('input'));$('roughness').dispatchEvent(new Event('change'));await lab.rebuild();
  assert(lab.model.genome.genes.roughness===1&&lab.model.root.children[0].geometry!==before,'Gene edit rebuilds rock');
  $('detail').value='world';$('detail').dispatchEvent(new Event('change'));await lab.rebuild();assert(lab.model.stats.triangles===80,'World detail');
  const dna=R.RockGenome.create('sandstone',581,{moss:.7}),data={genome:dna,state:{detail:'high'}},transfer=new DataTransfer();
  transfer.items.add(new File([JSON.stringify(data)],'rock.json',{type:'application/json'}));$('import').files=transfer.files;await $('import').onchange();await lab.rebuild(true);
  assert(JSON.stringify(lab.genome)===JSON.stringify(dna),'Rock DNA import roundtrip');
  lab.renderer.render(lab.scene,lab.camera);
  assert(!(lab.renderer.info.programs||[]).some(p=>p.diagnostics&&p.diagnostics.runnable===false),'Rock shader compiles');
  assert(!$('message').textContent,'No editor error');
  return {results,tests:['8 types','category controls','gene editing','LOD','DNA import','WebGL rendering']};
})();
