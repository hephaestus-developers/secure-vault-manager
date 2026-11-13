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
    build_passkey_item, bump_passkey_sign_count, decode_base64url, encode_base64url,
    execute_passkey_assertion, extract_normalized_host, extract_passkey_credential_id,
    is_matching_login_domain, matches_passkey_rp_id, passkey_matches_credential_list,
    query_passkeys_payload, PasskeyItemParams, WebAuthnAuthenticator,
};
pub use fido2::{
    authenticate_passkey_assertion, create_passkey_credential, PasskeyAssertion,
    PasskeyRegistration, SVM_AAGUID,
};
