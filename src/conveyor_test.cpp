#if defined(BOAT_CONVEYOR_TEST) && BOAT_CONVEYOR_TEST
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "app_config.h"
#include "board_config.h"

namespace {
WebServer server(80);
QueueHandle_t commands = nullptr;
constexpr uint32_t runMs = 3000;
int activeDirection = 0;
int pendingDirection = 0;
uint32_t stoppedAt = 0;
uint32_t startedAt = 0;

const char page[] PROGMEM = R"TESTPAGE(
<!doctype html><html lang="vi"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kiểm tra băng tải</title>
<style>
body{font-family:system-ui,sans-serif;max-width:520px;margin:24px auto;padding:16px}
button{font:inherit;min-height:48px;margin:6px;padding:10px 16px}
#status{white-space:pre-line}
</style>
<h1>Kiểm tra băng tải</h1>
<p>Mỗi lần bấm chạy tối đa 3 giây.</p>
<button data-direction="1">Chạy thuận 3 giây</button>
<button data-direction="-1">Chạy ngược 3 giây</button>
<button data-direction="0">Dừng</button>
<p id="status" role="status"></p>
<p id="message" role="status"></p>
<script>
async function request(url,options={}){
  const controller=new AbortController();
  const timer=setTimeout(()=>controller.abort(),1500);
  try{
    const response=await fetch(url,{...options,signal:controller.signal});
    if(!response.ok)throw Error(await response.text());
    return await response.json();
  }finally{clearTimeout(timer);}
}
async function command(direction){
  try{
    await request('/run',{
      method:'POST',headers:{'X-Motor-Control':'1'},body:String(direction)
    });
    document.getElementById('message').textContent=direction===0?'Đã gửi lệnh dừng.':'Đã nhận lệnh chạy thử.';
  }catch(error){document.getElementById('message').textContent='Lỗi gửi lệnh: '+error.message;}
}
document.querySelectorAll('[data-direction]').forEach(button=>{
  button.onclick=()=>command(Number(button.dataset.direction));
});
async function status(){
  try{
    const s=await request('/status',{cache:'no-store'});
    document.getElementById('status').textContent=
      `Đầu ra: ${s.direction===0?'dừng':s.direction===1?'thuận':'ngược'}\nGPIO${s.in1Pin}=${s.in1} · GPIO${s.in2Pin}=${s.in2}`;
  }catch(error){document.getElementById('status').textContent=error.message;}
  setTimeout(status,400);
}
document.addEventListener('visibilitychange',()=>{
  if(document.hidden)fetch('/run',{
    method:'POST',headers:{'X-Motor-Control':'1'},body:'0',keepalive:true
  }).catch(()=>{});
});
status();
</script></html>
)TESTPAGE";

void stopConveyor() {
    if (board::conveyorEnable >= 0) digitalWrite(board::conveyorEnable, LOW);
    digitalWrite(board::conveyorIn1, LOW);
    digitalWrite(board::conveyorIn2, LOW);
    activeDirection = 0;
    pendingDirection = 0;
    stoppedAt = millis();
    Serial.println("[TEST] STOP: both conveyor inputs LOW");
}

void sendStatus() {
    const int in1 = digitalRead(board::conveyorIn1);
    const int in2 = digitalRead(board::conveyorIn2);
    const int direction = (in1 - in2) * (board::invertConveyor ? -1 : 1);
    char json[180];
    snprintf(json, sizeof(json),
        "{\"direction\":%d,\"in1Pin\":%d,\"in2Pin\":%d,\"in1\":%d,\"in2\":%d}",
        direction, board::conveyorIn1, board::conveyorIn2, in1, in2);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}
void webTask(void*) {
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
}

void setup() {
    Serial.begin(115200);
    const int pins[] = {board::leftEnable, board::leftIn1, board::leftIn2,
        board::rightEnable, board::rightIn1, board::rightIn2,
        board::conveyorIn1, board::conveyorIn2, board::conveyorEnable};
    for (int pin : pins) {
        if (pin < 0) continue;
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    stopConveyor();
    commands = xQueueCreate(1, sizeof(int));
    if (!commands) {
        Serial.println("[TEST] Command queue initialization failed");
        return;
    }
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    const bool configured = WiFi.softAPConfig(IPAddress(192,168,4,1),
        IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    const bool started = WiFi.softAP(BOAT_AP_SSID, BOAT_AP_PASSWORD);
    if (!configured || !started) {
        Serial.println("[TEST] Wi-Fi initialization failed");
        return;
    }
    const char* headers[] = {"X-Motor-Control"};
    server.collectHeaders(headers, 1);
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Cache-Control", "no-store");
        server.send_P(200, "text/html; charset=utf-8", page);
    });
    server.on("/status", HTTP_GET, sendStatus);
    server.on("/run", HTTP_POST, []() {
        if (server.header("X-Motor-Control") != "1") {
            server.send(403, "text/plain", "Missing control header");
            return;
        }
        const String body = server.arg("plain");
        if (body != "0" && body != "1" && body != "-1") {
            server.send(400, "text/plain", "Expected 0, 1 or -1");
            return;
        }
        const int direction = body.toInt();
        if (xQueueOverwrite(commands, &direction) != pdPASS) {
            server.send(503, "text/plain", "Command queue unavailable");
            return;
        }
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "application/json", "{\"accepted\":true}");
    });
    server.begin();
    if (xTaskCreatePinnedToCore(webTask, "conveyor-web", 6144, nullptr, 1, nullptr, 0) != pdPASS) {
        server.stop();
        Serial.println("[TEST] HTTP task initialization failed");
        return;
    }
    Serial.printf("[TEST] %s | http://192.168.4.1 | IN1=GPIO%d IN2=GPIO%d EN=%d\n",
        BOAT_AP_SSID, board::conveyorIn1, board::conveyorIn2, board::conveyorEnable);
}

void loop() {
    // The GPIO/deadline loop runs independently of HTTP body reads.
    uint32_t now = millis();
    if (activeDirection != 0 && now - startedAt >= runMs) stopConveyor();
    int direction = 0;
    if (commands && xQueueReceive(commands, &direction, 0) == pdTRUE) {
        stopConveyor();
        pendingDirection = direction;
    }
    now = millis();
    if (pendingDirection != 0 && now - stoppedAt >= config::reverseDeadTimeMs) {
        activeDirection = pendingDirection;
        pendingDirection = 0;
        const int physicalDirection = board::invertConveyor ? -activeDirection : activeDirection;
        digitalWrite(board::conveyorIn1, physicalDirection > 0 ? HIGH : LOW);
        digitalWrite(board::conveyorIn2, physicalDirection < 0 ? HIGH : LOW);
        if (board::conveyorEnable >= 0) digitalWrite(board::conveyorEnable, HIGH);
        startedAt = now;
        Serial.printf("[TEST] RUN=%d GPIO%d=%d GPIO%d=%d duration=%lu ms\n",
            activeDirection, board::conveyorIn1, digitalRead(board::conveyorIn1),
            board::conveyorIn2, digitalRead(board::conveyorIn2),
            static_cast<unsigned long>(runMs));
    }
    delay(1);
}
#endif
