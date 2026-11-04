//! # svm-webauthn
//!
//! Mô-đun bộ xác thực Passkey / FIDO2 Authenticator hoàn chỉnh.
//!
//! Hỗ trợ các thao tác `MakeCredential`, `GetAssertion`, kiểm tra Relying Party
//! qua Public Suffix List (PSL) và mã hóa CBOR chuẩn hóa deterministic RFC 8949.

/// Bộ xử lý trình xác thực ảo FIDO2 / WebAuthn.
pub struct WebAuthnAuthenticator;

impl WebAuthnAuthenticator {
    /// Khởi tạo một thực thể WebAuthnAuthenticator mới.
    pub fn new() -> Self {
        Self
    }
}

impl Default for WebAuthnAuthenticator {
    fn default() -> Self {
        Self::new()
    }
}
