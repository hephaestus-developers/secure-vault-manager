//! Thuật toán tính toán mã xác thực hai lớp TOTP theo chuẩn RFC 6238 và HOTP theo chuẩn RFC 4226.

#[cfg(not(target_arch = "wasm32"))]
use std::time::{SystemTime, UNIX_EPOCH};

use svm_crypto::{base32_decode, hmac_sha1, hmac_sha256};

use crate::otpauth::parse_otpauth_uri;

/// Tính toán mã số xác thực hai lớp TOTP theo chuẩn RFC 6238 dựa trên secret key và dấu thời gian Unix.
///
/// # Arguments
/// * `secret_or_uri` - Chuỗi khóa bí mật đã được mã hóa Base32 hoặc URI `otpauth://`.
/// * `timestamp` - Dấu thời gian Unix Epoch tính bằng giây (truyền `0` để lấy thời gian thực hiện tại).
///
/// # Returns
/// Bộ đôi giá trị `(mã số OTP 6-8 chữ số dạng chuỗi, số giây hiệu lực còn lại)`.
pub fn generate_totp(secret_or_uri: &str, timestamp: u64) -> (String, u32) {
    #[cfg(not(target_arch = "wasm32"))]
    let ts = if timestamp == 0 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_secs())
            .unwrap_or(0)
    } else {
        timestamp
    };
    #[cfg(target_arch = "wasm32")]
    let ts = timestamp;

    let (secret_b32, digits, period, algo) = if secret_or_uri.trim().starts_with("otpauth://") {
        match parse_otpauth_uri(secret_or_uri) {
            Ok(cfg) => (cfg.secret, cfg.digits, cfg.period as u64, cfg.algorithm),
            Err(_) => (secret_or_uri.to_string(), 6, 30, "SHA1".to_string()),
        }
    } else {
        (secret_or_uri.to_string(), 6, 30, "SHA1".to_string())
    };

    let period = period.max(1);
    let remaining = (period - (ts % period)) as u32;
    let counter = ts / period;

    let key_bytes = match base32_decode(&secret_b32) {
        Ok(b) if !b.is_empty() => b,
        _ => {
            if secret_b32.trim().is_empty() {
                return ("000000".to_string(), remaining);
            }
            secret_b32.as_bytes().to_vec()
        }
    };

    let counter_bytes = counter.to_be_bytes();
    let mac: Vec<u8> = if algo == "SHA256" {
        hmac_sha256(&key_bytes, &counter_bytes).to_vec()
    } else {
        hmac_sha1(&key_bytes, &counter_bytes).to_vec()
    };

    // Dynamic truncation theo RFC 4226 Section 5.3
    let offset = (mac[mac.len() - 1] & 0x0f) as usize;
    let binary = (((mac[offset] & 0x7f) as u32) << 24)
        | ((mac[offset + 1] as u32) << 16)
        | ((mac[offset + 2] as u32) << 8)
        | (mac[offset + 3] as u32);

    let modulo = 10u32.pow(digits);
    let otp = binary % modulo;
    let code = format!("{:0width$}", otp, width = digits as usize);
    (code, remaining)
}
