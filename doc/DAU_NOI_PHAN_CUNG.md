# Đấu nối GOOUUU ESP32-S3 N16R8-CAM

Bảng này đối chiếu với `include/board_config.h`, sơ đồ `doc/sodomach.jpg` và [pinout board do người dùng cung cấp](https://forum.arduino.cc/t/chinese-esp32-s3-cam/1318290). GPIO là tên tín hiệu, không phải số thứ tự chân trên hàng cắm.

## 1. Driver số 1: hai động cơ di chuyển

Sơ đồ gốc ghi “L298M”. Phần mềm giả định module L298N thông dụng có ENA, IN1–IN4, ENB; đối chiếu chữ in trên module thực tế.

| Chân ESP32-S3 | Chân driver số 1 | Chức năng |
|---|---|---|
| (jumper) | ENA | **Cắm jumper**, luôn bật |
| GPIO2 | IN1 | PWM động cơ trái, chiều tiến |
| GPIO14 | IN2 | PWM động cơ trái, chiều lùi |
| GPIO21 | IN3 | PWM động cơ phải, chiều tiến |
| GPIO47 | IN4 | PWM động cơ phải, chiều lùi |
| (jumper) | ENB | **Cắm jumper**, luôn bật |
| GND | GND | Mass chung |

- OUT1/OUT2 → hai dây động cơ trái.
- OUT3/OUT4 → hai dây động cơ phải.
- Chân nguồn động lực `12V/VS` → cực dương pin qua cầu chì và công tắc.
- **Cắm jumper ENA và ENB.** Tốc độ được điều bằng PWM 1 kHz trên chân IN: chạy tiến thì PWM ở IN1 (IN3) và IN2 (IN4) giữ mức thấp, chạy lùi thì ngược lại. Trong mỗi chu kỳ PWM, lúc tắt là hãm động cơ. Các jumper này khác với `5V-EN`.
- **Bắt buộc lắp điện trở kéo xuống 10 kΩ** từ IN1, IN2, IN3, IN4 xuống GND. Vì EN luôn bật, chân IN thả nổi lúc ESP32 khởi động hoặc reset sẽ làm động cơ quay.
- GPIO2 cũng nối LED chỉ thị trên board; đèn này có thể sáng/tắt theo IN1, không phải LED báo trạng thái độc lập.
- GPIO38, 39 và 40 dành cho khe thẻ microSD trên board (xem mục "Thẻ nhớ microSD"), không nối dây gì khác vào các chân này.
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

Không còn GPIO trống để chỉnh tốc độ băng tải: GPIO40 trước đây dùng cho tùy chọn này nay thuộc khe thẻ SD.

## 2a. Thẻ nhớ microSD (khe trên board)

Khe thẻ nối sẵn trên board, không cần đấu dây. Firmware dùng chế độ SD_MMC 1-bit.

| GPIO | Tín hiệu thẻ |
|---|---|
| GPIO39 | CLK |
| GPIO38 | CMD |
| GPIO40 | DATA0 |

- Thẻ phải định dạng **FAT32** (hoặc FAT16 với thẻ nhỏ). Core Arduino 2.0.17 tắt exFAT (`FF_FS_EXFAT 0`), nên thẻ SDXC 64 GB trở lên mặc định exFAT phải định dạng lại sang FAT32. Firmware không tự định dạng thẻ.
- Trên trang camera, chọn khoảng thời gian trong **Cài đặt camera → Ghi SD mỗi** rồi bấm **Ghi thẻ SD**. Ảnh lưu vào `/cap_0001/00000.jpg`, `00001.jpg`…, kèm `index.csv` ghi thời điểm (ms từ lúc khởi động), kích thước và cách chụp (`auto`/`manual`) của từng ảnh. Mỗi lần bấm ghi tạo một thư mục `cap_NNNN` mới.
- Ảnh có cùng độ phân giải với luồng camera: VGA ở `boat-live`, QVGA 320×240 ở `boat-ai`; chọn được trong `data-collect`.
- Ghi thẻ chạy trong tác vụ riêng có hàng đợi 4 ảnh. Thẻ chậm hơn camera thì ảnh thừa bị bỏ và được đếm trong trạng thái, không làm chậm camera, nhận diện hay động cơ.
- Tự dừng ghi khi thẻ còn dưới 8 MB hoặc ghi lỗi 3 lần liên tiếp.
- API: `GET /sd/status`; `POST /sd/record?action=start&interval=1000` hoặc `?action=stop`, kèm header `X-Motor-Control: 1`.
- Thẻ SD hút dòng tăng vọt khi ghi. Nếu board reset khi động cơ và thẻ cùng chạy, thêm tụ lọc gần board và kiểm tra lại nguồn 5 V.

## 3. LED trong sơ đồ và LED trên board

**Giao diện điện thoại điều khiển được LED RGB WS2812 tích hợp trên GPIO48.** Bảng sau đối chiếu các đèn trong sơ đồ cũ với cấu hình hiện tại.

| LED | Trong sơ đồ gốc | Cách xử lý với cấu hình hiện tại |
|---|---|---|
| LED đỏ ngoài | GPIO40 → 220 Ω → LED → GND | Không dùng: GPIO40 thuộc khe thẻ SD |
| LED vàng ngoài | GPIO48 → 220 Ω → LED → GND | Chưa đấu theo hình: GPIO48 đã nối LED RGB WS2812 trên board |
| LED xanh ngoài | GPIO19 → 220 Ω → LED → GND | Không dùng: GPIO19 nay là TRIG của cảm biến siêu âm |
| LED RGB tích hợp | GPIO48 | Đã điều khiển bằng WS2812/RMT; chọn tắt/đỏ/vàng/xanh dương/xanh lá/trắng trên điện thoại |
| LED chỉ thị tích hợp | GPIO2 | Dùng chung chân IN1 của driver số 1, không dùng làm báo trạng thái riêng |

Đặt `rgbLed=-1` trong `include/board_config.h` để tắt LED tích hợp. Bộ chọn màu
vẫn hoạt động khi motor đang dừng.

### LED RGB rời 4 chân

Không còn hỗ trợ. LED này cần GPIO40 cho màu xanh lá, nay thuộc khe thẻ SD; môi trường `boat-live-rgb4` đã bị bỏ. Nếu bật lại cờ `BOAT_RGB4_ENABLED`, build sẽ báo lỗi.

### Cảm biến siêu âm HC-SR04 phía trước

| Dây | Kết nối |
|---|---|
| VCC (cam) | 5 V |
| TRIG (vàng) | **GPIO19** |
| ECHO (xanh lá) | GPIO1 qua chia áp: 1 kΩ từ ECHO tới GPIO1, 2 kΩ từ GPIO1 xuống GND |
| GND (xanh biển) | GND chung |

GPIO19 là chân D− của cổng USB **OTG**. Firmware chuyển chân này sang GPIO thường, nên **không cắm cáp vào cổng OTG**; nạp code và xem Serial qua cổng **TTL** (CH340).

### Cảm biến siêu âm HC-SR04 báo thùng đầy

Gắn phía trên thùng, đầu dò hướng thẳng xuống mặt rác.

| Dây | Kết nối |
|---|---|
| VCC | 5 V |
| TRIG | **GPIO20** |
| ECHO | **GPIO46** qua chia áp: 1 kΩ từ ECHO tới GPIO46, 2 kΩ từ GPIO46 xuống GND |
| GND | GND chung |

- GPIO20 là chân D+ của cổng USB OTG (cổng này không dùng). GPIO46 là chân strapping: điện trở 2 kΩ của mạch chia áp giữ chân này ở mức thấp lúc khởi động, **không bỏ mạch chia áp**.
- Thùng đầy khi mặt rác cách cảm biến ≤ 8 cm, hết đầy khi > 12 cm (`binFullCm`, `binClearCm` trong `include/app_config.h`); cần 3 lần đo liên tiếp. HC-SR04 đo được từ khoảng 2 cm.
- Hai cảm biến đo xen kẽ, mỗi cảm biến khoảng 5 lần/giây. Thùng đầy thì dừng động cơ, băng tải và báo `bin_full`; đổ thùng rồi bấm xóa lỗi để chạy lại.
- Khoảng cách đo được hiện trên trang `data-collect` để chỉnh ngưỡng theo thùng thật.
- Sau khi gắn cảm biến này, board không còn GPIO trống.

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
- Không dùng GPIO35–37 vì bộ nhớ PSRAM; GPIO43/44 dành cho UART nạp/Serial. GPIO19 và GPIO20 dùng cho TRIG của hai cảm biến siêu âm (cổng USB OTG không dùng được); GPIO46 là ECHO cảm biến thùng.
- Không còn GPIO trống. GPIO0, 3, 45 là chân strapping, không dùng.
- Không dùng GPIO48 cho LED đơn khi cần giữ LED RGB tích hợp.
- Khi thử trên bàn, ngắt nguồn động cơ hoặc kê thuyền lên; các chân driver phải có điện trở kéo xuống.
