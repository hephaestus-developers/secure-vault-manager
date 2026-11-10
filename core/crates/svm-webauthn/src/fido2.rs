//! Triển khai quy trình đăng ký (MakeCredential) và xác thực (GetAssertion) FIDO2 / WebAuthn Level 3.

use ring::rand::{SecureRandom, SystemRandom};
use ring::signature::{EcdsaKeyPair, KeyPair, ECDSA_P256_SHA256_ASN1_SIGNING};
use svm_crypto::sha256;

use crate::cbor::{encode_attestation_object_none, encode_cose_key_p256};

/// AAGUID cố định của Secure Vault Manager: "0073766d-7661-756c-742d-6d616e616765"
pub const SVM_AAGUID: [u8; 16] = [
    0x00, 0x73, 0x76, 0x6d, 0x76, 0x61, 0x75, 0x6c, 0x74, 0x2d, 0x6d, 0x61, 0x6e, 0x61, 0x67, 0x65,
];

/// Kết quả tạo Passkey mới (`passkey_create`).
#[derive(Debug, Clone)]
pub struct PasskeyRegistration {
    pub credential_id: Vec<u8>,
    pub private_key_pkcs8: Vec<u8>,
    pub public_key_spki: Vec<u8>,
    pub attestation_object: Vec<u8>,
    pub client_data_json: String,
    pub algorithm: i32,
}

/// Kết quả ký số xác thực đăng nhập (`passkey_get`).
#[derive(Debug, Clone)]
pub struct PasskeyAssertion {
    pub credential_id: Vec<u8>,
    pub authenticator_data: Vec<u8>,
    pub signature: Vec<u8>,
    pub client_data_json: String,
    pub user_handle: Option<Vec<u8>>,
}

/// Sinh khóa mới và tạo Attestation Object (FIDO2 MakeCredential / WebAuthn Create).
pub fn create_passkey_credential(
    rp_id: &str,
    challenge: &str,
    origin: &str,
) -> Result<PasskeyRegistration, String> {
    let rng = SystemRandom::new();

    // 1. Sinh cặp khóa ECDSA P-256 (ES256 / COSE -7) chuẩn PKCS#8
    let pkcs8_doc = EcdsaKeyPair::generate_pkcs8(&ECDSA_P256_SHA256_ASN1_SIGNING, &rng)
        .map_err(|e| format!("Failed to generate P-256 keypair: {e}"))?;
    let pkcs8_bytes = pkcs8_doc.as_ref().to_vec();

    let key_pair = EcdsaKeyPair::from_pkcs8(&ECDSA_P256_SHA256_ASN1_SIGNING, &pkcs8_bytes, &rng)
        .map_err(|e| format!("Failed to parse P-256 keypair: {e}"))?;

    // Public key dạng không nén: 0x04 (1 byte) || X (32 bytes) || Y (32 bytes) = 65 bytes
    let pub_key_bytes = key_pair.public_key().as_ref();
    if pub_key_bytes.len() != 65 || pub_key_bytes[0] != 0x04 {
        return Err("Unexpected uncompressed public key format".to_string());
    }

    let x_coord = &pub_key_bytes[1..33];
    let y_coord = &pub_key_bytes[33..65];

    // 2. Đóng gói COSE Key theo RFC 8152
    let cose_key = encode_cose_key_p256(x_coord, y_coord);

    // 3. Sinh Credential ID ngẫu nhiên (32 bytes)
    let mut credential_id = vec![0u8; 32];
    rng.fill(&mut credential_id)
        .map_err(|e| format!("RNG failure: {e}"))?;

    // 4. Xây dựng Authenticator Data:
    // - rpIdHash (32 bytes)
    // - flags (1 byte): UP (0x01) | UV (0x04) | AT (0x40) = 0x45
    // - signCount (4 bytes big-endian): 0
    // - attestedCredentialData: AAGUID (16 bytes) || credIdLen (2 bytes) || credentialId (32 bytes) || cose_key
    let mut auth_data = Vec::with_capacity(37 + 16 + 2 + credential_id.len() + cose_key.len());

    let rp_id_hash = sha256(rp_id.as_bytes());
    auth_data.extend_from_slice(&rp_id_hash);
    auth_data.push(0x45); // Flags: UP=1, UV=1, AT=1
    auth_data.extend_from_slice(&0u32.to_be_bytes()); // signCount = 0

    // Attested Credential Data
    auth_data.extend_from_slice(&SVM_AAGUID);
    auth_data.extend_from_slice(&(credential_id.len() as u16).to_be_bytes());
    auth_data.extend_from_slice(&credential_id);
    auth_data.extend_from_slice(&cose_key);

    // 5. Đóng gói Attestation Object (fmt: "none")
    let attestation_object = encode_attestation_object_none(&auth_data);

    // 6. Xây dựng Client Data JSON
    let client_data_json = format!(
        r#"{{"type":"webauthn.create","challenge":"{challenge}","origin":"{origin}","crossOrigin":false}}"#
    );

    Ok(PasskeyRegistration {
        credential_id,
        private_key_pkcs8: pkcs8_bytes,
        public_key_spki: pub_key_bytes.to_vec(),
        attestation_object,
        client_data_json,
        algorithm: -7, // ES256
    })
}

/// Thực thi ký số Assertion cho yêu cầu đăng nhập (`passkey_get`).
pub fn authenticate_passkey_assertion(
    rp_id: &str,
    challenge: &str,
    origin: &str,
    credential_id: &[u8],
    private_key_pkcs8: &[u8],
    sign_count: u32,
    user_handle: Option<Vec<u8>>,
) -> Result<PasskeyAssertion, String> {
    let rng = SystemRandom::new();

    let key_pair =
        EcdsaKeyPair::from_pkcs8(&ECDSA_P256_SHA256_ASN1_SIGNING, private_key_pkcs8, &rng)
            .map_err(|e| format!("Invalid P-256 PKCS#8 key: {e}"))?;

    // 1. Authenticator Data cho Assertion:
    // - rpIdHash (32 bytes)
    // - flags (1 byte): UP (0x01) | UV (0x04) = 0x05
    // - signCount (4 bytes big-endian)
    let rp_id_hash = sha256(rp_id.as_bytes());
    let mut auth_data = Vec::with_capacity(37);
    auth_data.extend_from_slice(&rp_id_hash);
    auth_data.push(0x05); // Flags: UP=1, UV=1
    auth_data.extend_from_slice(&sign_count.to_be_bytes());

    // 2. Client Data JSON
    let client_data_json = format!(
        r#"{{"type":"webauthn.get","challenge":"{challenge}","origin":"{origin}","crossOrigin":false}}"#
    );
    let client_data_hash = sha256(client_data_json.as_bytes());

    // 3. Ký số trên (auth_data || client_data_hash)
    let mut msg_to_sign = Vec::with_capacity(auth_data.len() + client_data_hash.len());
    msg_to_sign.extend_from_slice(&auth_data);
    msg_to_sign.extend_from_slice(&client_data_hash);

    let sig = key_pair
        .sign(&rng, &msg_to_sign)
        .map_err(|e| format!("ECDSA signing failed: {e}"))?;

    Ok(PasskeyAssertion {
        credential_id: credential_id.to_vec(),
        authenticator_data: auth_data,
        signature: sig.as_ref().to_vec(),
        client_data_json,
        user_handle,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_passkey_create_and_assertion_roundtrip() {
        let reg =
            create_passkey_credential("github.com", "random_challenge_123", "https://github.com")
                .expect("create passkey");
        assert_eq!(reg.credential_id.len(), 32);
        assert_eq!(reg.algorithm, -7);
        assert!(!reg.attestation_object.is_empty());
        assert!(reg.client_data_json.contains("webauthn.create"));

        let assertion = authenticate_passkey_assertion(
            "github.com",
            "random_challenge_456",
            "https://github.com",
            &reg.credential_id,
            &reg.private_key_pkcs8,
            1,
            Some(b"user_123".to_vec()),
        )
        .expect("authenticate passkey");

        assert_eq!(assertion.credential_id, reg.credential_id);
        assert!(!assertion.signature.is_empty());
        assert_eq!(assertion.authenticator_data.len(), 37);
        assert_eq!(assertion.authenticator_data[32], 0x05); // UP=1, UV=1
        assert_eq!(assertion.user_handle, Some(b"user_123".to_vec()));
    }
}
