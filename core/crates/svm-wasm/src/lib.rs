//! # svm-wasm
//!
//! Gói WebAssembly biên dịch từ Rust cho Tiện ích mở rộng Trình duyệt (Browser Extension).
//! Cung cấp khả năng giải mã cục bộ bản chụp kho ngoại tuyến (Offline Vault Snapshot),
//! sinh mã TOTP 2FA (RFC 6238), sinh mật khẩu ngẫu nhiên mật mã học và đánh giá độ mạnh mật khẩu
//! trực tiếp trong WebExtension Service Worker / Popup UI.

pub mod dispatcher;
pub mod envelope;
pub mod ffi;

pub use dispatcher::{
    wasm_dispatch_json, wasm_evaluate_password_strength, wasm_generate_password,
    wasm_generate_totp, wasm_hmac_sha256_hex, wasm_init, wasm_parse_otpauth, wasm_sha256_hex,
};
pub use envelope::{wasm_decrypt_offline_envelope, wasm_encrypt_offline_envelope};
pub use ffi::{svm_wasm_alloc, svm_wasm_dealloc, svm_wasm_invoke, svm_wasm_seed};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_wasm_offline_envelope_and_dispatch() {
        wasm_init(0x1234_5678_9ABC_DEF0);
        let secret_payload = r#"{"items":[{"id":"1","title":"GitHub"}]}"#;
        let envelope =
            wasm_encrypt_offline_envelope("MasterPass!2026", secret_payload).expect("encrypt");
        let decrypted =
            wasm_decrypt_offline_envelope("MasterPass!2026", &envelope).expect("decrypt");
        assert_eq!(decrypted, secret_payload);

        // Wrong password must fail
        assert!(wasm_decrypt_offline_envelope("WrongPass", &envelope).is_err());

        // Test JSON dispatcher for TOTP and Password Generation
        let totp_resp = wasm_dispatch_json(
            r#"{"action":"totp","secret":"JBSWY3DPEHPK3PXP","timestamp":1700000000}"#,
        );
        assert!(totp_resp.contains(r#""ok":true"#));
        assert!(totp_resp.contains(r#""code":"#));

        let gen_resp = wasm_dispatch_json(r#"{"action":"generate_password","length":24}"#);
        assert!(gen_resp.contains(r#""ok":true"#));
        assert!(gen_resp.contains(r#""password":"#));
    }
}
