// Exercise the actual embedded motor script without a board or browser.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const html = fs.readFileSync(path.join(__dirname, '../web/camera.html'), 'utf8');
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
for (const script of scripts) new vm.Script(script);
const elements = new Map();
const element = id => {
  if (!elements.has(id)) elements.set(id, {id, setAttribute() {}, textContent: '', value: '100'});
  return elements.get(id);
};
const buttons = [1, 2, 3, 4].map(move => ({
  dataset: {move: String(move)}, style: {}, events: {},
  addEventListener(name, callback) { this.events[name] = callback; },
  setPointerCapture() {}
}));
const calls = [], events = {};
let pump, reject = false, controllerState = 'FAULT';
let ultrasonicCycle = 'idle';
const responseHeaders = { get: name => name === 'X-HC-SR04-Cycle' ? ultrasonicCycle : null };
let holdNextRequest = false, pendingRequest = null;
const context = {
  AbortController,
  document: {hidden: false, getElementById: element, querySelectorAll: () => buttons,
    addEventListener(name, callback) { events[name] = callback; }},
  window: {addEventListener() {}},
  setTimeout() { return 1; }, clearTimeout() {},
  setInterval(callback) { pump = callback; },
  fetch: async (url, options) => {
    calls.push({url, ...options});
    if (holdNextRequest) {
      holdNextRequest = false;
      return new Promise((resolve, reject) => { pendingRequest = {resolve, reject}; });
    }
    return {ok: !reject, headers: responseHeaders, text: async () => reject ? 'Battery invalid' : 'OK',
      json: async () => ({state: controllerState, epoch: 42, key: 'test-key'})};
  }
};
vm.createContext(context);
vm.runInContext(scripts[0], context);
const flush = () => new Promise(resolve => setImmediate(resolve));
const body = () => calls.at(-1).body;
const motion = () => body().slice(0, 3);
(async () => {
  buttons[0].events.pointerdown({preventDefault() {}, pointerId: 1});
  await flush(); assert.equal(motion(), '1:0');
  element('beltOn').onclick(); await flush(); assert.equal(motion(), '1:1');
  const driveSpeed = element('driveSpeed');
  const beltSpeed = element('beltSpeed');
  driveSpeed.value = '42'; driveSpeed.oninput(); await flush();
  assert.equal(body(), '1:1:042:100');
  assert.equal(element('driveSpeedValue').textContent, '42%');
  beltSpeed.value = '9'; beltSpeed.oninput(); await flush();
  assert.equal(body(), '1:1:042:009');
  assert.equal(element('beltSpeedValue').textContent, '9%');

  buttons[0].events.pointerup(); await flush(); assert.equal(motion(), '0:1');
  for (const button of buttons.slice(1)) {
    button.events.pointerdown({preventDefault() {}, pointerId: 2});
    await flush(); assert.equal(motion(), `${button.dataset.move}:1`);
    button.events.pointercancel(); await flush(); assert.equal(motion(), '0:1');
  }
  const beforeStop = calls.length;
  element('allStop').onclick(); await flush(); assert.equal(motion(), '0:0');
  assert.equal(calls[beforeStop].body, 'stop');
  element('beltReverse').onclick(); await flush(); assert.equal(motion(), '0:2');
  pump(); await flush(); assert.equal(motion(), '0:2');
  element('beltOff').onclick(); await flush(); assert.equal(motion(), '0:0');
  buttons[0].events.pointerdown({preventDefault() {}, pointerId: 3});
  await flush();
  element('beltOn').onclick(); await flush();
  pump(); await flush(); assert.equal(motion(), '1:1');
  element('beltOn').onclick(); await flush(); assert.equal(motion(), '1:1');
  buttons[0].events.pointerup(); await flush();
  element('allStop').onclick(); await flush();
  reject = true;
  element('beltOn').onclick(); await flush(); await flush();
  assert.equal(motion(), '0:0'); // A rejected command clears desired movement.
  assert(element('motorNote').textContent.includes('Battery invalid'));
  reject = false;
  pump(); await flush();
  assert(element('motorNote').textContent.includes('Battery invalid'));
  element('beltOn').onclick(); await flush();
  assert(!element('motorNote').textContent.includes('Battery invalid'));
  await element('clearMotorFault').onclick();
  assert.equal(calls.at(-1).url, '/api/command');
  assert.equal(calls.at(-1).headers['X-Boat-Key'], 'test-key');
  assert.deepEqual(JSON.parse(body()), {action: 'clear', epoch: 42});
  pump(); await flush();
  assert.equal(motion(), '0:0'); // Clearing a fault must not restart the conveyor.
  controllerState = 'MANUAL';
  await element('clearMotorFault').onclick();
  assert.deepEqual(JSON.parse(body()), {action: 'stop', epoch: 42});
  holdNextRequest = true;
  pump();
  element('beltOn').onclick();
  pendingRequest.reject(new Error('Older heartbeat failed'));
  await flush(); await flush();
  assert.equal(motion(), '0:1'); // A pending heartbeat failure must not erase activation.
  holdNextRequest = true;
  pump();
  element('beltOff').onclick();
  element('beltOn').onclick();
  pendingRequest.resolve({ok: true, headers: responseHeaders, text: async () => 'OK'});
  await flush(); await flush();
  assert.equal(motion(), '0:1'); // An older response must not erase a new off/on cycle.
  holdNextRequest = true;
  pump();
  element('allStop').onclick();
  pendingRequest.reject(new Error('Older drive failed'));
  await flush(); await flush();
  assert.equal(motion(), '0:0'); // A newer stop remains authoritative.
  ultrasonicCycle = 'running';
  pump(); await flush();
  assert(element('ultrasonicNote').textContent.includes('đang quay'));
  ultrasonicCycle = 'finished';
  reject = true;
  pump(); await flush();
  assert(element('ultrasonicNote').textContent.includes('đã dừng'));
  reject = false;
  ultrasonicCycle = 'interrupted';
  pump(); await flush();
  assert(element('ultrasonicNote').textContent.includes('dừng sớm'));
  ultrasonicCycle = 'idle';
  pump(); await flush();
  assert.equal(element('ultrasonicNote').textContent, '');
  context.document.hidden = true;
  const count = calls.length; pump(); assert.equal(calls.length, count);
  console.log('PASS: camera scripts parse; direction, release, conveyor, stop, rejection and hidden tab');
})().catch(error => { console.error(error); process.exitCode = 1; });
