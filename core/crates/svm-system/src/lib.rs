//! # svm-system
//!
//! Mô-đun tích hợp các tính năng hệ điều hành: Tăng cường bảo mật tiến trình
//! (`PR_SET_DUMPABLE=0`), chặn hệ thống chuyển sang chế độ ngủ (sleep blocker),
//! tích hợp cảm biến sinh trắc học (Biometrics) và OS Keystore.

pub mod hardening;

pub use hardening::{harden_current_process, is_biometric_available};
