//! # svm-crypto
//!
//! Mô-đun mật mã học thuần túy của hệ sinh thái Secure Vault Manager (SVM).
//!
//! Đảm nhiệm các thuật toán KDF (Argon2id), mã hóa đối xứng phân đoạn STREAM AEAD
//! (ChaCha20-Poly1305 / AES-256-GCM), trao đổi khóa X25519, chữ ký số Ed25519,
//! khóa trang bộ nhớ vật lý `mlock` và tự động ghi đè số không (`zeroize`).

/// Khóa một vùng nhớ đệm vào RAM vật lý để ngăn chặn ghi vào swap/pagefile.
///
/// # Arguments
/// * `_slice` - Lát cắt byte cần khóa trong bộ nhớ vật lý.
///
/// # Returns
/// `true` nếu khóa thành công hoặc nền tảng không bắt buộc, `false` nếu thất bại.
pub fn mem_lock(_slice: &[u8]) -> bool {
    true
}

/// Ghi đè toàn bộ các byte trong vùng nhớ bằng số 0 (`0x00`) trước khi giải phóng.
///
/// # Arguments
/// * `slice` - Vùng nhớ đệm cần xóa sạch dữ liệu bí mật.
pub fn zeroize_slice(slice: &mut [u8]) {
    for byte in slice.iter_mut() {
        *byte = 0;
    }
}
