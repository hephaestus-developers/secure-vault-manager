//! # svm-crypto
//!
//! Mô-đun mật mã học thuần túy của hệ sinh thái Secure Vault Manager (SVM).
//!
//! Đảm nhiệm các thuật toán băm mật mã (SHA-256, SHA-1), mã xác thực thông điệp
//! (HMAC-SHA256, HMAC-SHA1), dẫn xuất khóa KDF (PBKDF2-HMAC-SHA256 / Memory-Hard KDF),
//! mã hóa đối xứng xác thực AEAD (ChaCha20-Poly1305 theo chuẩn RFC 8439),
//! sinh số ngẫu nhiên CSPRNG, mã hóa Base64/Base32/Hex, sinh mật khẩu, đánh giá
//! độ mạnh mật khẩu (Entropy/zxcvbn) và tự động xóa bộ nhớ (`zeroize`).

pub mod aead;
pub mod encoding;
pub mod hash;
pub mod kdf;
pub mod mem;
pub mod password;

pub use aead::{chacha20_poly1305_decrypt, chacha20_poly1305_encrypt};
pub use encoding::{base32_decode, base64_decode, base64_encode, hex_decode, hex_encode};
pub use hash::{hmac_sha1, hmac_sha256, sha1, sha256};
pub use kdf::{derive_kek, pbkdf2_hmac_sha256};
pub use mem::{
    generate_recovery_key, generate_uuid_v4, mem_lock, random_bytes, seed_entropy, zeroize_slice,
};
pub use password::{evaluate_password_strength, generate_password, PasswordStrength};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sha256_nist_vector() {
        let digest = sha256(b"abc");
        assert_eq!(
            hex_encode(&digest),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
    }

    #[test]
    fn test_sha1_nist_vector() {
        let digest = sha1(b"abc");
        assert_eq!(
            hex_encode(&digest),
            "a9993e364706816aba3e25717850c26c9cd0d89d"
        );
    }

    #[test]
    fn test_chacha20_poly1305_roundtrip_and_tamper() {
        let key = [0x42u8; 32];
        let nonce = [0x24u8; 12];
        let aad = b"svm-header-v1";
        let plaintext = b"Super secret master vault payload";

        let encrypted = chacha20_poly1305_encrypt(&key, &nonce, aad, plaintext);
        let decrypted =
            chacha20_poly1305_decrypt(&key, &nonce, aad, &encrypted).expect("decrypt ok");
        assert_eq!(decrypted, plaintext);

        // Tamper with ciphertext
        let mut tampered = encrypted.clone();
        tampered[0] ^= 0x01;
        assert!(chacha20_poly1305_decrypt(&key, &nonce, aad, &tampered).is_err());
    }

    #[test]
    fn test_base64_and_base32() {
        let raw = b"Hello Hephaestus Vault!";
        let b64 = base64_encode(raw);
        let dec = base64_decode(&b64).expect("b64 decode");
        assert_eq!(dec, raw);

        let b32_dec = base32_decode("JBSWY3DPEHPK3PXP").expect("b32 decode");
        assert!(!b32_dec.is_empty());
    }
}
