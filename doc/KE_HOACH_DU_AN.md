# Kế hoạch thực hiện thuyền thu gom rác dùng ESP32-S3 CAM

Ngày lập: 09/11/2026. Thời lượng dự kiến: 8 tuần, khoảng 10–15 giờ/tuần; dự phòng thêm 1–2 tuần nếu chờ linh kiện hoặc phải chỉnh cơ khí. Tuần 1 tính từ ngày bắt đầu thực hiện, không phải lịch cam kết hoàn thành.

**Cập nhật triển khai phần mềm 11/11/2026:** đã tạo dự án PlatformIO/Arduino, cấu hình GPIO theo pinout Goouuu được cung cấp, điều khiển motor/băng tải, đo pin, camera, giao diện Wi-Fi, máy trạng thái và adapter Edge Impulse. Xem `README.md`, `doc/DAU_NOI_PHAN_CUNG.md` và `doc/EDGE_IMPULSE.md`. Chưa có mô hình AI rác đã huấn luyện; các bước lắp ráp và nghiệm thu phần cứng dưới đây vẫn cần thực hiện thực tế, không được coi là đã đạt chỉ nhờ build mã.

## 1. Mục tiêu và phạm vi

Xây dựng thuyền mô hình sử dụng ESP32-S3 N16R8-CAM Goouuu và camera OV2640, điều khiển hai động cơ di chuyển và một động cơ băng tải, có hai chế độ:

- **Thủ công:** người dùng điều khiển tiến, rẽ, dừng và băng tải qua giao diện điện thoại trong mạng Wi-Fi thử nghiệm.
- **Tự động:** phát hiện một nhóm rác nổi đã huấn luyện, căn hướng, tiếp cận chậm và đưa rác lên băng tải trong vùng thử nghiệm được giám sát.

Bản đầu thử trên nước yên, trong khu vực giới hạn, có người giám sát và phương án thu hồi thuyền. Chưa đưa định vị GPS, tự quay về, tránh mọi chướng ngại vật, chạy đường dài hoặc nhận diện mọi loại rác vào phạm vi nghiệm thu.

Ưu tiên hoàn thành theo thứ tự: nguồn và cơ khí → điều khiển thủ công → thu gom thực tế → camera và nhận diện → điều khiển tự động.

## 2. Những thông tin cần chốt ở tuần 1

| Thông tin | Cách xác minh | Quyết định phụ thuộc |
|---|---|---|
| Phiên bản board Goouuu, pinout camera, cổng USB | Ảnh rõ hai mặt board, tài liệu người bán/nhà sản xuất, chương trình camera mẫu phù hợp | Bảng GPIO cuối cùng |
| Hai động cơ di chuyển và cơ cấu tạo lực đẩy | Nhãn động cơ, điện áp, dòng định mức, dòng khởi động/kẹt theo tài liệu hoặc phép đo có giới hạn dòng | Driver, dây, cầu chì, dung lượng pin |
| Động cơ và cơ cấu băng tải | Tỉ số truyền, dòng không tải/có tải, kích thước rác dự kiến | Driver băng tải, tốc độ, chống kẹt |
| Loại pin, dung lượng, dòng xả, BMS | Nhãn và datasheet bộ pin | Bộ sạc, ngưỡng pin thấp và thời gian hoạt động |
| Tấm pin và mạch sạc năng lượng mặt trời | Điện áp/dòng, loại pin và số cell được hỗ trợ | Khả năng ghép với bộ pin thực tế |
| Thân thuyền, tải mang và cửa thu | Cân tổng tải, thử nổi, đo độ nghiêng và độ ngập | Vị trí pin, camera, băng tải |
| Loại rác và nơi trình diễn | Chọn vật mẫu, kích thước, ánh sáng, mặt nước | Dữ liệu huấn luyện và bài thử nghiệm thu |

Không thử giữ kẹt động cơ trực tiếp với pin để đo dòng nếu chưa có phương án giới hạn dòng và thời gian. Khi chưa rõ thông số, ưu tiên tài liệu động cơ hoặc phép thử trên nguồn có giới hạn dòng.

## 3. Kiến trúc dự kiến

```text
OV2640 → thu ảnh → tiền xử lý → mô hình nhận diện → mục tiêu + thời điểm quan sát
                                                        ↓
Điện thoại → chế độ/lệnh → bộ điều khiển trạng thái → driver trái/phải
                                  ↑                 → driver băng tải
                     pin / cảm biến cửa thu / dòng băng tải
```

Chỉ bộ điều khiển trung tâm được phát lệnh động cơ. Nhận diện cung cấp vị trí mục tiêu và độ tin cậy; giao diện cung cấp yêu cầu của người dùng. Quy tắc dừng có ưu tiên cao nhất.

Các trạng thái dự kiến: `STOP`, `MANUAL`, `SEARCH`, `ALIGN`, `APPROACH`, `COLLECT`, `FAULT`. Khởi động và khởi động lại luôn về `STOP`; không tự tiếp tục chạy sau reset.

Camera đơn dùng để căn hướng. Chưa coi vị trí hoặc kích thước vật trên ảnh là khoảng cách thực nếu chưa hiệu chuẩn. Nếu dùng FOMO, không dùng kích thước ô phát hiện làm kích thước thật của rác. Việc kích hoạt thu gom dựa trên vùng ảnh đã thử nghiệm và cảm biến cửa thu nếu cảm biến đạt yêu cầu.

## 4. Lịch thực hiện 8 tuần

### Tuần 1 — Xác minh thiết bị và chốt thiết kế

- [ ] Kiểm kê linh kiện đang có, chụp nhãn và ghi thông số.
- [ ] Xác minh board, Flash/PSRAM và chạy camera độc lập.
- [ ] Lập bảng GPIO: camera, bộ nhớ, USB, động cơ, ADC, cảm biến và LED.
- [ ] Đo/tra dòng động cơ; quyết định giữ hay thay L298N.
- [ ] Kiểm tra tính tương thích pin–BMS–mạch sạc; tạm tách năng lượng mặt trời khỏi giai đoạn chạy thử ban đầu.
- [ ] Chọn cấu trúc thân thuyền, vị trí băng tải và loại rác mẫu.
- [ ] Chốt danh sách vật tư cần bổ sung, chi phí và thời gian giao trước khi mua.

**Đầu ra:** bảng linh kiện, bảng GPIO không trùng chức năng, sơ đồ đấu dây sửa đổi, thiết kế bố trí cơ khí và danh sách mua sắm.

**Điều kiện qua mốc:** camera hoạt động trên đúng board; biết điện áp và yêu cầu dòng của tải; không còn nghi vấn về đường cấp nguồn trước khi đấu toàn hệ thống.

### Tuần 2 — Nguồn và điều khiển động cơ trên bàn

- [ ] Lắp pin, cầu chì, công tắc/ngắt động lực và nhánh nguồn theo sơ đồ đã xác minh.
- [ ] Cấp ESP32 qua mạch hạ áp riêng; đi dây hồi dòng động cơ về điểm phân phối, tránh chạy dòng động cơ qua dây GND của board.
- [ ] Bổ sung lọc/khử nhiễu theo kết quả đo và đặc tính động cơ, driver.
- [ ] Viết điều khiển từng động cơ: chiều, PWM, tăng/giảm tốc và dừng.
- [ ] Bố trí chân enable và trạng thái điện trở kéo để động cơ không tự chạy trong lúc boot/reset.
- [ ] Với băng tải, ưu tiên điều khiển enable/PWM; nếu dùng L298N, tháo jumper enable của kênh được điều khiển sau khi kiểm tra module.
- [ ] Đo và hiệu chuẩn điện áp pin; xác định ngưỡng cảnh báo/ngắt theo bộ pin thực tế.
- [ ] Chạy camera và ba động cơ đồng thời dưới tải đại diện; ghi điện áp, reset và nhiệt độ driver.

**Đầu ra:** chương trình thử phần cứng, bảng chiều quay/PWM, số đo nguồn và sơ đồ đấu dây cập nhật.

**Điều kiện qua mốc:** không tự chạy khi bật nguồn/reset; dừng được bằng phần cứng và phần mềm; không reset hoặc vượt giới hạn linh kiện trong bài chạy đồng thời ít nhất 15 phút. Với động cơ cần nước để làm mát, thực hiện bài thử trong điều kiện phù hợp.

### Tuần 3 — Cơ khí và thu gom thủ công

- [ ] Lắp phao/thân, cơ cấu đẩy, cửa dẫn rác, băng tải và giỏ chứa.
- [ ] Cân bằng tải; bố trí hộp điện, đầu nối và đường dây chống nước bắn.
- [ ] Tạo giao diện điện thoại: tiến, rẽ, dừng, băng tải, chế độ và điện áp pin.
- [ ] Cài thời hạn hiệu lực lệnh; khi mất điều khiển quá thời hạn, đưa hệ thống về dừng. Mốc ban đầu đề xuất: 1 giây, cần đo lại khi thử.
- [ ] Thử nổi và di chuyển ở tốc độ thấp trong khu vực giới hạn.
- [ ] Thử đưa rác lên băng tải bằng điều khiển thủ công; chỉnh góc nghiêng, gờ bám, cửa dẫn và tốc độ.

**Đầu ra:** thuyền điều khiển thủ công hoàn chỉnh và video thử thu gom.

**Điều kiện qua mốc:** cân bằng khi mang tải thử, không có nước vào hộp điện trong bài thử; điều hướng và dừng ổn định; thu được tối thiểu 8/10 vật mẫu khi người dùng chủ động căn đúng cửa thu. Đây là chỉ tiêu dự kiến để kiểm chứng cơ khí.

### Tuần 4 — Camera và bộ dữ liệu

- [ ] Cố định camera, góc nhìn và vùng cửa thu trước khi thu dữ liệu.
- [ ] Hoàn thiện chụp/lưu ảnh để tạo dữ liệu, kiểm tra đồng thời với động cơ.
- [ ] Thu khoảng 500–1.000 ảnh ban đầu nếu khả thi; tăng hoặc sửa dữ liệu theo lỗi, không coi số lượng này là bảo đảm chất lượng.
- [ ] Có ảnh không có rác, phản chiếu, bóng cây, rác ở nhiều vị trí, độ xa và mức che khuất.
- [ ] Gán nhãn một lớp “rác cần thu”; chỉ tách thêm loại khi có nhu cầu thực tế.
- [ ] Chia dữ liệu huấn luyện/xác thực/kiểm tra theo buổi quay hoặc video, tham khảo 70/15/15; không để khung hình gần nhau lọt sang nhiều tập.
- [ ] Ghi lại điều kiện thu: góc camera, ánh sáng, mặt nước, kích thước vật.

**Đầu ra:** bộ dữ liệu có nhãn, danh sách phiên quay và tập kiểm tra được giữ riêng.

**Điều kiện qua mốc:** nhãn nhất quán, có mẫu nền khó và có dữ liệu kiểm tra từ buổi quay khác với huấn luyện.

### Tuần 5 — Huấn luyện và triển khai nhận diện

- [ ] Thử mô hình gọn, ưu tiên FOMO nếu cần vị trí tâm vật; huấn luyện trên máy tính/dịch vụ, suy luận trên ESP32.
- [ ] Xuất mô hình phù hợp với runtime đã chọn; không trộn trực tiếp mô hình của các runtime không tương thích.
- [ ] Bắt đầu với ảnh camera 320 × 240, cắt/thu nhỏ đúng đầu vào mô hình; đo thay vì mặc định cấu hình này tối ưu.
- [ ] Đo thời gian chụp, tiền xử lý, suy luận và tổng độ trễ; kiểm tra RAM/PSRAM còn trống.
- [ ] Giảm tần suất xem ảnh qua Wi-Fi khi cần; ưu tiên ảnh mới, tránh xếp hàng nhiều ảnh cũ.
- [ ] Đánh giá trên tập kiểm tra và video mới; ghi nhận nhầm phản chiếu, bỏ sót vật và mất mục tiêu.

**Đầu ra:** mô hình có phiên bản, firmware nhận diện trên board, bảng kết quả và ảnh/video lỗi.

**Điều kiện qua mốc:** nhận diện chạy ổn định cùng nguồn/động cơ; độ trễ đủ để chọn tốc độ tiếp cận phù hợp. Mục tiêu chất lượng ban đầu đề xuất: precision ≥85%, recall ≥80% trong điều kiện thử đã xác định; chốt lại sau khi có đường cơ sở và quy tắc ghép vị trí dự đoán với nhãn.

Nếu nhận diện trên ESP32 chưa đạt: giảm phạm vi rác, đầu vào hoặc tải truyền hình; bổ sung dữ liệu lỗi. Nếu vẫn không đạt sau tối ưu, đánh giá xử lý ngoài board như một thay đổi kiến trúc, đồng thời giữ bản thủ công hoạt động để bảo đảm tiến độ.

### Tuần 6 — Căn hướng và tiếp cận tự động

- [ ] Tích hợp máy trạng thái, ưu tiên dừng và quyền chuyển về thủ công.
- [ ] Chia ảnh thành trái/giữa/phải, thêm vùng chết để tránh đổi hướng liên tục.
- [ ] Chọn một mục tiêu ổn định; xác nhận qua nhiều khung hình trước khi bám.
- [ ] Giới hạn tốc độ và mức thay đổi PWM; hiệu chỉnh chênh lệch hai động cơ.
- [ ] Gắn thời điểm cho kết quả nhận diện; giảm tốc hoặc dừng khi dữ liệu quá cũ.
- [ ] Mất mục tiêu thì giảm tốc, dừng và chỉ tìm kiếm lại theo quy tắc có thời hạn.
- [ ] Đặt giới hạn thời gian tiếp cận để tránh chạy mãi tới mục tiêu sai.

**Đầu ra:** thuyền tự căn hướng và tiếp cận chậm; nhật ký chuyển trạng thái.

**Điều kiện qua mốc:** căn đúng mục tiêu từ các vị trí trái/giữa/phải; dừng khi mất ảnh, mất mục tiêu quá hạn hoặc có lệnh dừng; người dùng luôn chuyển về thủ công được.

### Tuần 7 — Tự thu gom và xử lý lỗi

- [ ] Tích hợp cảm biến cửa thu nếu đã thử đạt; kiểm tra rác trong suốt và nước bắn.
- [ ] Bật băng tải khi vào vùng thu đã hiệu chuẩn; tiếp tục chạy có thời hạn khi vật khuất camera.
- [ ] Nếu thiếu tín hiệu xác nhận thu, ghi kết quả “chưa xác nhận”, không tự tăng bộ đếm rác đã thu.
- [ ] Tích hợp phát hiện kẹt bằng dòng và thời gian nếu có phần cứng đo phù hợp; có khoảng bỏ qua dòng khởi động.
- [ ] Khi kẹt: dừng băng tải và báo lỗi. Chỉ bổ sung đảo chiều ngắn sau khi đã kiểm chứng cơ khí cho phép, giới hạn số lần thử.
- [ ] Thử pin thấp, mất Wi-Fi, reset, lỗi camera, ảnh cũ, mất mục tiêu và lỗi cảm biến.
- [ ] Tích hợp mạch sạc mặt trời đã xác minh; kiểm tra độc lập rồi mới chạy cùng hệ thống.

**Đầu ra:** chu trình tự động hoàn chỉnh và bảng thử lỗi.

**Điều kiện qua mốc:** không tiếp tục chạy vô hạn khi thu thất bại; trạng thái lỗi đưa tải về hành vi đã xác định; pin thấp dừng/báo và yêu cầu thu hồi thủ công. Không hứa tự quay về khi chưa có định vị và điều hướng tương ứng.

### Tuần 8 — Nghiệm thu và hồ sơ trình diễn

- [ ] Chạy toàn bộ bài thử ở mục 5; lưu cả lượt thành công và thất bại.
- [ ] Thử ít nhất 20 lượt tự thu gom, phân bố vị trí và ánh sáng đã định trước.
- [ ] Đo thời gian chạy với tải đại diện và mức thu gom thực tế.
- [ ] Đo riêng đóng góp năng lượng mặt trời; chưa tuyên bố tự cấp năng lượng nếu chưa có cân bằng năng lượng đo được.
- [ ] Chốt phiên bản phần cứng, phần mềm và mô hình dùng trình diễn.
- [ ] Hoàn thiện sơ đồ, hướng dẫn lắp/chạy, dữ liệu thử và video minh chứng.
- [ ] Chuẩn bị kịch bản demo thủ công dự phòng và phương án thu hồi thuyền.

**Đầu ra:** nguyên mẫu, mã nguồn, mô hình, sơ đồ cuối, hướng dẫn sử dụng và báo cáo kết quả.

**Điều kiện hoàn thành:** có số liệu đối chiếu mục tiêu; nêu rõ điều kiện vận hành, lỗi còn tồn tại và giới hạn hệ thống.

## 5. Bộ tiêu chí nghiệm thu dự kiến

Các ngưỡng dưới đây là mục tiêu kỹ thuật đề xuất, chưa phải hiệu năng đã được chứng minh. Chốt sau tuần 1–3 và không tự giảm ngưỡng để báo đạt.

| Hạng mục | Phép thử | Mục tiêu ban đầu |
|---|---|---|
| Khởi động | Bật nguồn/reset nhiều lần | Động cơ giữ dừng đến khi được cho phép |
| Dừng thủ công | Phát lệnh dừng trong lúc chạy | Đầu ra điều khiển về trạng thái dừng ≤1 giây; đo riêng thời gian thuyền trôi dừng |
| Mất kết nối | Ngắt đường điều khiển/giám sát khi chạy | Dừng theo timeout đã chốt; mặc định bản đầu áp dụng cho cả hai chế độ |
| Nguồn | Camera + Wi-Fi + tải động cơ đại diện | Không reset, không vượt giới hạn nguồn/driver trong ≥15 phút |
| Thu gom thủ công | 10 vật mẫu, người dùng căn đúng cửa | Thành công ≥8/10 |
| Nhận diện | Tập kiểm tra từ phiên quay riêng | Precision ≥85%, recall ≥80% trong điều kiện công bố |
| Tự thu gom | 20 lượt, vị trí/điều kiện ghi trước | Thành công ≥16/20; không loại lượt thất bại khỏi thống kê |
| Pin thấp | Mô phỏng tín hiệu bằng phương pháp phù hợp | Cảnh báo và dừng đúng ngưỡng, không cần xả sâu bộ pin thật |
| Kẹt băng tải | Tạo tải cản có kiểm soát | Dừng/báo trước khi vượt giới hạn đã xác định của động cơ và driver |
| Độ bền phiên thử | Chạy tích hợp | Mục tiêu ≥30 phút không reset; nếu pin không đáp ứng, ghi số đo và điều chỉnh cấu hình |

Một lượt tự thu thành công được tính khi thuyền phát hiện/tiếp cận và đưa vật vào giỏ, không có can thiệp lái từ người dùng. Can thiệp để tránh sự cố được ghi là lượt không hoàn thành, kèm nguyên nhân.

Ghi tối thiểu: mã lượt, phiên bản firmware/mô hình, loại và kích thước rác, vị trí xuất phát, ánh sáng/mặt nước, điện áp pin, thời gian chu trình, kết quả và lý do lỗi.

## 6. Tổ chức phần mềm dự kiến

Chốt Arduino hoặc ESP-IDF sau khi chạy được camera và xác minh runtime mô hình. Không đổi framework giữa chừng nếu chưa có trở ngại cụ thể.

```text
src/
  main                 Khởi tạo và điều phối
  board_config         GPIO, cực tính, tham số phần cứng
  camera_service       Thu ảnh và quản lý bộ đệm
  vision_service       Tiền xử lý, suy luận, kết quả có dấu thời gian
  motor_control        Chiều, PWM, tăng giảm tốc, dừng
  conveyor_control     Băng tải, thời hạn chạy, xử lý kẹt
  state_machine        Chế độ và chuyển trạng thái
  power_monitor        ADC, hiệu chuẩn, pin thấp
  web_control          Giao diện và thời hạn hiệu lực lệnh
  event_log            Trạng thái, lỗi, thời gian xử lý
```

Đây là cấu trúc đề xuất, chưa phải các tệp đã được triển khai. Chu kỳ điều khiển không được bị chặn bởi chụp ảnh, suy luận hay truyền hình. Khi dùng PWM, phân bổ timer/channel tránh ảnh hưởng xung XCLK của camera.

## 7. Vật tư và ngân sách

Chưa có đủ model và thông số để báo giá đáng tin cậy. Dùng bảng sau làm danh sách xác minh, không mua driver/pin chỉ dựa vào tên “12 V”.

| Nhóm | Cần xác minh hoặc bổ sung | Ưu tiên |
|---|---|---|
| Điều khiển | Board Goouuu, OV2640, cáp camera, cáp nạp | Đang có: kiểm tra |
| Công suất | Driver cho hai động cơ đẩy và băng tải, phù hợp dòng tải | Bắt buộc trước khi chạy |
| Nguồn | Pin/BMS, bộ sạc phù hợp, hạ áp, cầu chì, công tắc, đầu nối, dây | Bắt buộc |
| Cơ khí | Thân/phao, bộ đẩy, băng tải, ổ đỡ, giỏ, giá camera | Bắt buộc |
| Bảo vệ môi trường | Hộp điện, đầu xuyên dây, cố định chống rung/nước bắn | Bắt buộc |
| Cảm biến | Cửa thu và đo dòng băng tải | Ưu tiên cho tự động |
| Năng lượng mặt trời | Tấm pin và bộ sạc đã đối chiếu với pin | Tích hợp sau nguồn cơ bản |
| Đo kiểm | Đồng hồ, phương tiện đo dòng/nguồn giới hạn dòng, đo nhiệt nếu có | Chuẩn bị hoặc mượn |

Ghi chi phí thực trả, linh kiện đã có, thời gian giao và một lựa chọn thay thế cho mỗi linh kiện quan trọng. Chừa khoảng 15–20% ngân sách mua bổ sung cho sửa cơ khí và vật tư lắp ráp.

## 8. Rủi ro và cách xử lý

| Rủi ro | Dấu hiệu | Hướng xử lý |
|---|---|---|
| Trùng chân camera/USB/bộ nhớ | Camera không chạy, lỗi nạp hoặc reset | Xác minh pinout trước khi đấu công suất; giảm LED để dành chân |
| Driver quá tải/sụt áp | Motor yếu, driver nóng, pin sụt | Đo tải, chọn lại driver và dây; không bù bằng tăng PWM vô điều kiện |
| Nhiễu động cơ | ESP reset, ảnh lỗi khi khởi động tải | Sửa đường nguồn/GND, lọc và khử nhiễu theo phép đo |
| Băng tải không cuốn được rác | Nhận diện đúng nhưng thu thất bại | Sửa cửa dẫn, góc, gờ và tốc độ trước khi chỉnh AI |
| Nhận nhầm phản chiếu | Bám vào vùng nước trống | Bổ sung mẫu nền khó, điều chỉnh camera và xác nhận nhiều khung hình |
| Suy luận chậm | Thuyền vượt mục tiêu, lệnh theo ảnh cũ | Giảm tốc, giảm tải truyền hình, mô hình gọn hơn và bỏ ảnh cũ |
| Rác khuất trước cửa thu | Mất mục tiêu đúng lúc tiếp cận | Vùng thu đã hiệu chuẩn, cảm biến xác nhận, chạy băng tải có thời hạn |
| Camera/khối lượng đổi sau huấn luyện | Hiệu năng ngoài nước giảm | Cố định bố trí, thu lại dữ liệu đại diện |
| Chậm linh kiện hoặc AI chưa đạt | Trễ mốc tích hợp | Giữ bản thủ công hoạt động; thu hẹp phạm vi công khai, dành 1–2 tuần dự phòng |

## 9. Việc cần làm trong ba buổi đầu

**Buổi 1:** kiểm kê; ghi đúng board, động cơ, pin và mạch sạc; chụp hai mặt board; chốt vật mẫu và vùng thử.

**Buổi 2:** chạy camera độc lập; kiểm tra PSRAM; lập bảng GPIO; tra thông số tải và nguồn.

**Buổi 3:** sửa sơ đồ đấu dây; chốt driver, nhánh nguồn, cầu chì và danh sách vật tư; bắt đầu chương trình thử từng động cơ sau khi đường nguồn đã được kiểm tra.

## 10. Tài liệu kỹ thuật tham chiếu

- Sơ đồ đầu vào: `doc/sodomach.jpg`; cần sửa sau khi xác minh thiết bị thực tế.
- [Espressif: esp32-camera](https://github.com/espressif/esp32-camera) — yêu cầu PSRAM, định dạng ảnh và tải khi dùng Wi-Fi.
- [Espressif: ESP32-S3 GPIO](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-reference/peripherals/gpio.html) — chân bộ nhớ và USB; không thay thế pinout riêng của board Goouuu.
- [ST: datasheet L298](https://www.st.com/resource/en/datasheet/l298.pdf) — giới hạn điện và sụt áp; module thực tế cần được kiểm tra riêng.
- [Edge Impulse: FOMO](https://docs.edgeimpulse.com/studio/projects/learning-blocks/blocks/object-detection/fomo) — phát hiện theo tâm vật và các giới hạn liên quan.
