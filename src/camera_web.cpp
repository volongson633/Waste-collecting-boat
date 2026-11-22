#include "web_control.h"
#include "app_config.h"
#include "board_config.h"
#include "vision_service.h"
#include <esp_camera.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cmath>

namespace {
int readPercent(const String& body, unsigned int start) {
    int percent = 0;
    for (unsigned int i = start; i < start + 3; ++i) {
        const char digit = body[i];
        if (digit < '0' || digit > '9') return -1;
        percent = percent * 10 + (digit - '0');
    }
    return percent <= 100 ? percent : -1;
}
}

void WebControl::handlePhoneMotor() {
    if (server_.header("X-Motor-Control") != "1") {
        server_.send(403,"text/plain","Missing control header"); return;
    }
    const auto sendMotorStatus = [this](int code, const String& message, const RuntimeStatus& status) {
        server_.sendHeader("Cache-Control", "no-store");
        server_.sendHeader("X-HC-SR04-Cycle", boat::name(status.ultrasonicCycle));
        server_.send(code, "text/plain", message);
    };
    const String body = server_.arg("plain");
    if (body == "stop") {
        runtime_.operatorPresent();
        if (!runtime_.command(boat::Command())) {
            server_.send(503,"text/plain","Stop command busy"); return;
        }
        phoneOwned_ = false;
        sendMotorStatus(200, "OK", runtime_.status());
        return;
    }
    const bool hasSpeeds = body.length() == 11;
    if ((!hasSpeeds && body.length()!=3) || body[0]<'0' || body[0]>'4' || body[1]!=':' || body[2]<'0' || body[2]>'2') {
        server_.send(400,"text/plain","Expected direction:belt[:drivePercent:beltPercent]"); return;
    }
    const int direction=body[0]-'0';
    const int conveyor=body[2]-'0'; // 0: stop, 1: forward, 2: reverse.
    int speed = 180;
    int conveyorSpeed = 180;
    if (hasSpeeds) {
        const int drivePercent = readPercent(body, 4);
        const int conveyorPercent = readPercent(body, 8);
        if (body[3] != ':' || body[7] != ':' || drivePercent < 0 || conveyorPercent < 0) {
            server_.send(400,"text/plain","Speeds must be three-digit percentages (000-100)"); return;
        }
        speed = (drivePercent * boat::Settings().maxDrive + 50) / 100;
        conveyorSpeed = (conveyorPercent * 255 + 50) / 100;
    }
    boat::Output requested;
    if (direction==1) requested.left=requested.right=speed;
    else if (direction==2) requested.left=requested.right=-speed;
    else if (direction==3) { requested.left=-speed; requested.right=speed; }
    else if (direction==4) { requested.left=speed; requested.right=-speed; }
    requested.conveyor=conveyor==2 ? -conveyorSpeed : conveyor==1 ? conveyorSpeed : 0;
    runtime_.operatorPresent();
    if (!config::outputsEnabled) {
        server_.send(409,"text/plain","Motor outputs disabled: upload boat-live firmware"); return;
    }
    auto status = runtime_.status();
    static String previousPhoneCommand;
    if (body != previousPhoneCommand) {
        Serial.printf("[PHONE] direction=%d conveyor=%d | state=%s reason=%s\n",
            direction, conveyor, boat::name(status.state), boat::name(status.reason));
        previousPhoneCommand = body;
    }
    if (phoneOwned_ && (phoneEpoch_!=status.epoch || status.state!=boat::State::Manual)) phoneOwned_=false;
    if (!phoneOwned_ && !direction && !conveyor) {
        sendMotorStatus(200, "OK", status);
        return;
    }
    if (!phoneOwned_) {
        boat::Command arm;
        arm.action=boat::Action::Manual; arm.epoch=status.epoch;
        if (!runtime_.command(arm)) {
            status=runtime_.status();
            String error="Motor locked: state=";
            error+=boat::name(status.state);
            error+=" reason="; error+=boat::name(status.reason);
            error+=" ready="; error+=status.readyToArm ? "yes" : "no";
            sendMotorStatus(409, error, status);
            return;
        }
        status=runtime_.status(); phoneEpoch_=status.epoch; phoneOwned_=true;
    }
    boat::Command drive;
    drive.action=boat::Action::Drive; drive.epoch=phoneEpoch_; drive.sequence=status.sequence+1;
    drive.left=requested.left; drive.right=requested.right; drive.conveyor=requested.conveyor;
    if (!runtime_.command(drive)) {
        phoneOwned_=false;
        sendMotorStatus(409, "Control session expired; release controls and retry", runtime_.status());
        return;
    }
    sendMotorStatus(200, "OK", runtime_.status());
}

void WebControl::cameraRoutes() {
    server_.on("/capture",HTTP_GET,[this]() {
        runtime_.operatorPresent();
        // Use the incoming host: works on SoftAP and optional station network.
        String host=server_.hostHeader();
        const int colon=host.indexOf(':');
        if (colon>=0) host=host.substring(0,colon);
        server_.sendHeader("Location",String("http://")+host+":82/capture");
        server_.sendHeader("Cache-Control","no-store");
        server_.send(302,"text/plain","");
    });
    server_.on("/status",HTTP_GET,[this]() {
        runtime_.operatorPresent();
        sensor_t* sensor=esp_camera_sensor_get();
        if (!sensor) { server_.send(503,"text/plain","Camera unavailable"); return; }
        StaticJsonDocument<128> doc;
        doc["framesize"]=sensor->status.framesize; doc["quality"]=sensor->status.quality;
        doc["conveyorPwm"] = board::conveyorEnable >= 0;
        String json; serializeJson(doc,json);
        server_.sendHeader("Cache-Control","no-store");
        server_.send(200,"application/json",json);
    });
    server_.on("/control",HTTP_GET,[this]() {
        sensor_t* sensor=esp_camera_sensor_get();
        if (!sensor) { server_.send(503,"text/plain","Camera unavailable"); return; }
        const String variable=server_.arg("var"), value=server_.arg("val");
        char* end=nullptr;
        long number=strtol(value.c_str(),&end,10);
        if (!value.length() || *end) { server_.send(400,"text/plain","Invalid number"); return; }
        int result=-1;
        if (variable=="framesize" && number>=FRAMESIZE_QVGA && number<=FRAMESIZE_UXGA) {
            if (vision::available() && number!=FRAMESIZE_QVGA) {
                server_.send(409,"text/plain","Vision requires QVGA 320x240"); return;
            }
            result=sensor->set_framesize(sensor,static_cast<framesize_t>(number));
        } else if (variable=="quality" && number>=4 && number<=63) result=sensor->set_quality(sensor,number);
        else { server_.send(400,"text/plain","Unsupported camera setting"); return; }
        server_.send(result==0 ? 200 : 500,"text/plain",result==0 ? "OK" : "Camera rejected setting");
    });
    server_.on("/calibration",HTTP_GET,[this]() {
        float points[8]; Preferences prefs;
        const bool opened=prefs.begin("calib",true);
        const bool valid=opened && prefs.getBytes("pts",points,sizeof(points))==sizeof(points);
        if (opened) prefs.end();
        StaticJsonDocument<384> doc; doc["ok"]=valid;
        if (valid) { auto array=doc.createNestedArray("pts"); for (float p:points) array.add(p); }
        String json; serializeJson(doc,json); server_.send(200,"application/json",json);
    });
    server_.on("/calibration",HTTP_POST,[this]() {
        if (server_.header("X-Motor-Control")!="1") { server_.send(403,"text/plain","Missing header"); return; }
        const String body=server_.arg("plain");
        if (!body.length() || body.length()>200) { server_.send(400,"text/plain","Invalid calibration"); return; }
        float points[8]; const char* cursor=body.c_str(); bool valid=true;
        for (int i=0;i<8;++i) {
            char* end=nullptr; points[i]=strtof(cursor,&end);
            if (end==cursor || !std::isfinite(points[i]) || points[i]<0 || points[i]>1 ||
                (i<7 ? *end!=',' : *end!='\0')) { valid=false; break; }
            cursor=end+1;
        }
        if (!valid) { server_.send(400,"text/plain","Expected eight normalized coordinates"); return; }
        Preferences prefs;
        if (!prefs.begin("calib",false)) { server_.send(500,"text/plain","Storage unavailable"); return; }
        const bool saved=prefs.putBytes("pts",points,sizeof(points))==sizeof(points); prefs.end();
        server_.send(saved ? 200 : 500,"text/plain",saved ? "OK" : "Save failed");
    });
}
