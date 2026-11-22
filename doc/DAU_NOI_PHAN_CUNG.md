# Đấu nối GOOUUU ESP32-S3 N16R8-CAM

Bảng này đối chiếu với `include/board_config.h`, sơ đồ `doc/sodomach.jpg` và [pinout board do người dùng cung cấp](https://forum.arduino.cc/t/chinese-esp32-s3-cam/1318290). GPIO là tên tín hiệu, không phải số thứ tự chân trên hàng cắm.

## 1. Driver số 1: hai động cơ di chuyển

Sơ đồ gốc ghi “L298M”. Phần mềm giả định module L298N thông dụng có ENA, IN1–IN4, ENB; đối chiếu chữ in trên module thực tế.

| Chân ESP32-S3 | Chân driver số 1 | Chức năng |
|---|---|---|
| GPIO21 | ENA | PWM tốc độ động cơ trái |
| GPIO2 | IN1 | Chiều động cơ trái |
| GPIO14 | IN2 | Chiều động cơ trái |
| GPIO39 | IN3 | Chiều động cơ phải |
| GPIO47 | IN4 | Chiều động cơ phải |
| GPIO38 | ENB | PWM tốc độ động cơ phải |
| GND | GND | Mass chung |

- OUT1/OUT2 → hai dây động cơ trái.
- OUT3/OUT4 → hai dây động cơ phải.
- Chân nguồn động lực `12V/VS` → cực dương pin qua cầu chì và công tắc.
- **Tháo jumper ENA và ENB** để ESP32 điều khiển PWM. Các jumper này khác với `5V-EN`.
- GPIO2 cũng nối LED chỉ thị trên board; đèn này có thể sáng/tắt theo IN1, không phải LED báo trạng thái độc lập.
- GPIO38/39 dùng chung đường thẻ SD: không cắm/sử dụng thẻ SD với cấu hình này.
- Nếu một động cơ chạy ngược, chỉnh `invertLeft` hoặc `invertRight` sau khi thử cơ cấu thực tế.

## 2. Driver số 2: băng tải

| Chân ESP32-S3 | Chân driver số 2 | Chức năng |
|---|---|---|
| GPIO41 | IN1 | Chiều băng tải |
| GPIO42 | IN2 | Chiều băng tải |
| GND | GND | Mass chung |

- OUT1/OUT2 → hai dây động cơ băng tải.
- `12V/VS` → dương pin qua nhánh bảo vệ phù hợp.
- **Giữ jumper ENA** ở cấu hình mặc định `conveyorEnable = -1`: băng tải bật/tắt ở toàn tốc, chưa chỉnh được tốc độ.
- Kênh B không dùng: OUT3/OUT4 để trống; có thể tháo jumper ENB và kéo ENB xuống GND để tắt kênh.

**Tùy chọn chỉnh tốc độ băng tải:** tháo jumper ENA, nối GPIO40 → ENA, đặt `conveyorEnable = 40` và `redLed = -1`. Khi đó tháo LED đỏ, không nối LED hoặc cảm biến vào GPIO40. Kiểm tra trùng chân khi build sẽ chặn cấu hình dùng chung chân này. Không cần giữ jumper ENB của kênh không dùng.

## 3. LED trong sơ đồ và LED trên board

**Giao diện điện thoại điều khiển được LED RGB WS2812 tích hợp trên GPIO48.** Bảng sau đối chiếu các đèn trong sơ đồ cũ với cấu hình hiện tại.

| LED | Trong sơ đồ gốc | Cách xử lý với cấu hình hiện tại |
|---|---|---|
| LED đỏ ngoài | GPIO40 → 220 Ω → LED → GND | `redLed=-1`. GPIO40 dùng cho TRIG của cảm biến siêu âm ở bản `boat-live` |
| LED vàng ngoài | GPIO48 → 220 Ω → LED → GND | Chưa đấu theo hình: GPIO48 đã nối LED RGB WS2812 trên board |
| LED xanh ngoài | GPIO19 → 220 Ω → LED → GND | Chưa đấu theo hình: GPIO19 là USB D− của cổng USB tích hợp |
| LED RGB tích hợp | GPIO48 | Đã điều khiển bằng WS2812/RMT; chọn tắt/đỏ/vàng/xanh dương/xanh lá/trắng trên điện thoại |
| LED chỉ thị tích hợp | GPIO2 | Dùng chung chân IN1 của driver số 1, không dùng làm báo trạng thái riêng |

Đặt `rgbLed=-1` trong `include/board_config.h` để tắt LED tích hợp. Bộ chọn màu
vẫn hoạt động khi môi trường LED RGB 4 chân được bật. LED tích hợp cũng hoạt động
ở bản `boat-bench` trong khi motor được mô phỏng.

### LED RGB rời 4 chân

Chọn môi trường `boat-live-rgb4` trong PlatformIO khi lắp LED rời. Kéo dây cảm biến
siêu âm vật cản ra khỏi GPIO1 và GPIO40 trước khi gắn LED. Môi trường này tắt cảm
biến đó và dùng GPIO20 cho màu xanh dương. Khi lắp LED, để cổng USB native của
ESP32-S3 trống và dùng cổng **UART/CH340** để nạp hoặc xem Serial.

Ngắt nguồn khi đấu dây. Tra sơ đồ chân theo mã LED hoặc dùng thang diode để xác
định chân chung và từng màu trước khi nối. Mỗi chân màu dùng một điện trở riêng.

| Chân LED | Kết nối |
|---|---|
| R (đỏ) | Qua điện trở **330 Ω** đến GPIO1 |
| G (xanh lá) | Qua điện trở **330 Ω** đến GPIO40 |
| B (xanh dương) | Qua điện trở **330 Ω** đến GPIO20 |
| COM (chung) | **GND** nếu chung âm (common cathode), **3V3** nếu chung dương (common anode) |

Mặc định mã dùng LED chung âm. Với LED chung dương, bỏ dấu `;` trước dòng
`-DBOAT_RGB4_COMMON_ANODE=1` trong môi trường `boat-live-rgb4` rồi biên dịch lại.
Chân COM chung dương dùng **3,3 V**, tránh cấp 5 V hoặc 12 V vào mạch LED này.

Nạp qua cổng UART/CH340 với lệnh sau. Cập nhật `upload_port` trong `platformio.ini`
nếu cổng COM trên máy khác cấu hình hiện tại.

```powershell
scripts\pio.cmd run -e boat-live-rgb4 -t upload
```

Khi kiểm tra LED lần đầu, ngắt nguồn động lực, mở **Trạng thái · LED** trên trang
điều khiển và thử từng màu. LED rời hiển thị cùng màu với LED RGB tích hợp.
Khi quay lại môi trường `boat-live`, tháo LED rời trước khi nối lại cảm biến.

## 4. Camera OV2640

Camera cắm bằng cáp FPC vào socket trên board. Các kết nối dưới đây là đường tín hiệu có sẵn, không phải yêu cầu đấu thêm dây jumper.

| Tín hiệu camera | GPIO |
|---|---:|
| SIOD / SDA | 4 |
| SIOC / SCL | 5 |
| VSYNC | 6 |
| HREF | 7 |
| XCLK | 15 |
| PCLK | 13 |
| D0 / Y2 | 11 |
| D1 / Y3 | 9 |
| D2 / Y4 | 8 |
| D3 / Y5 | 10 |
| D4 / Y6 | 12 |
| D5 / Y7 | 18 |
| D6 / Y8 | 17 |
| D7 / Y9 | 16 |

PWDN và RESET được đặt `-1` trong driver vì không điều khiển bằng GPIO riêng trong cấu hình này. Không dùng các GPIO camera cho driver, LED hoặc cảm biến.

## 5. Đo điện áp pin

```text
Dương pin sau công tắc ── 22 kΩ ──┬── GPIO1
                                 │
                               4,7 kΩ
                                 │
                                GND
```

Điểm giữa hai điện trở là tín hiệu ADC. Với pin Li-ion 3S đầy 12,6 V, điện áp tại ADC khoảng 2,22 V. Hiệu chuẩn `batteryCalibration` bằng đồng hồ; các ngưỡng pin trong mã chỉ là giá trị khởi đầu cho Li-ion 3S và phải khớp pin thực tế.

## 6. Nguồn và mass

| Từ | Đến |
|---|---|
| Pin + qua cầu chì/công tắc | Nguồn động lực của hai driver và IN+ mạch hạ áp |
| Pin − | GND hai driver và IN− mạch hạ áp |
| OUT+ hạ áp đã chỉnh 5 V | Chân 5V board Goouuu |
| OUT− hạ áp | GND board Goouuu |

GND phải nối chung; dây dòng động cơ quay về điểm phân phối nguồn, không đi qua dây mass nhỏ của ESP32. Không cấp pin 3S trực tiếp vào chân 5V/3V3 của board. Khi cấp ngoài, đối chiếu mạch nguồn thực tế trước khi đồng thời cắm USB có cấp 5 V.

**Chân 5V của module L298N phụ thuộc jumper `5V-EN` và thiết kế module:** khi bộ ổn áp trên module được bật, chân đó thường là đầu ra; khi bỏ jumper, module thường cần 5 V logic ngoài. Không nối hai đầu ra 5 V với nhau. Chưa có ảnh/model driver cụ thể nên cần kiểm tra module trước khi chốt jumper này, nhất là với pin 3S lên tới 12,6 V. Firmware không điều khiển phần nguồn logic này.

## 7. Phần chưa đấu trong cấu hình hiện tại

- `intakeSensor = -1`: chưa có chân cảm biến cửa thu.
- `currentAdc = -1`: chưa có chân đo dòng/chống kẹt bằng dòng.
- Không dùng GPIO35–37 vì bộ nhớ PSRAM; GPIO19/20 dành cho USB; GPIO43/44 dành cho UART nạp/Serial.
- Không dùng GPIO48 cho LED đơn khi cần giữ LED RGB tích hợp.
- `boat-bench` không phát tín hiệu điều khiển motor; chạy motor thật bằng `boat-live` sau khi đối chiếu đấu dây và nguồn. Khi thử `boat-bench`, ngắt nguồn động lực vì các chân driver không được điều khiển, có thể thả nổi nếu thiếu điện trở kéo.
