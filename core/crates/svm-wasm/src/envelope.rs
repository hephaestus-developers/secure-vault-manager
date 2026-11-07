//! Mã hóa và giải mã cục bộ gói JSON bản chụp ngoại tuyến (Offline Cache Envelope) cho WebAssembly.

use svm_crypto::{
    chacha20_poly1305_decrypt, chacha20_poly1305_encrypt, derive_kek, hex_decode, hex_encode,
    random_bytes,
};
use svm_domain::{DomainError, JsonValue};

/// Mã hóa cục bộ một gói JSON (Offline Cache Envelope) bằng khóa dẫn xuất từ mật khẩu chính.
pub fn wasm_encrypt_offline_envelope(
    master_password: &str,
    plaintext_json: &str,
) -> Result<String, DomainError> {
    let mut salt = [0u8; 16];
    random_bytes(&mut salt);
    let mut nonce = [0u8; 12];
    random_bytes(&mut nonce);

    let key = derive_kek(master_password, &salt);
    let ct_and_tag = chacha20_poly1305_encrypt(
        &key,
        &nonce,
        b"svm-wasm-offline-v1",
        plaintext_json.as_bytes(),
    );
    let ct_len = ct_and_tag.len().saturating_sub(16);
    let ciphertext = &ct_and_tag[..ct_len];
    let tag = &ct_and_tag[ct_len..];

    let mut obj = JsonValue::new_object();
    obj.insert("version", JsonValue::Number(1.0));
    obj.insert("salt", JsonValue::String(hex_encode(&salt)));
    obj.insert("nonce", JsonValue::String(hex_encode(&nonce)));
    obj.insert("tag", JsonValue::String(hex_encode(tag)));
    obj.insert("ciphertext", JsonValue::String(hex_encode(ciphertext)));
    Ok(obj.to_json_string())
}

/// Giải mã cục bộ một gói JSON (Offline Cache Envelope) bằng mật khẩu chính.
pub fn wasm_decrypt_offline_envelope(
    master_password: &str,
    envelope_json: &str,
) -> Result<String, DomainError> {
    let parsed = JsonValue::parse(envelope_json)
        .map_err(|e| DomainError::Parse(format!("Invalid offline envelope JSON: {e}")))?;

    let salt_hex = parsed.get_str_or("salt", "");
    let nonce_hex = parsed.get_str_or("nonce", "");
    let tag_hex = parsed.get_str_or("tag", "");
    let ct_hex = parsed.get_str_or("ciphertext", "");

    let salt_vec = hex_decode(salt_hex)?;
    let nonce_vec = hex_decode(nonce_hex)?;
    let tag_vec = hex_decode(tag_hex)?;
    let ciphertext = hex_decode(ct_hex)?;

    if salt_vec.len() != 16 || nonce_vec.len() != 12 || tag_vec.len() != 16 {
        return Err(DomainError::InvalidArg(
            "Invalid salt/nonce/tag length in envelope".to_string(),
        ));
    }

    let mut salt = [0u8; 16];
    salt.copy_from_slice(&salt_vec);
    let mut nonce = [0u8; 12];
    nonce.copy_from_slice(&nonce_vec);

    let mut ct_and_tag = Vec::with_capacity(ciphertext.len() + 16);
    ct_and_tag.extend_from_slice(&ciphertext);
    ct_and_tag.extend_from_slice(&tag_vec);

    let key = derive_kek(master_password, &salt);
    let plaintext = chacha20_poly1305_decrypt(&key, &nonce, b"svm-wasm-offline-v1", &ct_and_tag)?;
    String::from_utf8(plaintext)
        .map_err(|_| DomainError::Crypto("Decrypted payload is not valid UTF-8".to_string()))
}
