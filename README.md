# Bản tích hợp FW-camera

Dự án chính nằm ở `D:\khkt\code`. Đã nhập giao diện FW-camera làm trang chính: stream MJPEG cổng 81, chọn độ phân giải/chất lượng, chụp ảnh, hiệu chỉnh bốn góc và nắn ảnh trên trình duyệt. Các nút motor mới dùng chung Runtime/MotorControl của dự án lớn, không khởi tạo thêm driver hay tác vụ motor trùng lặp.

- Wi-Fi mặc định: **ESP32-S3-CAM**, mật khẩu **12345678**; trang chính **http://192.168.4.1**. Sửa `include/wifi_config.h` để đổi Wi-Fi. Nếu điền mạng nhà, ESP đồng thời giữ điểm truy cập và thử kết nối mạng đó. `include/secrets.h` nếu có vẫn có thể ghi đè cấu hình AP.
- Giữ nút hướng để chạy, thả để dừng; bật/tắt băng tải riêng. Trang tự vào điều khiển thủ công khi có lệnh chạy. Motor thật vẫn yêu cầu đo pin hợp lệ và không có lỗi; terminal 115200 baud in thay đổi đầu ra. Mất lệnh 1 giây sẽ dừng.
- **/dashboard** giữ trạng thái pin, LED, cấu hình tốc độ và chế độ nhận diện/tự động của dự án lớn. Hai trang có quyền điều khiển riêng; dừng phiên đang chạy trước khi chuyển sang trang còn lại để lái.
- Stream: **http://192.168.4.1:81/stream**. Chụp ảnh: **http://192.168.4.1/capture** (chuyển sang cổng 82). Chỉ một máy xem stream đồng thời.
- Vẫn giữ chu trình tự chạy khi không mở trang sau 10 giây. Mở trang sẽ dừng chu trình. `idlePatrolEnabled` trong `include/app_config.h` dùng để bật/tắt tính năng này.
- Build: `scripts\pio.cmd run -e boat-live`; thử mô phỏng bằng `camera-control` hoặc `boat-bench`. Nạp: `scripts\pio.cmd run -e boat-live -t upload --upload-port COM6`.
- Bản sao trước khi ghép: `backups/pre-fw-merge/`. Dự án FW-camera nguồn không bị thay đổi bởi bước ghép này.

# Camera stream và điều khiển thuyền

Khi không có trang điều khiển gửi cập nhật trong 10 giây, ESP tự lặp: tiến 3 giây → lùi 3 giây. Khi mở lại trang, ESP dừng chu trình và chờ điều khiển thủ công. Chỉ kết nối Wi-Fi mà không mở trang vẫn được coi là vắng người điều khiển; đóng trang/khóa màn hình sẽ bắt đầu đếm thời gian vắng. Chu trình này không phát hiện hay tránh vật cản. Bảo vệ pin/kẹt băng tải vẫn có hiệu lực; lỗi phải được xóa thủ công. Sửa `idlePatrolEnabled`, `idlePatrolWaitMs`, `idlePatrolStepMs`, `idlePatrolSpeed` trong `include/app_config.h` để cấu hình. Chế độ này cũng bắt đầu sau khi khởi động xong và chờ đủ 10 giây.

Mặc định hiện tại là `boat-live`: xuất tín hiệu motor thật, yêu cầu đo pin hợp lệ. Chọn `camera-control` để mô phỏng. Serial Monitor 115200 baud in mỗi khi hướng thay đổi, ví dụ `[MOTOR] Đã phát lệnh sang trái`, `[MOTOR] Đã dừng lái` và trạng thái bật/tắt băng tải (hoặc `[MO PHONG]` khi mô phỏng). Log xác nhận tín hiệu điều khiển, không xác nhận chuyển động cơ khí.

Bản `camera-control` chạy camera MJPEG và mô phỏng điều khiển thủ công, không chạy nhận diện. Phần truyền MJPEG tham khảo từ `D:\khkt\FW-camera\FW-camera`; giữ driver và logic điều khiển của dự án hiện tại.

- Kết nối Wi-Fi **ESP32-S3-CAM**, mật khẩu **12345678**, mở **http://192.168.4.1**. Stream tự mở cùng các nút điều khiển.
- Nhấn **Điều khiển thủ công**, giữ **Tiến / Lùi / Trái / Phải** để chạy; thả nút để dừng lái. **Bật băng tải / Tắt băng tải** điều khiển riêng băng chuyền. **DỪNG TẤT CẢ** dừng cả hai.
- Stream MJPEG: **http://192.168.4.1:81/stream** (một người xem đồng thời). Ảnh JPEG: **http://192.168.4.1:82/capture**. API điều khiển dùng cổng 80 riêng. Trang thử kết nối lại khi luồng báo lỗi.
- Build: `scripts\pio.cmd run -e camera-control`. Nạp: `scripts\pio.cmd run -e camera-control -t upload --upload-port COM6` (đổi COM theo máy).
- `camera-control` mô phỏng đầu ra motor. Để chạy motor thật, build/nạp `boat-live`; cấu hình này yêu cầu đo pin hợp lệ. Xem bảng đấu nối bên dưới.
- Stream mặc định VGA 640 × 480; chế độ nhận diện giữ QVGA 320 × 240, chu kỳ lấy ảnh mục tiêu 50 ms khi không chạy nhận diện; tốc độ thực tế phụ thuộc camera và Wi-Fi. Rời trang hoặc mất lệnh quá 1 giây sẽ dừng động cơ và băng tải.

# Thuyền thu gom rác — GOOUUU ESP32-S3 CAM

Dự án PlatformIO + Arduino cho ESP32-S3 N16R8 và OV2640. Cấu hình board theo [pinout người dùng cung cấp](https://forum.arduino.cc/t/chinese-esp32-s3-cam/1318290); xem **[bảng đấu nối driver, camera, LED và nguồn](doc/DAU_NOI_PHAN_CUNG.md)** trước khi nối phần công suất.

## Chức năng đã có trong mã

- Wi-Fi SoftAP và giao diện tiếng Việt trên điện thoại: giữ nút tiến/lùi/trái/phải để lái, thả để dừng lái; nút bật/tắt băng tải, điều khiển LED, trạng thái pin và ảnh camera. Điện thoại kết nối trực tiếp ESP32, không cần router hoặc Internet.
- Camera QVGA/JPEG, xem ảnh định kỳ và lưu ảnh phục vụ tạo dữ liệu. Ghi ảnh ra thẻ microSD trên board (GPIO38/39/40, SD_MMC 1-bit) để thu dữ liệu cả khi ở ngoài tầm Wi-Fi; xem [đấu nối](doc/DAU_NOI_PHAN_CUNG.md).
- Hai động cơ có PWM, giới hạn tốc độ, tăng giảm tốc và thời gian chờ khi đảo chiều.
- Băng tải bật/tắt toàn tốc theo jumper ENA. Không còn chân trống để chỉnh tốc độ băng tải vì GPIO40 thuộc khe thẻ SD.
- Hai động cơ đẩy: jumper ENA/ENB cắm sẵn, PWM trên IN1–IN4 (GPIO2, 14, 21, 47). Bắt buộc có điện trở kéo xuống 10 kΩ ở IN1–IN4.
- Bộ điều khiển chạy riêng mỗi 20 ms. Mất lệnh quá 1 giây, dữ liệu ảnh quá cũ, pin thấp hoặc lỗi sẽ dừng. Phiên lệnh hết hạn không tự được khôi phục bởi gói tin đến muộn.
- Luồng tự động: tìm → xác nhận mục tiêu qua các khung hình → căn hướng → tiếp cận → thu có thời hạn → dừng. Cần bấm chạy lại cho lượt tiếp theo.
- Có thể bổ sung cảm biến cửa thu và dòng băng tải qua cấu hình. Mặc định chưa gán GPIO, chưa có chống kẹt bằng dòng khi chưa lắp cảm biến.
- Không còn LED đỏ ngoài (GPIO40 thuộc thẻ SD). Chọn tắt/đỏ/vàng/xanh dương/xanh lá/trắng cho LED RGB WS2812 tích hợp GPIO48. Hai LED đơn vàng/xanh trong sơ đồ gốc không được dùng. LED hoạt động thật cả ở `boat-bench`, độc lập với việc arm motor; lệnh LED không gia hạn thời gian chạy motor.
- Bộ đếm là chu kỳ thu đã chạy, chưa phải số rác xác nhận vào giỏ.

**Chưa có mô hình AI rác đã huấn luyện hoặc dữ liệu thực nghiệm trên thuyền.** Phần mềm không thay thế các bước lắp ráp, hiệu chuẩn và thử trên nước trong kế hoạch.

## Các môi trường PlatformIO

| Môi trường | Motor | Nhận diện | Sử dụng |
|---|---|---|---|
| `boat-bench` (mặc định) | Mô phỏng, không ghi chân motor | Vật màu đỏ, thuật toán thử đơn giản | Thử camera, giao diện và logic trên bàn; ngắt nguồn động lực |
| `boat-live` | Thật | Chưa có mô hình | Điều khiển thủ công và thu dữ liệu |
| `boat-ai` | Thật | Edge Impulse | Chỉ build sau khi thêm thư viện mô hình và `ei_model.h` |
| `data-collect` | Thật | Không | Firmware riêng để thu dữ liệu: stream camera, lái tay + chụp tay, tự chạy + tự chụp vào thẻ SD |
| `test-windows` | Không dùng board | Kiểm thử C++ trên Windows | Kiểm tra logic điều khiển |
| `test-native` | Không dùng board | Kiểm thử C++ | Linux/macOS có g++ |

`boat-bench` nhận biết tâm các điểm màu đỏ, không phải AI phân biệt rác. Khi có nhiều vật đỏ, tâm có thể nằm giữa các vật. Không dùng kết quả này để đánh giá độ chính xác nhận diện rác.

## Build và nạp

1. Mở thư mục dự án trong VS Code có extension PlatformIO. Chọn môi trường `boat-bench`.
2. Ngắt nguồn motor, nối cáp USB dữ liệu vào cổng nối CH340 trên board. `ARDUINO_USB_CDC_ON_BOOT=0` dùng Serial qua UART này. Không cắm cáp vào cổng USB OTG: GPIO19 là TRIG siêu âm.
3. Chọn **Build**, sau đó **Upload** và **Monitor** 115200 baud. Nếu cần, chỉ định `upload_port`/`monitor_port` đúng cổng COM trong cấu hình môi trường; chưa có cổng COM cố định trong dự án.
4. Kết nối điện thoại vào Wi-Fi `ESP32-S3-CAM`, mật khẩu mặc định `12345678`, mở `http://192.168.4.1`.
5. Bấm **Bật xem ảnh**, đưa một vật đỏ vào khung hình để thử. Chọn thủ công hoặc tự động để xem đầu ra mô phỏng. Không kết luận chiều quay motor từ kết quả mô phỏng.
6. Sau khi nguồn, chiều quay, kéo mức lúc boot và bộ chia áp đã kiểm tra, chuyển `boat-live`, Build và Upload. Mạch đo pin phải cho số đọc hợp lệ trước khi được bật chế độ chạy.

Trên điện thoại: chọn **Điều khiển thủ công**, giữ **Tiến/Lùi/Trái/Phải** để lái; dùng hai thanh tốc độ bên trái camera. Thanh bánh lái đặt mức từ 0 đến 100% của giới hạn lái trong firmware. Thanh băng chuyền điều chỉnh PWM khi chân ENA nối với GPIO PWM. Với jumper ENA, băng chuyền chạy toàn tốc khi bật và thanh tốc độ bị khóa. Nhấn **Bật băng tải** một lần để chạy và **Tắt băng tải** để dừng; băng tải không tắt chỉ vì thả nút lái. Nút **DỪNG TẤT CẢ** tắt motor, băng tải và LED. Rời trang, khóa màn hình hoặc mất lệnh quá 1 giây sẽ dừng phần động lực; phiên chạy không tự tiếp tục khi kết nối trở lại.

Khối **Điều khiển LED** dùng được khi motor đang dừng, kể cả chưa nối pin 3S. LED đỏ bật/tắt bằng nút riêng; các nút màu điều khiển một LED RGB có sẵn trên board. LED được tắt khi khởi động, đổi phiên chạy, dừng hoặc phát sinh lỗi. Khi đang STOP và chỉ bật LED, đèn giữ trạng thái đến lệnh tắt/dừng hoặc reset; không có heartbeat motor cho chế độ chỉ thử LED.

Lệnh trong PlatformIO terminal:

```sh
pio run -e boat-bench
pio run -e boat-bench -t upload
pio device monitor -b 115200
pio run -e boat-live
pio test -e test-windows
scripts\pio.cmd device monitor -e camera-control -p COM6
```

Đổi `COM6` thành cổng COM của board. Lệnh monitor sẽ hiện log motor trong terminal; nếu đang chạy firmware `boat-live`, đổi `-e camera-control` thành `-e boat-live`.

Máy hiện tại có bộ công cụ cục bộ trong `.tools`/`.pio-core` (không đưa vào git). Có thể dùng `scripts\pio.cmd run -e boat-bench` và `scripts\pio.cmd test -e test-windows` từ thư mục gốc nếu lệnh `pio` chưa có trên PATH. File `pio.cmd` dùng được cả khi PowerShell chặn chạy `.ps1`. Script này dùng `.pio/verified-build` và `.pio/verified-libdeps` riêng để tránh đụng thư mục build khi extension IDE đang thao tác; các lệnh `pio` thông thường vẫn dùng `.pio/build`.

Nền tảng khóa ở `espressif32@6.12.0`, Arduino core 2.0.17; Flash QIO 16 MB, PSRAM OPI 8 MB. Không tự nâng sang Arduino 3.x vì API PWM khác. Partition có hai vùng ứng dụng 6 MB nhưng firmware chưa triển khai cập nhật OTA.

## Cấu hình cần hiệu chỉnh

- `include/board_config.h`: GPIO, chiều motor, chân cảm biến và LED; có kiểm tra trùng chân khi build. GPIO48 dành cho WS2812 tích hợp; GPIO38–40 thuộc khe thẻ SD (`sdEnabled`); đặt `rgbLed = -1` nếu board không có WS2812.
- `include/app_config.h`: camera mirror/flip, hiệu chuẩn ADC, ngưỡng pin, cảm biến dòng. Các ngưỡng 10,2/10,8 V chỉ là điểm bắt đầu cho Li-ion 3S; chốt theo pin thật.
- `lib/BoatCore/src/boat/Controller.h`: tốc độ, timeout, số khung hình xác nhận và vùng cửa thu `collectY`. Vùng này phải kiểm tra với góc camera cố định, không phải phép đo khoảng cách.
- Sao chép `include/secrets.example.h` thành `include/secrets.h` để đổi SSID/mật khẩu trước thử nghiệm ngoài phòng.
- Camera dùng LEDC channel 0/timer 0; motor dùng channel 2/3/4, tránh dùng timer camera cho PWM motor.

Động cơ dừng lúc khởi động trong bản live; vẫn phải lắp điện trở kéo xuống phù hợp tại enable/IN của driver để giữ dừng trong thời gian ESP chưa khởi tạo GPIO hoặc đang reset. Bản bench không ghi các chân motor nên phải ngắt nguồn động lực.

## Thu ảnh và tích hợp AI

Camera snapshot: `http://192.168.4.1:82/capture`. Giao diện có nút lưu từng ảnh. Từ máy tính đã kết nối Wi-Fi của thuyền:

```sh
python scripts/capture_dataset.py --session ao_sang_01 --count 100 --interval 1 --notes "Chai nhua, mat nuoc yen"
```

Ảnh lưu dưới `data/captures/<session>/`, kèm nhật ký. Chương trình lưu JPEG định kỳ, không quay MP4 và không tự gán nhãn. Chia tập huấn luyện/kiểm tra theo phiên chụp, không chia ngẫu nhiên các khung hình liên tiếp. Xem [hướng dẫn Edge Impulse](doc/EDGE_IMPULSE.md).

## Firmware thu dữ liệu (`data-collect`)

Firmware riêng, dùng chung `platformio.ini`, cấu hình chân và các module camera/motor/thẻ SD; mã nằm trong `src/collector/` và trang điện thoại `web/collector.html`.

```sh
pio run -e data-collect -t upload
```

Kết nối Wi-Fi `ESP32-S3-CAM`, mở `http://192.168.4.1`:

- **Bằng tay:** giữ nút ▲▼◀▶ để chạy, thả để dừng; nút **Băng tải** bật/tắt; **Chụp ảnh** lưu một ảnh vào thẻ SD (không có thẻ thì tải ảnh về điện thoại).
- **Tự động:** chọn **Chụp mỗi** 0,2–10 giây và tốc độ, bấm **Bắt đầu tự chạy và chụp**. Thuyền chạy thẳng 4 giây rồi xoay 1,5 giây, lần lượt phải/trái, và lưu ảnh theo khoảng đã chọn. Đổi tốc độ và khoảng chụp được khi đang chạy. Cần thẻ SD.
- Tự chạy chỉ hoạt động khi trang đang mở: trang gửi tín hiệu mỗi 300 ms, mất quá 1 giây thì thuyền dừng và ngừng ghi. Ẩn/đóng trang cũng dừng.
- Siêu âm phát hiện vật, kẹt băng tải hoặc thùng đầy sẽ dừng lượt chạy như firmware chính; bấm **Xóa lỗi để chạy lại**.
- Mỗi lượt tự chạy tạo thư mục `cap_NNNN` mới; ảnh chụp tay gom vào một thư mục đến khi bắt đầu tự chạy. Cột `trigger` trong `index.csv` ghi `auto` hoặc `manual`.
- Chọn độ phân giải trong **Cài đặt camera**; QVGA 320×240 trùng với ảnh đầu vào của mô hình AI (`boat-ai`).
- Không bật chế độ tự chạy khi vắng người (`idlePatrol`) của firmware chính.

Thời gian chạy thẳng/xoay và tốc độ xoay chỉnh trong `include/app_config.h` (`collectorStraightMs`, `collectorTurnMs`, `collectorTurnPercent`).

## Kiến trúc và giao tiếp

Tác vụ camera xử lý ảnh ở core 0; tác vụ điều khiển ở core 1 ưu tiên cao hơn web. JPEG gửi qua HTTP port 81 bằng bản sao ảnh; mạng chậm không giữ khóa bộ điều khiển. `loop()` phục vụ web/API port 80.

- `GET /api/status`: trạng thái, pin, đầu ra, mục tiêu, thời gian ảnh, `epoch`, khóa API theo lần boot.
- `POST /api/command`: JSON và header `X-Boat-Key` lấy từ status. Các action: `manual`, `auto`, `drive`, `heartbeat`, `stop`, `clear`.
- `manual`/`auto`/`clear` cần `epoch` hiện tại. Arm tạo epoch mới. `drive`/`heartbeat` cần epoch phiên đã arm và `sequence` tăng dần.
- `drive`: `left`, `right` trong −255..255 (bị giới hạn thêm bởi cấu hình), `conveyor` 0..255. Ở manual cần gửi đầy đủ lệnh mỗi 200 ms. `heartbeat` chỉ gia hạn tự động; xem status không gia hạn phiên chạy.
- `stop` không cần epoch, luôn có ưu tiên dừng; không xóa lỗi đã chốt. Sau lỗi, xử lý nguyên nhân, bấm xóa lỗi rồi arm lại.
- `POST /api/led`: cùng header xác thực, JSON `{ "epoch": 10, "revision": 2, "red": true, "rgb": "yellow" }`. Lấy `epoch` và `lights.revision` từ status hiện tại. Cập nhật đồng thời trạng thái hai LED; revision chống gói cũ ghi đè trạng thái mới. Các màu hợp lệ: `off`, `red`, `yellow`, `blue`, `green`, `white`. Lệnh này không thay đổi lệnh lái/băng tải hoặc gia hạn phiên chạy.

Kiểm tra giao diện bằng Node: `node --test test/web/control.test.cjs`. Bộ kiểm thử chạy các handler thật của trang với API mô phỏng: hướng lái, nhả nút, bật/tắt băng tải, LED, gói trạng thái cũ, mất mạng và rời trang. Cần kiểm tra thêm trên điện thoại nối board thật.

Không mở API này ra Internet. Thiết kế dùng cho mạng AP riêng trong vùng thử nghiệm có giám sát.

## Kiểm thử phần cứng còn cần thực hiện

1. Nguồn và camera khi motor cùng chạy; không reset, không quá nhiệt.
2. Chiều trái/phải, đảo chiều có dừng, băng tải đúng hướng.
3. Nhả nút, khóa màn hình, tắt Wi-Fi, reset board: đầu ra dừng như thiết kế.
4. Đo pin bằng đồng hồ, xác nhận ngưỡng và tình huống mất dây ADC.
5. Thử AI bằng dữ liệu chưa huấn luyện; đo độ trễ thực trước khi tăng tốc.

Xem [kế hoạch dự án](doc/KE_HOACH_DU_AN.md). Kiểm thử C++ và build không xác nhận chất lượng cơ khí, điện hoặc độ chính xác AI ngoài thực tế.

### Cảm biến siêu âm

Với một cảm biến, chọn một vị trí lắp. Hướng về phía trước thì cấu hình `obstacleTrig` và `obstacleEcho`. Hướng xuống thùng thì cấu hình `binTrig` và `binEcho`. Có thể dùng cả hai vị trí khi có hai cảm biến. Điền số GPIO thực tế trong `include/board_config.h` sau khi xác nhận chân còn trống. Cấu hình hiện tại dùng cảm biến phía trước. Nối dây cam VCC vào 5 V, vàng TRIG vào GPIO19 (không cắm cáp cổng USB OTG), xanh lá ECHO vào GPIO1 qua chia áp (1 kΩ từ ECHO tới GPIO1, 2 kΩ từ GPIO1 xuống GND), và xanh biển GND vào GND chung. Tháo mạch chia áp đo pin cũ khỏi GPIO1. Firmware tắt đo điện áp pin và bảo vệ pin thấp dựa trên ADC trong cấu hình này. Với cảm biến có ECHO 5 V, cần hạ mức tín hiệu xuống 3,3 V trước khi nối vào ESP32.

Các ngưỡng khởi điểm trong `include/app_config.h` là 8 cm cho thùng đầy và 45 cm cho vật cản. Đo và hiệu chỉnh theo kích thước thùng, góc đặt cảm biến và khoảng dừng thực tế. Khi cảm biến phía trước phát hiện vật trong một phiên điều khiển đang hoạt động, bánh lái dừng và băng tải chạy tiến khoảng một vòng. `conveyorRevolutionMs` trong `include/app_config.h` đặt thời gian này là 3.000 ms theo số đo của băng tải. Trang Camera thuyền báo khi chu kỳ bắt đầu, kết thúc hoặc dừng sớm. HC-SR04 phát hiện vật phía trước, còn số vòng được ước lượng theo thời gian chạy. Sau đó thuyền dừng, chờ vật rời đi và người dùng xóa lỗi để chạy lại. Nút DỪNG TẤT CẢ, hết hạn lệnh và lỗi kẹt đều có thể ngắt chu kỳ sớm. Nếu cảm biến trong thùng phát hiện thùng đầy thì bộ điều khiển dừng ngay. Mẫu không có echo có thể do ngoài tầm đo, phản xạ kém hoặc mất kết nối.

Tự quay về bờ cần nguồn định vị và hướng, điểm về đã lưu và dữ liệu vật cản để lập đường đi.
