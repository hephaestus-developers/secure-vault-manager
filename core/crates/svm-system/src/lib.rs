//! # svm-system
//!
//! Mô-đun tích hợp các tính năng hệ điều hành: Tăng cường bảo mật tiến trình
//! (`PR_SET_DUMPABLE=0`), chặn hệ thống chuyển sang chế độ ngủ (sleep blocker),
//! tích hợp cảm biến sinh trắc học (Biometrics) và OS Keystore.

pub mod hardening;
pub mod keystore;

pub use hardening::{authenticate_os_user, harden_current_process, is_biometric_available};
pub use keystore::{
    is_os_keystore_available, keystore_delete_secret, keystore_get_secret, keystore_set_secret,
};
