//! Cấu trúc Header nhị phân của tệp kho dữ liệu `vault.svm` và cơ chế bọc khóa kép (Master + Recovery DEK).

use svm_crypto::{
    chacha20_poly1305_encrypt, derive_kek, normalize_recovery_key, random_bytes, zeroize_slice,
};

/// Magic bytes nhận diện tệp `vault.svm` phiên bản 1.
pub const VAULT_MAGIC: &[u8; 4] = b"SVM1";
/// Phiên bản định dạng tệp `vault.svm`.
pub const VAULT_VERSION: u8 = 1;
/// Tổng kích thước cố định của phần Header + Payload Nonce (169 bytes).
pub const HEADER_LEN: usize = 4 + 1 + 16 + 12 + 48 + 16 + 12 + 48 + 12;

/// Dữ liệu xác thực bổ sung (AAD) khi bọc DEK bằng Master KEK.
pub const AAD_MASTER_DEK: &[u8] = b"svm-master-dek-v1";
/// Dữ liệu xác thực bổ sung (AAD) khi bọc DEK bằng Recovery KEK.
pub const AAD_RECOVERY_DEK: &[u8] = b"svm-recovery-dek-v1";
/// Dữ liệu xác thực bổ sung (AAD) khi mã hóa Payload chính của kho.
pub const AAD_PAYLOAD: &[u8] = b"svm-vault-payload-v1";
/// Dữ liệu xác thực bổ sung (AAD) cho tệp sinh trắc học `biometric.env`.
pub const AAD_BIOMETRIC: &[u8] = b"svm-biometric-dek-v1";
/// Dữ liệu xác thực bổ sung (AAD) cho tệp đính kèm mã hóa.
pub const AAD_ATTACHMENT: &[u8] = b"svm-attachment-v1";

/// Cấu trúc Header của tệp nhị phân `vault.svm` chứa khóa DEK được bọc kép (Master + Recovery).
#[derive(Debug, Clone)]
pub struct VaultHeader {
    /// Salt 16-byte dùng cho dẫn xuất khóa từ Master Password.
    pub master_salt: [u8; 16],
    /// Nonce 12-byte bọc DEK bằng Master KEK.
    pub master_nonce: [u8; 12],
    /// Khóa DEK đã bọc bằng Master KEK (32B ciphertext + 16B Poly1305 tag).
    pub master_wrapped_dek: [u8; 48],
    /// Salt 16-byte dùng cho dẫn xuất khóa từ Recovery Key.
    pub recovery_salt: [u8; 16],
    /// Nonce 12-byte bọc DEK bằng Recovery KEK.
    pub recovery_nonce: [u8; 12],
    /// Khóa DEK đã bọc bằng Recovery KEK (32B ciphertext + 16B Poly1305 tag).
    pub recovery_wrapped_dek: [u8; 48],
}

impl VaultHeader {
    /// Tạo mới hoàn toàn cấu trúc bọc khóa `VaultHeader` cho DEK từ Master Password và Recovery Key.
    pub fn create(dek: &[u8; 32], master_password: &str, recovery_key: &str) -> Self {
        let mut master_salt = [0u8; 16];
        let mut master_nonce = [0u8; 12];
        random_bytes(&mut master_salt);
        random_bytes(&mut master_nonce);

        let mut master_kek = derive_kek(master_password, &master_salt);
        let wrapped_m = chacha20_poly1305_encrypt(&master_kek, &master_nonce, AAD_MASTER_DEK, dek);
        zeroize_slice(&mut master_kek);
        let mut master_wrapped_dek = [0u8; 48];
        master_wrapped_dek.copy_from_slice(&wrapped_m[..48]);

        let mut recovery_salt = [0u8; 16];
        let mut recovery_nonce = [0u8; 12];
        random_bytes(&mut recovery_salt);
        random_bytes(&mut recovery_nonce);

        let norm_rec = normalize_recovery_key(recovery_key);
        let mut recovery_kek = derive_kek(&norm_rec, &recovery_salt);
        let wrapped_r =
            chacha20_poly1305_encrypt(&recovery_kek, &recovery_nonce, AAD_RECOVERY_DEK, dek);
        zeroize_slice(&mut recovery_kek);
        let mut recovery_wrapped_dek = [0u8; 48];
        recovery_wrapped_dek.copy_from_slice(&wrapped_r[..48]);

        Self {
            master_salt,
            master_nonce,
            master_wrapped_dek,
            recovery_salt,
            recovery_nonce,
            recovery_wrapped_dek,
        }
    }

    /// Cập nhật lại khóa bọc Master Password trên `VaultHeader` hiện có (giữ nguyên DEK và Recovery Key).
    pub fn rewrap_master_password(&mut self, dek: &[u8; 32], new_password: &str) {
        random_bytes(&mut self.master_salt);
        random_bytes(&mut self.master_nonce);
        let mut master_kek = derive_kek(new_password, &self.master_salt);
        let wrapped_m =
            chacha20_poly1305_encrypt(&master_kek, &self.master_nonce, AAD_MASTER_DEK, dek);
        zeroize_slice(&mut master_kek);
        self.master_wrapped_dek.copy_from_slice(&wrapped_m[..48]);
    }

    /// Cập nhật lại khóa bọc Recovery Key trên `VaultHeader` hiện có (giữ nguyên DEK và Master Password).
    pub fn rewrap_recovery_key(&mut self, dek: &[u8; 32], new_recovery_key: &str) {
        random_bytes(&mut self.recovery_salt);
        random_bytes(&mut self.recovery_nonce);
        let norm_rec = normalize_recovery_key(new_recovery_key);
        let mut recovery_kek = derive_kek(&norm_rec, &self.recovery_salt);
        let wrapped_r =
            chacha20_poly1305_encrypt(&recovery_kek, &self.recovery_nonce, AAD_RECOVERY_DEK, dek);
        zeroize_slice(&mut recovery_kek);
        self.recovery_wrapped_dek.copy_from_slice(&wrapped_r[..48]);
    }
}
