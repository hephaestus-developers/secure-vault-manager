//! Tiện ích mã hóa và giải mã nhị phân sang chuỗi: Hex, Base64 và Base32 (RFC 4648).

use svm_domain::DomainError;

/// Mã hóa mảng byte sang chuỗi Hex chữ thường.
pub fn hex_encode(bytes: &[u8]) -> String {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    let mut out = String::with_capacity(bytes.len() * 2);
    for &b in bytes {
        out.push(HEX[(b >> 4) as usize] as char);
        out.push(HEX[(b & 0x0f) as usize] as char);
    }
    out
}

/// Giải mã chuỗi Hex thành mảng byte.
pub fn hex_decode(s: &str) -> Result<Vec<u8>, DomainError> {
    let clean = s.trim();
    if !clean.len().is_multiple_of(2) {
        return Err(DomainError::Parse(
            "Hex string must have even length".to_string(),
        ));
    }
    let mut out = Vec::with_capacity(clean.len() / 2);
    let bytes = clean.as_bytes();
    for i in (0..bytes.len()).step_by(2) {
        let hi = hex_val(bytes[i])?;
        let lo = hex_val(bytes[i + 1])?;
        out.push((hi << 4) | lo);
    }
    Ok(out)
}

fn hex_val(b: u8) -> Result<u8, DomainError> {
    match b {
        b'0'..=b'9' => Ok(b - b'0'),
        b'a'..=b'f' => Ok(b - b'a' + 10),
        b'A'..=b'F' => Ok(b - b'A' + 10),
        _ => Err(DomainError::Parse("Invalid hex character".to_string())),
    }
}

const B64_TABLE: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/// Mã hóa mảng byte sang chuỗi Base64 chuẩn (RFC 4648).
pub fn base64_encode(bytes: &[u8]) -> String {
    let mut out = String::with_capacity(bytes.len().div_ceil(3) * 4);
    for chunk in bytes.chunks(3) {
        let b0 = chunk[0] as u32;
        let b1 = if chunk.len() > 1 { chunk[1] as u32 } else { 0 };
        let b2 = if chunk.len() > 2 { chunk[2] as u32 } else { 0 };
        let n = (b0 << 16) | (b1 << 8) | b2;

        out.push(B64_TABLE[((n >> 18) & 0x3f) as usize] as char);
        out.push(B64_TABLE[((n >> 12) & 0x3f) as usize] as char);
        if chunk.len() > 1 {
            out.push(B64_TABLE[((n >> 6) & 0x3f) as usize] as char);
        } else {
            out.push('=');
        }
        if chunk.len() > 2 {
            out.push(B64_TABLE[(n & 0x3f) as usize] as char);
        } else {
            out.push('=');
        }
    }
    out
}

/// Giải mã chuỗi Base64 thành mảng byte.
pub fn base64_decode(s: &str) -> Result<Vec<u8>, DomainError> {
    let mut vals = Vec::with_capacity(s.len());
    for b in s.bytes() {
        let v = match b {
            b'A'..=b'Z' => b - b'A',
            b'a'..=b'z' => b - b'a' + 26,
            b'0'..=b'9' => b - b'0' + 52,
            b'+' | b'-' => 62,
            b'/' | b'_' => 63,
            b'=' | b' ' | b'\n' | b'\r' | b'\t' => continue,
            _ => return Err(DomainError::Parse("Invalid base64 character".to_string())),
        };
        vals.push(v);
    }

    let mut out = Vec::with_capacity((vals.len() * 3) / 4);
    for chunk in vals.chunks(4) {
        if chunk.len() < 2 {
            break;
        }
        let c0 = chunk[0] as u32;
        let c1 = chunk[1] as u32;
        out.push(((c0 << 2) | (c1 >> 4)) as u8);
        if chunk.len() > 2 {
            let c2 = chunk[2] as u32;
            out.push((((c1 & 0x0f) << 4) | (c2 >> 2)) as u8);
            if chunk.len() > 3 {
                let c3 = chunk[3] as u32;
                out.push((((c2 & 0x03) << 6) | c3) as u8);
            }
        }
    }
    Ok(out)
}

/// Giải mã chuỗi Base32 chuẩn RFC 4648 (dùng cho TOTP secret).
pub fn base32_decode(s: &str) -> Result<Vec<u8>, DomainError> {
    let mut bits: u64 = 0;
    let mut bit_count: u32 = 0;
    let mut out = Vec::new();

    for ch in s.bytes() {
        let val = match ch {
            b'A'..=b'Z' => ch - b'A',
            b'a'..=b'z' => ch - b'a',
            b'2'..=b'7' => ch - b'2' + 26,
            b' ' | b'-' | b'=' | b'\t' | b'\n' | b'\r' => continue,
            _ => return Err(DomainError::Parse("Invalid Base32 character".to_string())),
        };
        bits = (bits << 5) | (val as u64);
        bit_count += 5;
        if bit_count >= 8 {
            bit_count -= 8;
            out.push(((bits >> bit_count) & 0xff) as u8);
        }
    }
    Ok(out)
}
