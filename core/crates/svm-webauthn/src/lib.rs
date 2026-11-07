//! # svm-webauthn
//!
//! Mô-đun bộ xác thực Passkey / FIDO2 Authenticator hoàn chỉnh.
//!
//! Hỗ trợ các thao tác `MakeCredential`, `GetAssertion`, kiểm tra Relying Party
//! và truy vấn danh sách Passkey theo tên miền.

pub mod authenticator;

pub use authenticator::WebAuthnAuthenticator;
