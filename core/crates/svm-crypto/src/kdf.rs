//! Dẫn xuất khóa mật mã học KDF (PBKDF2-HMAC-SHA256 & Key Encryption Key Derivation).

use crate::hash::hmac_sha256;

/// Dẫn xuất khóa PBKDF2-HMAC-SHA256 chuẩn RFC 8018.
pub fn pbkdf2_hmac_sha256(password: &[u8], salt: &[u8], iterations: u32, out: &mut [u8]) {
    let iters = iterations.max(1);
    let mut block_idx = 1u32;
    let mut offset = 0;

    while offset < out.len() {
        let mut salt_and_idx = Vec::with_capacity(salt.len() + 4);
        salt_and_idx.extend_from_slice(salt);
        salt_and_idx.extend_from_slice(&block_idx.to_be_bytes());

        let mut u = hmac_sha256(password, &salt_and_idx);
        let mut t = u;

        for _ in 1..iters {
            u = hmac_sha256(password, &u);
            for i in 0..32 {
                t[i] ^= u[i];
            }
        }

        let take = (out.len() - offset).min(32);
        out[offset..offset + take].copy_from_slice(&t[..take]);
        offset += take;
        block_idx += 1;
    }
}

/// Dẫn xuất khóa bọc 256-bit (KEK) từ mật khẩu Master Password và Salt 16-byte.
pub fn derive_kek(password: &str, salt: &[u8; 16]) -> [u8; 32] {
    let mut kek = [0u8; 32];
    pbkdf2_hmac_sha256(password.as_bytes(), salt, 10_000, &mut kek);
    kek
}
