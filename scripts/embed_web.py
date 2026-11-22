from pathlib import Path

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html = (root / "web/index.html").read_text(encoding="utf-8")
js = (root / "web/control.js").read_text(encoding="utf-8")
html = html.replace("/*__CONTROL_JS__*/", js)
if ')BOATPAGE"' in html:
    raise ValueError("Raw string delimiter found in web source")
output = '#pragma once\n#include <Arduino.h>\nstatic const char WEB_PAGE[] PROGMEM = R"BOATPAGE(' + html + ')BOATPAGE";\n'
target = root / "include/web_page.h"
if not target.exists() or target.read_text(encoding="utf-8") != output:
    target.write_text(output, encoding="utf-8")
camera_html = (root / "web/camera.html").read_text(encoding="utf-8")
if ')CAMERAPAGE"' in camera_html:
    raise ValueError("Raw string delimiter found in camera page")
camera_output = '#pragma once\n#include <Arduino.h>\nstatic const char CAMERA_PAGE[] PROGMEM = R"CAMERAPAGE(' + camera_html + ')CAMERAPAGE";\n'
camera_target = root / "include/camera_page.h"
if not camera_target.exists() or camera_target.read_text(encoding="utf-8") != camera_output:
    camera_target.write_text(camera_output, encoding="utf-8")
collector_html = (root / "web/collector.html").read_text(encoding="utf-8")
if ')COLLECTORPAGE"' in collector_html:
    raise ValueError("Raw string delimiter found in collector page")
collector_output = '#pragma once\n#include <Arduino.h>\nstatic const char COLLECTOR_PAGE[] PROGMEM = R"COLLECTORPAGE(' + collector_html + ')COLLECTORPAGE";\n'
collector_target = root / "include/collector_page.h"
if not collector_target.exists() or collector_target.read_text(encoding="utf-8") != collector_output:
    collector_target.write_text(collector_output, encoding="utf-8")
