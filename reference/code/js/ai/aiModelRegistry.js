(function(){
  'use strict';
  const R=globalThis.RTS=globalThis.RTS||{};
  class AIModelRegistry{
    constructor(){this.factories=new Map();}
    register(id,factory){if(typeof id!=='string'||!id||typeof factory!=='function')throw new TypeError('AI model requires an id and factory.');this.factories.set(id,factory);return this;}
    create(id,initialization={}){const factory=this.factories.get(id);if(!factory)throw new Error('Unknown AI model: '+id);const model=factory(initialization);if(!model||typeof model.update!=='function')throw new TypeError('AI model '+id+' must expose update(observation, dt).');return model;}
    has(id){return this.factories.has(id);}
  }
  R.AIModelRegistry=R.AIModelRegistry instanceof AIModelRegistry?R.AIModelRegistry:new AIModelRegistry();
  R.AIModelRegistry.Registry=AIModelRegistry;
})();
