'use strict';
const $ = id => document.getElementById(id);
let remote = null, key = '', ownedEpoch = null, sequence = 0, generation = 0;
let direction = '', conveyorOn = false, commandBusy = false, armBusy = false, ledBusy = false;
let previewOn = true, retryTimer = null;
const snapshotUrl = () => `http://${location.hostname}:82/capture?t=${Date.now()}`;
const reasons = {
  none:'', user_stop:'Đã dừng theo yêu cầu.', link_timeout:'Đã dừng do quá hạn lệnh điều khiển.',
  battery_invalid:'Chưa đo được điện áp pin hợp lệ.', battery_low:'Pin thấp. Sạc/kiểm tra pin trước khi xóa lỗi.',
  camera_unavailable:'Camera chưa sẵn sàng.', vision_unavailable:'Chưa có bộ nhận diện.', vision_stale:'Ảnh nhận diện đã quá cũ.',
  search_timeout:'Hết thời gian tìm kiếm.', approach_timeout:'Hết thời gian tiếp cận.',
  bin_full:'Thùng rác đã đầy.', obstacle:'Phát hiện vật cản phía trước.',
  jam:'Phát hiện kẹt băng tải.', hardware:'Lỗi khởi tạo phần cứng.'
};
async function request(url, options = {}, timeoutMs = 1000, format = 'json') {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, {...options, signal:controller.signal});
    if (!response.ok) throw new Error('Lệnh bị từ chối. Kiểm tra trạng thái, pin và camera.');
    return await response[format]();
  } finally { clearTimeout(timer); }
}
async function post(body, endpoint = '/api/command') {
  return request(endpoint, {
    method:'POST', headers:{'Content-Type':'application/json','X-Boat-Key':key},
    body:JSON.stringify(body), cache:'no-store'
  }, 800);
}
function relinquish() { ownedEpoch = null; direction = ''; conveyorOn = false; ++generation; }
function render(s) {
  // Requests can finish out of order. Never restore an older motor/LED state.
  if (remote && s.key === remote.key && ((s.version - remote.version) | 0) < 0) return;
  remote = s; key = s.key;
  if (ownedEpoch !== null && (ownedEpoch !== s.epoch || s.state === 'STOP' || s.state === 'FAULT')) relinquish();
  $('connection').textContent = 'Đã kết nối'; $('state').textContent = s.state;
  $('battery').textContent = s.batteryValid ? `${s.batteryV.toFixed(2)} V` : 'Chưa đo được';
  $('cycles').textContent = s.collectionCycles; $('latency').textContent = `${s.processingMs} ms`;
  $('mode').textContent = s.outputsEnabled ? 'ĐANG DÙNG ĐỘNG CƠ THẬT' : 'THỬ TRÊN BÀN · Đầu ra động cơ được mô phỏng';
  $('detector').textContent = s.detector === 'red_marker_demo' ? 'Thử căn hướng với vật màu đỏ. Đây chưa phải AI nhận diện rác.' :
    s.detector === 'edge_impulse' ? 'Nhận diện bằng mô hình Edge Impulse đã nạp.' : 'Chưa nạp mô hình nhận diện; có thể điều khiển thủ công và thu ảnh.';
  $('conveyorMode').textContent = s.conveyorPwm ? 'Băng tải có điều chỉnh PWM.' : 'Băng tải dùng jumper ENA: khi bật sẽ chạy toàn tốc.';
  $('conveyorState').textContent = s.motor.conveyor < 0 ? 'Đang quay ngược' : s.motor.conveyor > 0 ? 'Đang chạy' : 'Đang tắt';
  $('conveyorOn').setAttribute('aria-pressed', String(conveyorOn));
  $('detail').textContent = `Trái/phải/băng tải: ${s.motor.left}/${s.motor.right}/${s.motor.conveyor}. PSRAM trống: ${Math.round(s.psram/1024)} KB. Lỗi chụp: ${s.captureFailures}.`;
  $('message').textContent = s.state === 'STOP' && !s.readyToArm ? 'Chưa thể chạy motor: kiểm tra nguồn, bộ chia áp đo pin hoặc lỗi phần cứng.' : reasons[s.reason] || s.reason;
  const ours = ownedEpoch !== null && ownedEpoch === s.epoch;
  const manual = ours && s.state === 'MANUAL';
  document.querySelectorAll('[data-drive],#conveyorOn,#conveyorOff,#neutral').forEach(b => b.disabled = !manual);
  $('manual').disabled = armBusy || s.state !== 'STOP' || !s.readyToArm;
  $('auto').disabled = armBusy || s.state !== 'STOP' || !s.readyToArm || !s.detectorReady || !s.detection.healthy || s.detection.ageMs > 1500;
  $('clear').disabled = s.state !== 'FAULT' || !s.readyToArm;
  renderLights(s);
  const visible = s.detection.found && s.detection.healthy && s.detection.ageMs <= 1500 && previewOn;
  $('marker').style.display = visible ? 'block' : 'none';
  $('marker').style.left = `${s.detection.x*100}%`; $('marker').style.top = `${s.detection.y*100}%`;
}
function renderLights(s) {
  $('redLed').disabled = ledBusy || !s.lights.redAvailable;
  $('redLed').setAttribute('aria-pressed', String(s.lights.red));
  $('redText').textContent = s.lights.redAvailable ? (s.lights.red ? 'Tắt LED đỏ ngoài' : 'Bật LED đỏ ngoài') : 'LED đỏ ngoài đã tắt trong cấu hình';
  $('redDot').style.background = s.lights.red ? '#ed384b' : '#b6c6be';
  document.querySelectorAll('[data-rgb]').forEach(button => {
    button.disabled = ledBusy || !s.lights.rgbAvailable;
    button.setAttribute('aria-pressed', String(button.dataset.rgb === s.lights.rgb));
  });
}
async function setLights(change) {
  if (!remote || !key || ledBusy) return;
  ledBusy = true; renderLights(remote);
  const localGeneration = generation;
  const body = {epoch:remote.epoch, revision:remote.lights.revision, red:remote.lights.red, rgb:remote.lights.rgb, ...change};
  try {
    const s = await post(body, '/api/led');
    if (localGeneration === generation) { render(s); $('ledMessage').textContent = 'Đã gửi lệnh LED.'; }
  } catch { $('ledMessage').textContent = 'Chưa đổi được LED. Chờ trạng thái cập nhật rồi thử lại.'; }
  finally { ledBusy = false; if (remote) renderLights(remote); }
}
async function stopAll() {
  relinquish();
  if (!key) return;
  try { render(await post({action:'stop'})); } catch { $('message').textContent = 'Mất liên lạc. Firmware sẽ dừng khi hết hạn lệnh (1 giây).'; }
}
async function arm(action) {
  if (!remote || armBusy) return;
  armBusy = true; const localGeneration = ++generation;
  direction = ''; conveyorOn = false;
  try {
    const s = await post({action,epoch:remote.epoch});
    if (localGeneration !== generation) { await stopAll(); return; }
    ownedEpoch = s.epoch; sequence = 0; render(s);
  } catch (e) { relinquish(); $('message').textContent = e.message; }
  finally { armBusy = false; }
}
async function pump() {
  if (commandBusy || ownedEpoch === null || !remote || document.hidden) return;
  commandBusy = true;
  const epoch = ownedEpoch, requestGeneration = generation;
  const body = {action:'heartbeat',epoch,sequence:++sequence};
  if (remote.state === 'MANUAL') {
    body.action = 'drive'; const p = Math.round(Number($('speed').value)*255/100);
    const movement = {forward:[p,p],back:[-p,-p],left:[-p,p],right:[p,-p]};
    [body.left, body.right] = movement[direction] || [0,0];
    body.conveyor = conveyorOn ? 180 : 0;
  }
  try { const s = await post(body); if (requestGeneration === generation) render(s); }
  catch (e) { if (requestGeneration === generation) { relinquish(); await stopAll(); $('message').textContent = e.message; } }
  finally { commandBusy = false; }
}
document.querySelectorAll('[data-drive]').forEach(button => {
  button.addEventListener('pointerdown', e => { if (button.disabled) return; e.preventDefault(); button.setPointerCapture(e.pointerId); direction = button.dataset.drive; pump(); });
  const release = () => { if (direction === button.dataset.drive) { direction = ''; pump(); } };
  button.addEventListener('pointerup', release); button.addEventListener('pointercancel', release); button.addEventListener('lostpointercapture', release);
});
$('conveyorOn').onclick = () => { if ($('conveyorOn').disabled) return; conveyorOn = true; pump(); };
$('conveyorOff').onclick = () => { if ($('conveyorOff').disabled) return; conveyorOn = false; pump(); };
$('redLed').onclick = () => { if (!$('redLed').disabled && remote) setLights({red:!remote.lights.red}); };
document.querySelectorAll('[data-rgb]').forEach(button => { button.onclick = () => { if (!button.disabled) setLights({rgb:button.dataset.rgb}); }; });
$('neutral').onclick = () => { direction = ''; pump(); };
$('stop').onclick = stopAll; $('manual').onclick = () => arm('manual'); $('auto').onclick = () => arm('auto');
$('clear').onclick = async () => { try { render(await post({action:'clear',epoch:remote.epoch})); } catch(e) { $('message').textContent=e.message; } };
$('speed').oninput = () => $('speedText').textContent = `${$('speed').value}%`;
document.addEventListener('visibilitychange', () => { if (document.hidden) stopAll(); });
window.addEventListener('blur', () => { if (ownedEpoch !== null) stopAll(); });
window.addEventListener('pagehide', () => {
  relinquish(); if (key) fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json','X-Boat-Key':key},body:'{"action":"stop"}',keepalive:true}).catch(()=>{});
});
window.addEventListener('keydown', e => { if (e.code === 'Space') { e.preventDefault(); stopAll(); } });
function updateStream() {
  clearTimeout(retryTimer);
  $('preview').textContent = previewOn ? 'Tắt stream' : 'Bật stream';
  $('camera').hidden = !previewOn || document.hidden;
  $('camera').removeAttribute('src');
  $('marker').style.display = 'none';
  if (previewOn && !document.hidden) {
    $('cameraNote').textContent = 'Đang kết nối camera…';
    $('camera').src = `http://${location.hostname}:81/stream?t=${Date.now()}`;
  } else $('cameraNote').textContent = previewOn ? 'Stream tạm dừng' : 'Đã tắt stream';
}
$('preview').onclick = () => { previewOn = !previewOn; updateStream(); };
$('camera').onload = () => { $('cameraNote').textContent = ''; };
$('camera').onerror = () => {
  $('camera').hidden = true;
  $('cameraNote').textContent = 'Mất luồng camera. Đang kết nối lại…';
  clearTimeout(retryTimer);
  if (previewOn && !document.hidden) retryTimer = setTimeout(updateStream, 2000);
};
document.addEventListener('visibilitychange', updateStream);
window.addEventListener('pagehide', () => {
  clearTimeout(retryTimer);
  $('camera').removeAttribute('src');
});
window.addEventListener('pageshow', updateStream);
$('capture').onclick = async () => {
  try {
    const url = URL.createObjectURL(await request(snapshotUrl(), {}, 2000, 'blob')), a = document.createElement('a');
    a.href = url; a.download = `rac-${Date.now()}.jpg`; a.click(); setTimeout(() => URL.revokeObjectURL(url),1000);
  } catch(e) { $('message').textContent = e.message; }
};
async function poll() {
  try {
    render(await request('/api/status', {cache:'no-store'}));
  } catch {
    relinquish(); $('connection').textContent = 'Mất kết nối';
    document.querySelectorAll('[data-drive],#conveyorOn,#conveyorOff,#neutral,#manual,#auto,#clear,#redLed,[data-rgb]').forEach(b => b.disabled = true);
  } finally { setTimeout(poll,350); }
}
setInterval(pump,200); updateStream(); poll();
