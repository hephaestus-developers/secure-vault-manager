//! Mã hóa nhị phân CBOR (RFC 8949) cho WebAuthn Attestation Object và COSE Key.

pub fn encode_uint(val: u64, out: &mut Vec<u8>) {
    if val < 24 {
        out.push(val as u8);
    } else if val <= 0xff {
        out.push(0x18);
        out.push(val as u8);
    } else if val <= 0xffff {
        out.push(0x19);
        out.extend_from_slice(&(val as u16).to_be_bytes());
    } else if val <= 0xffff_ffff {
        out.push(0x1a);
        out.extend_from_slice(&(val as u32).to_be_bytes());
    } else {
        out.push(0x1b);
        out.extend_from_slice(&val.to_be_bytes());
    }
}

pub fn encode_neg_int(val: i64, out: &mut Vec<u8>) {
    // Trong CBOR, số nguyên âm -1 - n được mã hóa với major type 1 (0x20)
    let n = (-1 - val) as u64;
    if n < 24 {
        out.push(0x20 | (n as u8));
    } else if n <= 0xff {
        out.push(0x38);
        out.push(n as u8);
    } else if n <= 0xffff {
        out.push(0x39);
        out.extend_from_slice(&(n as u16).to_be_bytes());
    } else {
        out.push(0x3a);
        out.extend_from_slice(&(n as u32).to_be_bytes());
    }
}

pub fn encode_bytes(bytes: &[u8], out: &mut Vec<u8>) {
    let len = bytes.len();
    if len < 24 {
        out.push(0x40 | (len as u8));
    } else if len <= 0xff {
        out.push(0x58);
        out.push(len as u8);
    } else if len <= 0xffff {
        out.push(0x59);
        out.extend_from_slice(&(len as u16).to_be_bytes());
    } else {
        out.push(0x5a);
        out.extend_from_slice(&(len as u32).to_be_bytes());
    }
    out.extend_from_slice(bytes);
}

pub fn encode_text(text: &str, out: &mut Vec<u8>) {
    let bytes = text.as_bytes();
    let len = bytes.len();
    if len < 24 {
        out.push(0x60 | (len as u8));
    } else if len <= 0xff {
        out.push(0x78);
        out.push(len as u8);
    } else if len <= 0xffff {
        out.push(0x79);
        out.extend_from_slice(&(len as u16).to_be_bytes());
    } else {
        out.push(0x7a);
        out.extend_from_slice(&(len as u32).to_be_bytes());
    }
    out.extend_from_slice(bytes);
}

pub fn encode_map_header(len: usize, out: &mut Vec<u8>) {
    if len < 24 {
        out.push(0xa0 | (len as u8));
    } else if len <= 0xff {
        out.push(0xb8);
        out.push(len as u8);
    } else {
        out.push(0xb9);
        out.extend_from_slice(&(len as u16).to_be_bytes());
    }
}

/// Đóng gói Attestation Object định dạng `fmt: "none"` theo chuẩn WebAuthn Level 3.
/// ```cbor
/// {
///   "fmt": "none",
///   "attStmt": {},
///   "authData": h'...'
/// }
/// ```
pub fn encode_attestation_object_none(auth_data: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(auth_data.len() + 64);
    encode_map_header(3, &mut out);

    // 1. fmt: "none"
    encode_text("fmt", &mut out);
    encode_text("none", &mut out);

    // 2. attStmt: {}
    encode_text("attStmt", &mut out);
    encode_map_header(0, &mut out);

    // 3. authData: h'...'
    encode_text("authData", &mut out);
    encode_bytes(auth_data, &mut out);

    out
}

/// Đóng gói COSE Key cho thuật toán ES256 (ECDSA P-256) (RFC 8152 / RFC 9052).
/// Các trường bắt buộc:
/// - 1: 2 (kty: EC2)
/// - 3: -7 (alg: ES256)
/// - -1: 1 (crv: P-256)
/// - -2: x-coordinate (32 bytes)
/// - -3: y-coordinate (32 bytes)
pub fn encode_cose_key_p256(x_bytes: &[u8], y_bytes: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(80);
    encode_map_header(5, &mut out);

    // 1 (kty) -> 2 (EC2)
    encode_uint(1, &mut out);
    encode_uint(2, &mut out);

    // 3 (alg) -> -7 (ES256)
    encode_uint(3, &mut out);
    encode_neg_int(-7, &mut out);

    // -1 (crv) -> 1 (P-256)
    encode_neg_int(-1, &mut out);
    encode_uint(1, &mut out);

    // -2 (x) -> 32 bytes
    encode_neg_int(-2, &mut out);
    encode_bytes(x_bytes, &mut out);

    // -3 (y) -> 32 bytes
    encode_neg_int(-3, &mut out);
    encode_bytes(y_bytes, &mut out);

    out
}

/// Đóng gói COSE Key cho thuật toán EdDSA (Ed25519 / RFC 8152).
/// - 1: 1 (kty: OKP)
/// - 3: -8 (alg: EdDSA)
/// - -1: 6 (crv: Ed25519)
/// - -2: x-coordinate (32 bytes public key)
pub fn encode_cose_key_ed25519(public_key: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(48);
    encode_map_header(4, &mut out);

    // 1 (kty) -> 1 (OKP)
    encode_uint(1, &mut out);
    encode_uint(1, &mut out);

    // 3 (alg) -> -8 (EdDSA)
    encode_uint(3, &mut out);
    encode_neg_int(-8, &mut out);

    // -1 (crv) -> 6 (Ed25519)
    encode_neg_int(-1, &mut out);
    encode_uint(6, &mut out);

    // -2 (x) -> 32 bytes
    encode_neg_int(-2, &mut out);
    encode_bytes(public_key, &mut out);

    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cbor_primitives() {
        let mut out = Vec::new();
        encode_uint(10, &mut out);
        assert_eq!(out, vec![10]);

        out.clear();
        encode_neg_int(-7, &mut out);
        assert_eq!(out, vec![0x26]);

        out.clear();
        encode_text("none", &mut out);
        assert_eq!(out, vec![0x64, b'n', b'o', b'n', b'e']);
    }

    #[test]
    fn test_attestation_object_encoding() {
        let auth_data = vec![0x01, 0x02, 0x03, 0x04];
        let obj = encode_attestation_object_none(&auth_data);
        assert_eq!(obj[0], 0xa3); // Map with 3 items
    }
}
