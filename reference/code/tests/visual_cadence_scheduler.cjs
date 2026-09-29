'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');

let numberConversions=0,maxCalls=0;
function CountedNumber(value){numberConversions++;return Number(value);}
CountedNumber.isFinite=Number.isFinite;
const math={min:Math.min,max(...values){maxCalls++;return Math.max(...values);}};
const R={Unit:class Unit{}};
vm.runInNewContext(fs.readFileSync(path.join(__dirname,'..','js','units','infantryUnit.js'),'utf8'),{window:{RTS:R},Number:CountedNumber,Math:math});

const unit=Object.create(R.InfantryUnit.prototype);
Object.assign(unit,{animationUpdateInterval:.1,faceUpdateInterval:.1,_animationUpdateAccumulator:.05,_faceUpdateAccumulator:.075,
  _animationCadencePhase:.23,_faceCadencePhase:.71});
unit.setVisualCadence(.1,.1);
assert.equal(numberConversions,0,'unchanged cadence should skip numeric normalization');
assert.equal(maxCalls,0,'unchanged cadence should skip clamping');
assert.equal(unit._animationUpdateAccumulator,.05,'unchanged cadence preserves animation progress');
assert.equal(unit._faceUpdateAccumulator,.075,'unchanged cadence preserves face progress');

unit.setVisualCadence(.05,.08);
assert.equal(numberConversions,2,'changed numeric intervals still normalize');
assert.equal(maxCalls,2,'changed numeric intervals still clamp');
assert.equal(unit._animationUpdateAccumulator,.025,'animation phase is preserved across cadence change');
assert.ok(Math.abs(unit._faceUpdateAccumulator-.06)<1e-12,'face phase is preserved across cadence change');
const afterChange=[numberConversions,maxCalls,unit._animationUpdateAccumulator,unit._faceUpdateAccumulator];
unit.setVisualCadence(.05,.08);
assert.deepEqual([numberConversions,maxCalls,unit._animationUpdateAccumulator,unit._faceUpdateAccumulator],afterChange,
  'stable medium cadence must not remap accumulated progress');

unit.setVisualCadence(0,0);
assert.equal(unit.animationUpdateInterval,0,'close-range animation interval remains unthrottled');
assert.equal(unit.faceUpdateInterval,0,'close-range face interval remains unthrottled');
assert.equal(unit._animationUpdateAccumulator,0);
assert.equal(unit._faceUpdateAccumulator,0);
const stableCount=[numberConversions,maxCalls];
unit.setVisualCadence(0,0);
assert.deepEqual([numberConversions,maxCalls],stableCount,'stable close-range cadence takes the fast path');

// Non-number API inputs must continue through Number()/Math.max() normalization.
const beforeString=[numberConversions,maxCalls];
unit.setVisualCadence('0','0');
assert.equal(numberConversions,beforeString[0]+2,'string intervals still pass through numeric conversion');
assert.equal(maxCalls,beforeString[1]+2,'string intervals still pass through clamping');
assert.equal(unit.animationUpdateInterval,0);
assert.equal(unit.faceUpdateInterval,0);
console.log('PASS visual cadence stable-profile fast path preserves phase and close-range updates');
