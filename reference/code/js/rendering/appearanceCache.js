(function(){
  'use strict';
  const R=window.RTS;
  // These face values drive animation only; they don't change the compiled
  // surface. neutralEyeOpen stays in the key because it is stored in the
  // geometry's face metadata and consumed when the rig is initialized.
  const animatedFaceGenes=new Set([
    'blinkInterval','blinkDuration','gazeRestlessness','neutralBrow',
    'expressionScale','eyeExpressionScale','mouthExpressionScale','browExpressionScale'
  ]);
  // Only inactive geometry is retained. take() transfers exclusive ownership
  // back to one model; skeletons, morph weights and materials are never cached.
  const entries=new Map(),activeEntries=new Map(),activeBuffers=new Map();let bytes=0,activeBytes=0,peakActiveBytes=0,activeAppearances=0,peakActiveAppearances=0,limit=64*1024*1024,accessClock=0;
  // Buffer enumeration is synchronous and callbacks below only update counts
  // or Maps. Reuse this tiny deduplication list instead of allocating a Set,
  // parts array, and nested closures on every acquire/release/size operation.
  const bufferScratch=[];
  const stats={hits:0,misses:0,evictions:0,builds:0,buildMs:0};
  // Keep bounded, geometry-key-level evidence so cache policy can be tuned
  // from real use without retaining unbounded genome strings.
  const keyStats=new Map(),KEY_STATS_LIMIT=256;
  function fingerprint(key){let h=2166136261;for(let i=0;i<key.length;i++){h^=key.charCodeAt(i);h=Math.imul(h,16777619);}return (h>>>0).toString(16).padStart(8,'0');}
  function keyRecord(key,detail){let item=keyStats.get(key);if(!item){item={id:fingerprint(key),detail,accesses:0,hits:0,misses:0,evictions:0,hitsAfterEviction:0,protectedHits:0,bytes:0,lastAccess:0};keyStats.set(key,item);if(keyStats.size>KEY_STATS_LIMIT)keyStats.delete(keyStats.keys().next().value);}else{keyStats.delete(key);keyStats.set(key,item);if(detail!==undefined)item.detail=detail;}return item;}
  function access(key,hit){const item=keyRecord(key);item.accesses++;item.lastAccess=++accessClock;if(hit){item.hits++;if(item.evictions>item.hitsAfterEviction)item.hitsAfterEviction++;}else item.misses++;}
  function addBuffer(buffer,visit){
    if(!buffer)return;
    for(let i=0;i<bufferScratch.length;i++)if(bufferScratch[i]===buffer)return;
    bufferScratch.push(buffer);visit(buffer);
  }
  function addAttributeBuffer(attribute,visit){
    if(attribute&&attribute.array)addBuffer(attribute.array.buffer,visit);
  }
  function eachGeometryBuffer(geometry,visit){
    if(!geometry)return;
    addAttributeBuffer(geometry.index,visit);
    const attributes=geometry.attributes||{};
    for(const name in attributes)addAttributeBuffer(attributes[name],visit);
    const morphAttributes=geometry.morphAttributes||{};
    for(const name in morphAttributes){const list=morphAttributes[name];for(let i=0;i<list.length;i++)addAttributeBuffer(list[i],visit);}
  }
  function eachBuffer(value,visit){
    bufferScratch.length=0;
    try{
      if(value.surface)eachGeometryBuffer(value.surface.geometry,visit);
      if(value.gear)eachGeometryBuffer(value.gear.geometry,visit);
    }finally{bufferScratch.length=0;}
  }
  function size(value){let total=0;eachBuffer(value,buffer=>{total+=buffer.byteLength;});return total;}
  function geometryFaceGenes(face){return Object.keys(face).filter(key=>!animatedFaceGenes.has(key)).sort().map(key=>[key,face[key]]);}
  function surfaceKey(phenotype,side,detail,equipment,fitSignature=R.EquipmentFit?R.EquipmentFit.surfaceSignatureFor(equipment):null){
    return JSON.stringify([R.Config.GENERATOR_VERSION,phenotype.height,geometryFaceGenes(phenotype.face),phenotype.body,side.uniformColor,detail,fitSignature]);
  }
  function gearKey(phenotype,side,equipment,detail){
    // Gear placement uses the complete fitted anatomy. Slot contents, palette,
    // and wear also affect emitted topology, dimensions, or vertex colors.
    return JSON.stringify([R.Config.GENERATOR_VERSION,phenotype.height,geometryFaceGenes(phenotype.face),phenotype.body,side.uniformColor,detail,equipment?{slots:equipment.slots,palette:equipment.palette,wear:equipment.wear}:null]);
  }
  function acquire(value){
    eachBuffer(value,buffer=>{const item=activeBuffers.get(buffer);if(item)item.refs++;else{activeBuffers.set(buffer,{refs:1,bytes:buffer.byteLength});activeBytes+=buffer.byteLength;}});
    activeAppearances++;
    if(activeBytes>peakActiveBytes)peakActiveBytes=activeBytes;
    if(activeAppearances>peakActiveAppearances)peakActiveAppearances=activeAppearances;
  }
  function release(value){
    eachBuffer(value,buffer=>{const item=activeBuffers.get(buffer);if(!item)throw new Error('Appearance geometry released without an active owner');if(--item.refs===0){activeBuffers.delete(buffer);activeBytes-=item.bytes;}});
    if(activeAppearances<=0)throw new Error('Appearance released without an active owner');
    activeAppearances--;
  }
  function dispose(value){value.surface.geometry.dispose();if(value.gear)value.gear.geometry.dispose();}
  function remove(key,eviction){const item=entries.get(key);if(!item)return;entries.delete(key);bytes-=item.bytes;dispose(item.value);if(eviction){stats.evictions++;const record=keyRecord(key);record.evictions++;record.bytes=item.bytes;}}
  function trim(){
    while(bytes>limit||entries.size>64){
      let victim=null;
      for(const [key,item] of entries){if(item.protectUntil<=accessClock){victim=key;break;}}
      // Reuse evidence only grants a short second chance. Hard memory/count
      // limits always win, even when every retained entry is still protected.
      if(victim===null)victim=entries.keys().next().value;
      if(victim===undefined)break;
      remove(victim,true);
    }
  }
  R.AppearanceCache={
    geometryFaceGenes,
    surfaceKey,
    gearKey,
    // Exact values: do not quantize DNA to make cache hits more likely.
    key(phenotype,side,equipment,detail){
      // Keep separate exact signatures for reusable body and combined gear
      // geometry. Animated face controls stay outside both dependency graphs.
      const key=JSON.stringify(['appearance-3',surfaceKey(phenotype,side,detail,equipment),gearKey(phenotype,side,equipment,detail)]);keyRecord(key,detail);return key;
    },
    take(key){
      const active=activeEntries.get(key);if(active){stats.hits++;access(key,true);return active.value;}
      const item=entries.get(key);if(!item){stats.misses++;access(key,false);return null;}entries.delete(key);bytes-=item.bytes;stats.hits++;access(key,true);return item.value;
    },
    acquire(key,value){
      const item=activeEntries.get(key);
      if(item)item.refs++;
      else activeEntries.set(key,{value,refs:1});
      acquire(value);
    },
    release(key,value){
      const item=activeEntries.get(key);
      if(!item)throw new Error('Appearance released without an active cache owner');
      release(value);
      if(--item.refs>0)return false;
      activeEntries.delete(key);return true;
    },
    put(key,value,detail){
      // Identical active models share immutable geometry. Keep it alive until
      // the last rig releases it; a concurrent put must never dispose it.
      if(activeEntries.has(key))return false;
      const count=size(value);if(!key||count>limit||limit===0){dispose(value);return false;}remove(key,false);const record=keyRecord(key,detail),protectUntil=record.hitsAfterEviction>record.protectedHits?accessClock+64:0;if(protectUntil)record.protectedHits=record.hitsAfterEviction;entries.set(key,{value,bytes:count,protectUntil});bytes+=count;record.bytes=count;trim();return entries.has(key);
    },
    recordBuild(ms){stats.builds++;stats.buildMs+=ms;},
    setLimit(value){if(!Number.isFinite(value)||value<0)throw new RangeError('Cache limit must be nonnegative bytes');limit=value;trim();},
    clear(){for(const key of Array.from(entries.keys()))remove(key,false);},
    stats(){return {...stats,bytes,inactiveBytes:bytes,activeBytes,peakActiveBytes,activeAppearances,peakActiveAppearances,activeEntries:activeEntries.size,limit,entries:entries.size,keyStats:Array.from(keyStats.values(),item=>({...item})).sort((a,b)=>(b.evictions-a.evictions)||(b.misses-a.misses)||(b.accesses-a.accesses)).slice(0,12)};}
  };
})();
