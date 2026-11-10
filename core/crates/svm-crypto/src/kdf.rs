//! Dẫn xuất khóa mật mã học KDF (PBKDF2-HMAC-SHA256 & Key Encryption Key Derivation).

use crate::hash::{hmac_sha256, sha256, sha256_compress_block, sha256_compress_words, SHA256_INIT};
use crate::mem::zeroize_slice;

#[inline]
fn precompute_hmac_states(password: &[u8]) -> ([u32; 8], [u32; 8]) {
    let mut k_block = [0u8; 64];
    if password.len() > 64 {
        let hashed = sha256(password);
        k_block[..32].copy_from_slice(&hashed);
    } else {
        k_block[..password.len()].copy_from_slice(password);
    }

    let mut ipad = [0x36u8; 64];
    let mut opad = [0x5cu8; 64];
    for i in 0..64 {
        ipad[i] ^= k_block[i];
        opad[i] ^= k_block[i];
    }

    let mut ipad_state = SHA256_INIT;
    sha256_compress_block(&mut ipad_state, &ipad);

    let mut opad_state = SHA256_INIT;
    sha256_compress_block(&mut opad_state, &opad);

    zeroize_slice(&mut k_block);
    zeroize_slice(&mut ipad);
    zeroize_slice(&mut opad);

    (ipad_state, opad_state)
}

#[inline]
fn hmac_sha256_words_precomputed(
    ipad_state: &[u32; 8],
    opad_state: &[u32; 8],
    words: &[u32; 8],
) -> [u32; 8] {
    let mut block = [0u32; 16];
    block[..8].copy_from_slice(words);
    block[8] = 0x8000_0000;
    // Tổng độ dài: 64 (ipad) + 32 = 96 bytes = 768 bits
    block[15] = 96 * 8;

    let mut h_in = *ipad_state;
    sha256_compress_words(&mut h_in, &block);

    block[..8].copy_from_slice(&h_in);
    let mut h_out = *opad_state;
    sha256_compress_words(&mut h_out, &block);

    h_out
}

/// Dẫn xuất khóa PBKDF2-HMAC-SHA256 chuẩn RFC 8018.
pub fn pbkdf2_hmac_sha256(password: &[u8], salt: &[u8], iterations: u32, out: &mut [u8]) {
    let iters = iterations.max(1);
    let (mut ipad_state, mut opad_state) = precompute_hmac_states(password);
    let mut block_idx = 1u32;
    let mut offset = 0;

    while offset < out.len() {
        let mut salt_and_idx = Vec::with_capacity(salt.len() + 4);
        salt_and_idx.extend_from_slice(salt);
        salt_and_idx.extend_from_slice(&block_idx.to_be_bytes());

        let mut u_bytes = hmac_sha256(password, &salt_and_idx);
        zeroize_slice(&mut salt_and_idx);

        let mut u_words = [0u32; 8];
        for i in 0..8 {
            u_words[i] = u32::from_be_bytes([
                u_bytes[i * 4],
                u_bytes[i * 4 + 1],
                u_bytes[i * 4 + 2],
                u_bytes[i * 4 + 3],
            ]);
        }
        zeroize_slice(&mut u_bytes);
        let mut t_words = u_words;

        for _ in 1..iters {
            u_words = hmac_sha256_words_precomputed(&ipad_state, &opad_state, &u_words);
            for i in 0..8 {
                t_words[i] ^= u_words[i];
            }
        }

        let mut t_bytes = [0u8; 32];
        for i in 0..8 {
            t_bytes[i * 4..i * 4 + 4].copy_from_slice(&t_words[i].to_be_bytes());
        }
        u_words.fill(0);
        t_words.fill(0);

        let take = (out.len() - offset).min(32);
        out[offset..offset + take].copy_from_slice(&t_bytes[..take]);
        zeroize_slice(&mut t_bytes);
        offset += take;
        block_idx += 1;
    }

    ipad_state.fill(0);
    opad_state.fill(0);
}

/// Dẫn xuất khóa bọc 256-bit (KEK) từ mật khẩu Master Password và Salt 16-byte.
pub fn derive_kek(password: &str, salt: &[u8; 16]) -> [u8; 32] {
    let mut kek = [0u8; 32];
    pbkdf2_hmac_sha256(password.as_bytes(), salt, 100_000, &mut kek);
    kek
}
