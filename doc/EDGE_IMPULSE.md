# Tích hợp mô hình phát hiện rác

Mã adapter đã có ở `src/vision_service.cpp`. Dự án chưa kèm mô hình được huấn luyện; `boat-ai` cố ý báo lỗi build nếu chưa có `include/ei_model.h`.

1. Cố định camera, dùng `boat-live` để thu ảnh thực tế. Chọn ảnh có rác và nền khó (mặt nước không có rác, bóng cây, phản chiếu).
2. Trong Edge Impulse, tạo dự án ảnh **object detection**, gán nhãn mục tiêu `rac` hoặc sửa `config::targetLabel` để khớp nhãn đã chọn. Không dùng mô hình phân loại cả ảnh vì điều hướng cần vị trí vật.
3. Thử FOMO, đầu vào nhỏ như 96×96 hoặc 160×160, lượng tử hóa INT8 nếu phù hợp. Đánh giá trên tập kiểm tra từ buổi chụp khác. Mô hình phải phù hợp bộ nhớ và các toán tử runtime của ESP32.
4. Tiền xử lý firmware lấy JPEG 320×240, giải mã BGR888 theo camera driver đi cùng core Arduino 2.0.17, cắt giữa theo tỉ lệ đầu vào rồi lấy mẫu về kích thước mô hình. Thiết lập crop tương ứng khi huấn luyện; kiểm tra lại bằng cùng ảnh trên Studio và board vì thuật toán resize có thể khác. Màu được đổi sang giá trị RGB 24-bit cho `signal.get_data`.
5. Export **Arduino library**, giải nén sao cho thư viện nằm trực tiếp trong `lib/TrashModel/`, có `library.properties` và thư mục `src/`. Không để thừa nhiều lớp thư mục ZIP.
6. Sao chép `include/ei_model.example.h` thành `include/ei_model.h`, thay include bằng tên header thực do Edge Impulse sinh, ví dụ `#include <thu_gom_rac_inferencing.h>`.
7. Build `pio run -e boat-ai`. Runtime của mô hình do thư viện export cung cấp; chưa thể xác nhận tương thích hoặc bộ nhớ trước khi có thư viện thật. Nếu lỗi SDK, dùng phiên bản export hỗ trợ core này hoặc đánh giá nâng toolchain một cách riêng biệt.
8. Thử trên bàn với nguồn động lực ngắt; kiểm tra mục tiêu, nhãn, tọa độ và thời gian suy luận. Nếu cần thử AI với đầu ra mô phỏng, tạo môi trường kế thừa `boat-ai` nhưng đổi cờ `BOAT_OUTPUTS_ENABLED=0` (không để hai định nghĩa trái nhau).

Adapter chọn kết quả có nhãn đúng và điểm tin cậy cao nhất; máy trạng thái yêu cầu ba khung hình liên tiếp có vị trí gần nhau. Đây là cách bám mục tiêu đơn giản, chưa phải bộ theo dõi định danh nhiều vật.

Tọa độ từ mô hình được ánh xạ ngược về ảnh gốc trước điều khiển. Với ảnh vuông cắt giữa, hai mép trái/phải ngoài vùng crop sẽ không được mô hình quan sát. FOMO cho vị trí tâm, không cung cấp kích thước thật hoặc khoảng cách. Vùng thu `collectY` cần hiệu chuẩn theo góc camera; nếu vùng crop không bao trùm cửa thu thì phải đổi cách bố trí/crop hoặc dùng cảm biến cửa thu đã thử đạt.

Firmware dừng nếu dữ liệu suy luận cũ hơn `visionTimeoutMs` (mặc định 1.500 ms). Chọn tốc độ thuyền sau khi đo tổng trễ trên board; không chỉ tăng timeout để che giấu suy luận quá chậm.

Tham chiếu: [FOMO](https://docs.edgeimpulse.com/studio/projects/learning-blocks/blocks/object-detection/fomo), [ví dụ suy luận ESP32 của Edge Impulse](https://github.com/edgeimpulse/example-standalone-inferencing-espressif-esp32), [driver camera Espressif](https://github.com/espressif/esp32-camera).
