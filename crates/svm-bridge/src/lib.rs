//! # svm-bridge
//!
//! Mô-đun cầu nối trình duyệt: Lắng nghe kết nối IPC từ Extension, quản lý danh sách
//! các trình duyệt đã kết nối và cài đặt tệp cấu hình Native Messaging Host manifest.

pub mod client;
pub mod external_installer;
pub mod server;
pub mod totp_actions;

pub use client::PairedClient;
pub use external_installer::ExternalExtensionInstaller;
pub use server::BridgeServer;
pub use totp_actions::{handle_decode_qr_action, handle_update_totp_action};
