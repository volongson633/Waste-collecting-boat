// Run actual UI handlers in Node's VM with a minimal DOM and a simulated device API.
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '../..');
const html = fs.readFileSync(path.join(root,'web/index.html'),'utf8');
const script = fs.readFileSync(path.join(root,'web/control.js'),'utf8');

function fixture() {
  const elements = [], listeners = {}, requests = [];
  for (const match of html.matchAll(/<([a-z]+)\b([^>]*)>/gi)) {
    const attrs = {};
    for (const attr of match[2].matchAll(/([\w-]+)="([^"]*)"/g)) attrs[attr[1]] = attr[2];
    if (!attrs.id && !attrs['data-drive'] && !attrs['data-rgb']) continue;
    const el = {id:attrs.id, dataset:{}, style:{}, textContent:'', disabled:/\bdisabled\b/.test(match[2]), value:attrs.value || '', events:{},
      setAttribute(k,v){attrs[k]=v;}, getAttribute(k){return attrs[k];},
      removeAttribute(k){delete attrs[k];},
      setPointerCapture(){}, addEventListener(k,f){this.events[k]=f;}};
    if (attrs['data-drive']) el.dataset.drive=attrs['data-drive'];
    if (attrs['data-rgb']) el.dataset.rgb=attrs['data-rgb'];
    elements.push(el);
  }
  const get = id => { const el=elements.find(x=>x.id===id); assert.ok(el,`Missing element ${id}`); return el; };
  const query = selector => elements.filter(el=>selector.split(',').some(s=>s.startsWith('#') ? el.id===s.slice(1) : s==='[data-drive]' ? !!el.dataset.drive : s==='[data-rgb]' ? !!el.dataset.rgb : false));
  const listen = (name, callback) => {
    const previous=listeners[name];
    listeners[name]=previous ? (...args)=>{previous(...args);callback(...args);} : callback;
  };
  const device = {state:'STOP',reason:'none',epoch:10,sequence:0,version:1,key:'boot-key',outputsEnabled:false,readyToArm:true,
    detector:'red_marker_demo',cameraReady:true,detectorReady:true,processingMs:100,collectionCycles:0,
    batteryValid:false,batteryV:0,conveyorPwm:false,psram:100000,captureFailures:0,
    motor:{left:0,right:0,conveyor:0},detection:{healthy:true,found:false,ageMs:10,x:.5,y:.5},
    lights:{redAvailable:true,rgbAvailable:true,red:false,rgb:'off',revision:0}};
  const copy = () => JSON.parse(JSON.stringify(device));
  // No AbortSignal.timeout: older phone browsers still have AbortController.
  const context = vm.createContext({console,AbortController,URL,location:{hostname:'192.168.4.1'},
    setTimeout(){return 1;},clearTimeout(){},setInterval(){return 1;},
    document:{hidden:false,getElementById:get,querySelectorAll:query,addEventListener:listen},
    window:{addEventListener:listen},
    async fetch(url,options={}) {
      const body=options.body?JSON.parse(options.body):null;
      requests.push({url,body});
      if (url==='/api/status') { if(device.offline) throw new Error('offline'); return {ok:true,json:async()=>copy()}; }
      if (url==='/api/led') {
        if(body.epoch!==device.epoch || body.revision!==device.lights.revision) return {ok:false};
        device.lights.red=body.red; device.lights.rgb=body.rgb; ++device.lights.revision;
      } else if (body.action==='stop') {
        device.state='STOP'; ++device.epoch; device.motor={left:0,right:0,conveyor:0};
        device.lights.red=false; device.lights.rgb='off'; ++device.lights.revision;
      } else if(body.action==='manual') {
        if(body.epoch!==device.epoch || device.state!=='STOP') return {ok:false};
        device.state='MANUAL'; ++device.epoch;
      } else if(body.action==='drive') {
        if(body.epoch!==device.epoch || device.state!=='MANUAL') return {ok:false};
        device.motor={left:body.left,right:body.right,conveyor:body.conveyor};
      }
      ++device.version;
      return {ok:true,json:async()=>copy()};
    }
  });
  vm.runInContext(script,context);
  const evaluate = text => vm.runInContext(text,context);
  return {get,query,device,requests,evaluate,listeners,context};
}
const flush = () => new Promise(resolve => setImmediate(resolve));

test('all four held directions send the correct motor signs; release stops only steering', async()=>{
  const f=fixture(); await flush(); await f.evaluate("arm('manual')");
  for(const [dir,signs] of Object.entries({forward:[1,1],back:[-1,-1],left:[-1,1],right:[1,-1]})) {
    const button=f.query('[data-drive]').find(b=>b.dataset.drive===dir);
    button.events.pointerdown({preventDefault(){},pointerId:1}); await flush();
    assert.deepEqual([Math.sign(f.device.motor.left),Math.sign(f.device.motor.right)],signs);
    button.events.pointerup(); await flush();
    assert.equal(f.device.motor.left,0); assert.equal(f.device.motor.right,0);
  }
});
test('conveyor latches on between drive commands and turns off explicitly',async()=>{
  const f=fixture(); await flush(); await f.evaluate("arm('manual')");
  f.get('conveyorOn').onclick(); await flush(); assert.equal(f.device.motor.conveyor,180);
  await f.evaluate('pump()'); assert.equal(f.device.motor.conveyor,180);
  f.get('conveyorOff').onclick(); await flush(); assert.equal(f.device.motor.conveyor,0);
});
test('stop clears the latched conveyor and does not resume on further ticks',async()=>{
  const f=fixture(); await flush(); await f.evaluate("arm('manual')");
  f.get('conveyorOn').onclick(); await flush();
  await f.evaluate('stopAll()'); await f.evaluate('pump()');
  assert.equal(f.device.state,'STOP'); assert.equal(f.device.motor.conveyor,0);
  assert.equal(f.evaluate('ownedEpoch'),null); assert.equal(f.evaluate('conveyorOn'),false);
});
test('LED controls work while stopped and never send a drive/arm heartbeat',async()=>{
  const f=fixture(); await flush();
  f.get('redLed').onclick(); await flush(); assert.equal(f.device.lights.red,true);
  f.query('[data-rgb]').find(b=>b.dataset.rgb==='yellow').onclick(); await flush();
  assert.equal(f.device.lights.rgb,'yellow'); assert.equal(f.device.lights.red,true);
  assert.equal(f.device.state,'STOP');
  assert.equal(f.requests.filter(r=>r.url==='/api/command').length,0);
});
test('old status responses cannot restore a previous LED or motor state',async()=>{
  const f=fixture(); await flush(); f.context.old=JSON.parse(JSON.stringify(f.device));
  f.get('redLed').onclick(); await flush();
  f.evaluate('render(old)');
  assert.equal(f.get('redLed').getAttribute('aria-pressed'),'true');
});
test('network loss drops control ownership and disables outputs in the UI',async()=>{
  const f=fixture(); await flush(); await f.evaluate("arm('manual')");
  f.get('conveyorOn').onclick(); await flush(); f.device.offline=true;
  await f.evaluate('poll()'); const commands=f.requests.length; await f.evaluate('pump()');
  assert.equal(f.evaluate('ownedEpoch'),null); assert.equal(f.evaluate('conveyorOn'),false);
  assert.equal(f.get('conveyorOn').disabled,true); assert.equal(f.get('redLed').disabled,true);
  assert.equal(f.requests.length,commands);
});
test('hidden page sends stop, including a latched conveyor',async()=>{
  const f=fixture(); await flush(); await f.evaluate("arm('manual')");
  f.get('conveyorOn').onclick(); await flush(); f.context.document.hidden=true;
  f.listeners.visibilitychange(); await flush();
  assert.equal(f.device.state,'STOP'); assert.equal(f.device.motor.conveyor,0);
});
