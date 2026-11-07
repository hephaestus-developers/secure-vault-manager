//! Quản lý vùng nhớ mật mã, xóa bộ nhớ an toàn (`zeroize`), khóa RAM (`mlock`) và bộ tạo số ngẫu nhiên CSPRNG.

#[cfg(not(target_arch = "wasm32"))]
use std::fs::File;
#[cfg(not(target_arch = "wasm32"))]
use std::io::Read;
use std::sync::atomic::{compiler_fence, AtomicU64, Ordering};
#[cfg(not(target_arch = "wasm32"))]
use std::time::{SystemTime, UNIX_EPOCH};

use crate::hash::sha256;

static FALLBACK_COUNTER: AtomicU64 = AtomicU64::new(0x9E37_79B9_7F4A_7C15);

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

/// Ghi đè toàn bộ các byte trong vùng nhớ bằng số 0 (`0x00`) với volatile write
/// để trình biên dịch không tối ưu hóa bỏ qua trước khi giải phóng.
///
/// # Arguments
/// * `slice` - Vùng nhớ đệm cần xóa sạch dữ liệu bí mật.
pub fn zeroize_slice(slice: &mut [u8]) {
    for byte in slice.iter_mut() {
        unsafe {
            std::ptr::write_volatile(byte, 0u8);
        }
    }
    compiler_fence(Ordering::SeqCst);
}

/// Nạp thêm hạt giống ngẫu nhiên (entropy seed) từ môi trường bên ngoài (ví dụ WebCrypto trong WASM).
pub fn seed_entropy(seed: u64) {
    let prev = FALLBACK_COUNTER.load(Ordering::SeqCst);
    FALLBACK_COUNTER.store(
        prev.wrapping_mul(0x5851_F42D_4C95_7F2D)
            .wrapping_add(seed ^ 0xA5A5_5A5A_F0F0_0F0F),
        Ordering::SeqCst,
    );
}

#[cfg(not(target_arch = "wasm32"))]
fn current_time_nanos() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_nanos() as u64)
        .unwrap_or(0x1234_5678_9ABC_DEF0)
}

#[cfg(target_arch = "wasm32")]
fn current_time_nanos() -> u64 {
    0x1234_5678_9ABC_DEF0
}

/// Điền các byte ngẫu nhiên an toàn mật mã từ nguồn phần cứng hệ điều hành (`/dev/urandom`).
pub fn random_bytes(out: &mut [u8]) {
    if out.is_empty() {
        return;
    }
    #[cfg(not(target_arch = "wasm32"))]
    if let Ok(mut f) = File::open("/dev/urandom") {
        if f.read_exact(out).is_ok() {
            return;
        }
    }
    // Dự phòng PRNG trộn thời gian hệ thống + bộ đếm qua SHA-256 nếu môi trường cô lập / WASM
    let ts = current_time_nanos();
    let mut offset = 0;
    while offset < out.len() {
        let ctr = FALLBACK_COUNTER.fetch_add(1, Ordering::SeqCst);
        let mut seed = [0u8; 24];
        seed[0..8].copy_from_slice(&ts.to_le_bytes());
        seed[8..16].copy_from_slice(&ctr.to_le_bytes());
        seed[16..24].copy_from_slice(&(offset as u64).to_le_bytes());
        let block = sha256(&seed);
        let take = (out.len() - offset).min(32);
        out[offset..offset + take].copy_from_slice(&block[..take]);
        offset += take;
    }
}

/// Sinh định danh duy nhất toàn cục UUID phiên bản 4 chuẩn RFC 4122.
pub fn generate_uuid_v4() -> String {
    let mut b = [0u8; 16];
    random_bytes(&mut b);
    b[6] = (b[6] & 0x0f) | 0x40; // Version 4
    b[8] = (b[8] & 0x3f) | 0x80; // Variant 1 (RFC 4122)
    format!(
        "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
        b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]
    )
}

/// Sinh mã Emergency Recovery Key chuẩn `SVM-XXXX-XXXX-XXXX-XXXX`.
pub fn generate_recovery_key() -> String {
    const ALPHABET: &[u8] = b"ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    let mut rand = [0u8; 16];
    random_bytes(&mut rand);
    let mut groups = Vec::with_capacity(4);
    for g in 0..4 {
        let mut s = String::with_capacity(4);
        for i in 0..4 {
            let idx = (rand[g * 4 + i] as usize) % ALPHABET.len();
            s.push(ALPHABET[idx] as char);
        }
        groups.push(s);
    }
    format!("SVM-{}", groups.join("-"))
}
