# Kiểm tra phần mềm — 11/11/2026

| Kiểm tra | Kết quả |
|---|---|
| Build `boat-live` | Thành công với điều khiển LED và băng tải bật/tắt |
| Build `boat-bench` | Thành công với motor mô phỏng, LED hoạt động thật |
| `test-windows / test_core` | 26/26 kiểm thử đạt |
| `node --test test/web/control.test.cjs` | 7/7 kiểm thử giao diện đạt |
| Cú pháp `scripts/capture_dataset.py` bằng Python | Đạt |

Kiểm thử bao gồm: khởi động ở trạng thái dừng, thời hạn lệnh, gói tin tới muộn/sai thứ tự, lệnh dừng vô hiệu hóa phiên cũ, pin thấp và lỗi được giữ đến khi xóa, giới hạn băng tải, ảnh cũ, xác nhận qua khung hình khác nhau, mất mục tiêu, timeout tìm/tiếp cận/thu, bộ đếm thời gian tràn và thời gian chờ đảo chiều motor. Phần LED kiểm tra chống ghi đè bằng revision cũ và từ chối màu không hợp lệ.

Kiểm thử giao diện chạy handler JavaScript thực với DOM/API mô phỏng: đúng bốn hướng, thả nút dừng lái, bật băng tải giữ trạng thái đến khi tắt, dừng toàn bộ, LED độc lập lúc STOP, phản hồi cũ không khôi phục trạng thái và mất mạng/rời trang hủy quyền điều khiển. Không yêu cầu `AbortSignal.timeout`, nhằm hỗ trợ các trình duyệt điện thoại cũ hơn có `AbortController`.

Môi trường: PlatformIO 6.1.18, espressif32 6.12.0, Arduino ESP32 2.0.17, Windows. Build không đo được heap/stack/bộ đệm động khi camera và Wi-Fi chạy trên board.

Artifact được lưu dưới `.pio/verified-build/<môi-trường>/firmware.bin`. Thư mục build mặc định đã gặp lỗi tệp bị mất trong lúc biên dịch; đầu ra của lần build live thành công ở đó cũng bị xóa sau đó. Vì vậy các bản firmware được build lại trong thư mục riêng. `scripts/pio.cmd` hiện dùng thư mục riêng để tránh chia sẻ đầu ra với tác vụ IDE.

Chưa nạp/đo trên board, chưa thử chiều quay, nguồn động lực, điện áp thực, camera trên phần cứng hoặc giao diện qua mạng của board. Chưa có thư viện mô hình thật nên **chưa build/kiểm chứng `boat-ai`** và chưa đo độ chính xác phát hiện rác. Adapter AI cần được xác minh lại với bản export Edge Impulse cụ thể.
