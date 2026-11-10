//! # svm-webauthn
//!
//! Mô-đun bộ xác thực Passkey / FIDO2 Authenticator hoàn chỉnh.
//!
//! Hỗ trợ các thao tác `MakeCredential`, `GetAssertion`, kiểm tra Relying Party
//! và truy vấn danh sách Passkey theo tên miền.

pub mod authenticator;
pub mod cbor;
pub mod fido2;

pub use authenticator::{
    bump_passkey_sign_count, extract_passkey_credential_id, matches_passkey_rp_id,
    passkey_matches_credential_list, WebAuthnAuthenticator,
};
pub use fido2::{
    authenticate_passkey_assertion, create_passkey_credential, PasskeyAssertion,
    PasskeyRegistration, SVM_AAGUID,
};
