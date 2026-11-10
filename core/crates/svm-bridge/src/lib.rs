//! # svm-bridge
//!
//! Mô-đun cầu nối trình duyệt: Lắng nghe kết nối IPC từ Extension, quản lý danh sách
//! các trình duyệt đã kết nối và cài đặt tệp cấu hình Native Messaging Host manifest.

pub mod client;
pub mod server;

pub use client::PairedClient;
pub use server::BridgeServer;
