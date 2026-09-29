'use strict';
const assert=require('assert');
const fs=require('fs');
const vm=require('vm');

const R={};
let squareRoots=0;
const instrumentedMath=new Proxy(Math,{get(target,key){if(key==='sqrt')return value=>{squareRoots++;return Math.sqrt(value);};return Reflect.get(target,key);}});
vm.runInNewContext(fs.readFileSync(require.resolve('../js/world/battlefield.js'),'utf8'),{window:{RTS:R},Math:instrumentedMath},{filename:'battlefield.js'});

let activeLocomotionCalls=0,activeSteps=0,arrivedSteps=0;
const active={
  marchArrived:false,marchTarget:100,marchSign:1,position:{x:0},marchLocomotionInput:{speedMps:0},
  setLocomotion(input){activeLocomotionCalls++;assert(input.speedMps>0);},
  step(){activeSteps++;}
};
const arrived={
  marchArrived:true,marchTarget:20,marchSign:1,position:{x:20},
  setLocomotion(){throw Error('settled marcher must not recompute locomotion');},
  step(){arrivedSteps++;}
};
const battlefield=Object.create(R.Battlefield.prototype);
Object.assign(battlefield,{elapsed:0,arrived:0,units:[active,arrived],terrain:{}});
battlefield.step(1/60);

assert.strictEqual(battlefield.elapsed,1/60);
assert.strictEqual(battlefield.arrived,1,'settled units remain included in arrival status');
assert.strictEqual(activeLocomotionCalls,1,'moving units still refresh their target speed');
assert.strictEqual(activeSteps,1,'moving unit simulation remains active');
assert.strictEqual(arrivedSteps,1,'settled units still advance animation and weapon state');
assert.strictEqual(active.marchLocomotionInput.speedMps,1.55);
battlefield.step(1/60);
assert.strictEqual(activeLocomotionCalls,1,'a capped unchanged march speed does not revalidate locomotion every fixed tick');
assert.strictEqual(activeSteps,2,'moving unit simulation continues when its requested speed is unchanged');
assert.strictEqual(arrivedSteps,2,'settled unit presentation continues on later fixed ticks');
assert.strictEqual(squareRoots,0,'the constant maximum-speed march band avoids a square root each fixed tick');
active.position.x=99.5;
battlefield.step(1/60);
assert.strictEqual(squareRoots,1,'the exact braking square root runs only after entering the speed-change band');
assert(Math.abs(active.marchLocomotionInput.speedMps-Math.sqrt(1.5))<1e-12,'near-target march speed retains the existing braking curve');
console.log('PASS settled marchers and unchanged capped-speed requests skip redundant path/controller work while simulation continues');
