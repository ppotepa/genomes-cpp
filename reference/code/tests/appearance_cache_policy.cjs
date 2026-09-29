const fs=require('fs'),vm=require('vm'),assert=require('assert');
const RTS={Config:{GENERATOR_VERSION:'test'}};
vm.runInNewContext(fs.readFileSync('js/rendering/appearanceCache.js','utf8'),{window:{RTS}});
const cache=RTS.AppearanceCache;
const phenotype={height:1.8,face:{},body:{}},side={uniformColor:0xffffff};
const key=detail=>cache.key(phenotype,side,null,detail);
const value=()=>{const array=new Uint8Array(8),geometry={index:null,attributes:{position:{array}},morphAttributes:{},dispose(){this.disposed=true;}};return{surface:{geometry},gear:null};};
const a=key('world'),b=key('far');
cache.put(a,value());cache.put(b,value());
assert.strictEqual(cache.stats().entries,2);
cache.setLimit(8); // Evicts the older key deterministically.
assert.strictEqual(cache.stats().entries,1);
assert.strictEqual(cache.take(a),null);
cache.put(a,value());
assert.ok(cache.take(a));
const stats=cache.stats(),record=stats.keyStats.find(item=>item.detail==='world');
assert.ok(record&&record.evictions===1&&record.misses===1&&record.hitsAfterEviction===1);
assert.strictEqual(Object.prototype.hasOwnProperty.call(record,'key'),false);
assert.match(record.id,/^[0-9a-f]{8}$/);
assert.ok(stats.activeBytes===0&&stats.inactiveBytes<=stats.limit);
const activeKey=key('active'),activeValue=value();cache.acquire(activeKey,activeValue);
assert.strictEqual(cache.stats().peakActiveBytes,8,'active appearance high-water captures the exact owned backing bytes');
assert.strictEqual(cache.stats().peakActiveAppearances,1,'active appearance high-water captures simultaneous models');
cache.put(a,value());cache.clear();
assert.strictEqual(activeValue.surface.geometry.disposed,undefined,'clearing retained entries must not dispose active geometry');
assert.ok(cache.stats().activeEntries===1&&cache.stats().entries===0,'clear should preserve active references and release every inactive entry');
assert.strictEqual(cache.release(activeKey,activeValue),true,'last active owner should release cleanly after cache clear');
assert.strictEqual(cache.stats().peakActiveBytes,8,'releasing appearances preserves the observed active-buffer peak');
const aliasedBuffer=new ArrayBuffer(32),aliasedGeometry={
  index:{array:new Uint16Array(aliasedBuffer,0,4)},
  attributes:{position:{array:new Float32Array(aliasedBuffer,0,4)},color:{array:new Uint8Array(aliasedBuffer,16,4)}},
  morphAttributes:{position:[{array:new Float32Array(aliasedBuffer,0,4)}]},dispose(){}
};
const aliasedKey=key('aliased-buffers'),aliasedValue={surface:{geometry:aliasedGeometry},gear:{geometry:aliasedGeometry}};
cache.acquire(aliasedKey,aliasedValue);
assert.strictEqual(cache.stats().activeBytes,32,'shared typed-array backing stores are counted once across attributes and parts');
assert.strictEqual(cache.release(aliasedKey,aliasedValue),true);
assert.strictEqual(cache.stats().activeBytes,0,'deduplicated active buffers are released once with their last owner');
cache.setLimit(64*1024*1024);cache.put(aliasedKey,aliasedValue);
assert.strictEqual(cache.stats().inactiveBytes,32,'retained geometry size also deduplicates shared backing buffers');
cache.clear();
// A key demonstrably reused after eviction gets one short second chance. It
// may displace a colder retained key, while the byte limit remains absolute.
cache.setLimit(16);
const warm=key('warm-after-eviction'),cold=key('cold-after-eviction'),newcomer=key('newcomer-after-eviction');
cache.put(warm,value());cache.put(cold,value());cache.setLimit(8);
assert.strictEqual(cache.take(warm),null,'oldest cold entry should be evicted first');
cache.put(warm,value());assert.ok(cache.take(warm),'rebuilt warm entry should be reusable');
cache.put(warm,value());
const newcomerValue=value();cache.put(newcomer,newcomerValue);
assert.strictEqual(newcomerValue.surface.geometry.disposed,true,'hard byte limit should evict the newcomer when all older data has reuse protection');
const retainedWarm=cache.take(warm);assert.ok(retainedWarm,'recently reused geometry should receive a second chance');
assert.strictEqual(cache.stats().inactiveBytes,0,'taking the protected entry should release it from retained-byte accounting');
cache.setLimit(64*1024*1024);
console.log(JSON.stringify({evictionId:record.id,evictions:record.evictions,misses:record.misses,hitsAfterEviction:record.hitsAfterEviction,entries:cache.stats().entries}));
