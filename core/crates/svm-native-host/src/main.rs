//! # svm-native-host
//!
//! Tiến trình Native Messaging Host Relay nhị phân độc lập.
//!
//! Đóng vai trò là cầu nối chuyển tiếp tin nhắn giữa WebExtension (giao tiếp qua Stdio
//! với framing 4-byte Little-Endian) và ứng dụng chính `secure-vault-manager` (qua Unix Domain Socket
//! hoặc Windows Named Pipe).
//!
//! Tiến trình này được thiết kế siêu nhẹ (< 2MB) để khởi động tức thì khi trình duyệt yêu cầu.

fn main() {
    println!("SVM Native Host v0.1.0 - Hephaestus Developers");
}
