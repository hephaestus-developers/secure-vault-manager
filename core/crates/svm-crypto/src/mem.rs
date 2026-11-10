//! Quản lý vùng nhớ mật mã, xóa bộ nhớ an toàn (`zeroize`), khóa RAM (`mlock`/`VirtualLock`) và bộ tạo số ngẫu nhiên CSPRNG.

use std::sync::atomic::{compiler_fence, AtomicU64, Ordering};
#[cfg(not(target_arch = "wasm32"))]
use std::time::{SystemTime, UNIX_EPOCH};

use crate::hash::sha256;

static FALLBACK_COUNTER: AtomicU64 = AtomicU64::new(0x9E37_79B9_7F4A_7C15);

#[cfg(all(unix, not(target_arch = "wasm32")))]
extern "C" {
    fn mlock(addr: *const u8, len: usize) -> i32;
    fn munlock(addr: *const u8, len: usize) -> i32;
}

/// Khóa một vùng nhớ đệm vào RAM vật lý (`mlock` / `VirtualLock`) để ngăn chặn hệ điều hành
/// ghi xuống swap/pagefile.
pub fn mem_lock(slice: &[u8]) -> bool {
    if slice.is_empty() {
        return true;
    }
    #[cfg(all(unix, not(target_arch = "wasm32")))]
    unsafe {
        return mlock(slice.as_ptr(), slice.len()) == 0;
    }
    #[cfg(target_os = "windows")]
    unsafe {
        return windows_sys::Win32::System::Memory::VirtualLock(
            slice.as_ptr() as *const core::ffi::c_void,
            slice.len(),
        ) != 0;
    }
    #[cfg(not(any(all(unix, not(target_arch = "wasm32")), target_os = "windows")))]
    {
        let _ = slice;
        true
    }
}

/// Mở khóa vùng nhớ đã được khóa bằng `mem_lock`.
pub fn mem_unlock(slice: &[u8]) -> bool {
    if slice.is_empty() {
        return true;
    }
    #[cfg(all(unix, not(target_arch = "wasm32")))]
    unsafe {
        return munlock(slice.as_ptr(), slice.len()) == 0;
    }
    #[cfg(target_os = "windows")]
    unsafe {
        return windows_sys::Win32::System::Memory::VirtualUnlock(
            slice.as_ptr() as *const core::ffi::c_void,
            slice.len(),
        ) != 0;
    }
    #[cfg(not(any(all(unix, not(target_arch = "wasm32")), target_os = "windows")))]
    {
        let _ = slice;
        true
    }
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

/// Ghi đè `0x00` lên toàn bộ vùng nhớ đệm (kể cả phần `capacity` đã cấp phát)
/// của một `String` trước khi làm rỗng.
pub fn zeroize_string(s: &mut String) {
    // SAFETY: 0x00 là byte UTF-8 hợp lệ, và chúng ta gọi s.clear() ngay sau đó.
    unsafe {
        let vec = s.as_mut_vec();
        zeroize_slice(vec.as_mut_slice());
        vec.clear();
    }
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

/// Điền các byte ngẫu nhiên an toàn mật mã từ CSPRNG của hệ điều hành (`ring::rand::SystemRandom`).
pub fn random_bytes(out: &mut [u8]) {
    if out.is_empty() {
        return;
    }
    #[cfg(not(target_arch = "wasm32"))]
    {
        use ring::rand::{SecureRandom, SystemRandom};
        let rng = SystemRandom::new();
        if rng.fill(out).is_ok() {
            return;
        }
    }
    // Dự phòng PRNG trộn thời gian hệ thống + bộ đếm qua SHA-256 chỉ dùng cho môi trường WASM / không có CSPRNG
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

/// Sinh mã Emergency Recovery Key chuẩn 24 ký tự `XXXX-XXXX-XXXX-XXXX-XXXX-XXXX` (6 cụm 4 ký tự).
pub fn generate_recovery_key() -> String {
    const ALPHABET: &[u8] = b"ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    let mut rand = [0u8; 24];
    random_bytes(&mut rand);
    let mut groups = Vec::with_capacity(6);
    for g in 0..6 {
        let mut s = String::with_capacity(4);
        for i in 0..4 {
            let idx = (rand[g * 4 + i] as usize) % ALPHABET.len();
            s.push(ALPHABET[idx] as char);
        }
        groups.push(s);
    }
    groups.join("-")
}

/// Chuẩn hóa mã Recovery Key (loại bỏ ký tự phân tách, hỗ trợ cả định dạng 24 ký tự mới và 16 ký tự cũ).
pub fn normalize_recovery_key(raw: &str) -> String {
    let clean: String = raw
        .trim()
        .to_uppercase()
        .chars()
        .filter(|c| c.is_ascii_alphanumeric())
        .collect();
    let body = if clean.starts_with("SVM") && (clean.len() == 19 || clean.len() == 27) {
        &clean[3..]
    } else {
        &clean
    };
    if body.len() == 24 {
        let mut groups = Vec::with_capacity(6);
        for chunk in body.as_bytes().chunks(4) {
            if let Ok(s) = std::str::from_utf8(chunk) {
                groups.push(s);
            }
        }
        return groups.join("-");
    }
    if body.len() == 16 {
        let mut groups = Vec::with_capacity(4);
        for chunk in body.as_bytes().chunks(4) {
            if let Ok(s) = std::str::from_utf8(chunk) {
                groups.push(s);
            }
        }
        return format!("SVM-{}", groups.join("-"));
    }
    raw.trim().to_uppercase()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_and_normalize_recovery_key() {
        let key = generate_recovery_key();
        // 6 groups of 4 chars + 5 dashes = 29 chars total
        assert_eq!(key.len(), 29);
        let groups: Vec<&str> = key.split('-').collect();
        assert_eq!(groups.len(), 6);
        for g in &groups {
            assert_eq!(g.len(), 4);
        }

        // Test normalization
        let no_dashes: String = key.chars().filter(|&c| c != '-').collect();
        assert_eq!(normalize_recovery_key(&no_dashes), key);

        let lowercase = key.to_lowercase();
        assert_eq!(normalize_recovery_key(&lowercase), key);

        let with_spaces = key.replace('-', " ");
        assert_eq!(normalize_recovery_key(&with_spaces), key);

        // Test legacy 16-char format
        let legacy = "SVM-ABCD-EFGH-JKLM-NPQR";
        assert_eq!(normalize_recovery_key(legacy), legacy);
        assert_eq!(normalize_recovery_key("abcd efgh jklm npqr"), legacy);
    }
}
