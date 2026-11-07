//! # svm-core
//!
//! Thư viện chia sẻ động C-ABI (`libsvm_core.so` / `.dylib` / `.dll`) cho **Secure Vault Manager (SVM)**.
//!
//! Crate này đóng vai trò là ranh giới FFI (Foreign Function Interface) chính giữa hệ thống lõi Rust
//! và giao diện người dùng C++17 Qt6 Desktop.
//!
//! # Nguyên tắc thiết kế FFI:
//! 1. **Ownership Contract**: Mọi chuỗi/bộ đệm do Rust cấp phát bắt buộc phải được giải phóng bằng
//!    `svm_free_string`, `svm_free_secret` hoặc `svm_free_buffer`.
//! 2. **Panic Boundary Isolation**: Mọi hàm extern "C" đều được bảo vệ để không làm Panic rò rỉ sang C++.
//! 3. **Thread Safety**: `SvmContext` an toàn để chia sẻ giữa các luồng (`Send + Sync`).

pub mod context;
pub mod item_ffi;
pub mod lifecycle_ffi;
pub mod service_ffi;
pub mod vault_ffi;

pub use context::{SvmBuffer, SvmContext, SvmEventCallback};
pub use item_ffi::*;
pub use lifecycle_ffi::*;
pub use service_ffi::*;
pub use svm_domain::status::SvmStatusCode;
pub use vault_ffi::*;
