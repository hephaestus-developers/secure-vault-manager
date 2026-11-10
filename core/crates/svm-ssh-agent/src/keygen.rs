//! Trình tạo cặp khóa SSH Ed25519 thực bằng `ring::signature::Ed25519KeyPair`,
//! đóng gói chuẩn `openssh-key-v1`, giải mã và ký số giao thức OpenSSH Agent.

use ring::signature::{Ed25519KeyPair, KeyPair};
use svm_crypto::{base64_decode, base64_encode, random_bytes, sha256, zeroize_slice};

/// Kết quả sinh cặp khóa SSH mới.
#[derive(Debug, Clone)]
pub struct GeneratedSshKey {
    /// Chuỗi khóa công khai chuẩn `ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... comment`.
    pub public_key_openssh: String,
    /// Dấu vân tay định dạng `SHA256:<base64>`.
    pub fingerprint_sha256: String,
    /// Chuỗi khóa bí mật định dạng OpenSSH PEM (`openssh-key-v1`).
    pub private_key_pem: String,
}

fn push_ssh_string(buf: &mut Vec<u8>, bytes: &[u8]) {
    buf.extend_from_slice(&(bytes.len() as u32).to_be_bytes());
    buf.extend_from_slice(bytes);
}

fn read_ssh_string<'a>(data: &'a [u8], cursor: &mut usize) -> Result<&'a [u8], String> {
    if *cursor + 4 > data.len() {
        return Err("Unexpected EOF reading SSH string length".to_string());
    }
    let len = u32::from_be_bytes([
        data[*cursor],
        data[*cursor + 1],
        data[*cursor + 2],
        data[*cursor + 3],
    ]) as usize;
    *cursor += 4;
    if *cursor + len > data.len() {
        return Err("Unexpected EOF reading SSH string payload".to_string());
    }
    let slice = &data[*cursor..*cursor + len];
    *cursor += len;
    Ok(slice)
}

/// Tạo wire blob chuẩn `ssh-ed25519` từ khóa công khai 32 byte.
pub fn build_openssh_ed25519_pub_wire(pub_raw: &[u8; 32]) -> Vec<u8> {
    let mut wire = Vec::with_capacity(51);
    push_ssh_string(&mut wire, b"ssh-ed25519");
    push_ssh_string(&mut wire, pub_raw);
    wire
}

/// Sinh cặp khóa SSH Ed25519 thực kèm public key chuẩn OpenSSH và private key `openssh-key-v1`.
pub fn generate_ssh_keypair(comment: &str) -> GeneratedSshKey {
    let mut seed = [0u8; 32];
    random_bytes(&mut seed);
    let key_pair =
        Ed25519KeyPair::from_seed_unchecked(&seed).expect("32-byte Ed25519 seed is always valid");
    let mut pub_raw = [0u8; 32];
    pub_raw.copy_from_slice(key_pair.public_key().as_ref());

    let wire = build_openssh_ed25519_pub_wire(&pub_raw);
    let pub_b64 = base64_encode(&wire);
    let fp_digest = sha256(&wire);
    let fp_b64 = base64_encode(&fp_digest).trim_end_matches('=').to_string();

    let clean_comment = if comment.trim().is_empty() {
        "user@secure-vault"
    } else {
        comment.trim()
    };

    let public_key_openssh = format!("ssh-ed25519 {pub_b64} {clean_comment}");
    let fingerprint_sha256 = format!("SHA256:{fp_b64}");

    // Đóng gói chuẩn openssh-key-v1 (unencrypted vì vault đã mã hóa ở tầng lưu trữ)
    let mut check = [0u8; 4];
    random_bytes(&mut check);

    let mut priv_section = Vec::with_capacity(160);
    priv_section.extend_from_slice(&check);
    priv_section.extend_from_slice(&check);
    push_ssh_string(&mut priv_section, b"ssh-ed25519");
    push_ssh_string(&mut priv_section, &pub_raw);

    let mut priv64 = [0u8; 64];
    priv64[..32].copy_from_slice(&seed);
    priv64[32..].copy_from_slice(&pub_raw);
    push_ssh_string(&mut priv_section, &priv64);
    push_ssh_string(&mut priv_section, clean_comment.as_bytes());
    zeroize_slice(&mut priv64);
    zeroize_slice(&mut seed);

    let mut pad: u8 = 1;
    while priv_section.len() % 8 != 0 {
        priv_section.push(pad);
        pad += 1;
    }

    let mut raw_key = Vec::with_capacity(256);
    raw_key.extend_from_slice(b"openssh-key-v1\0");
    push_ssh_string(&mut raw_key, b"none");
    push_ssh_string(&mut raw_key, b"none");
    push_ssh_string(&mut raw_key, b"");
    raw_key.extend_from_slice(&1u32.to_be_bytes());
    push_ssh_string(&mut raw_key, &wire);
    push_ssh_string(&mut raw_key, &priv_section);
    zeroize_slice(&mut priv_section);

    let encoded = base64_encode(&raw_key);
    zeroize_slice(&mut raw_key);

    let wrapped: Vec<&str> = encoded
        .as_bytes()
        .chunks(70)
        .filter_map(|c| std::str::from_utf8(c).ok())
        .collect();
    let private_key_pem = format!(
        "-----BEGIN OPENSSH PRIVATE KEY-----\n{}\n-----END OPENSSH PRIVATE KEY-----",
        wrapped.join("\n")
    );

    GeneratedSshKey {
        public_key_openssh,
        fingerprint_sha256,
        private_key_pem,
    }
}

/// Giải mã khóa bí mật Ed25519 từ chuỗi PEM chuẩn `openssh-key-v1`, trả về `(seed, pubkey, pub_wire)`.
pub fn parse_openssh_ed25519_private_key(
    pem: &str,
) -> Result<([u8; 32], [u8; 32], Vec<u8>), String> {
    let body: String = pem
        .lines()
        .map(str::trim)
        .filter(|l| !l.is_empty() && !l.starts_with("-----"))
        .collect();
    if body.is_empty() {
        return Err("Empty OpenSSH private key PEM".to_string());
    }
    let mut raw =
        base64_decode(&body).map_err(|e| format!("Invalid base64 PEM: {}", e.message()))?;
    let magic = b"openssh-key-v1\0";
    if !raw.starts_with(magic) {
        zeroize_slice(&mut raw);
        return Err("Not an openssh-key-v1 private key".to_string());
    }

    let mut cur = magic.len();
    let res: Result<([u8; 32], [u8; 32], Vec<u8>), String> = (|| {
        let cipher = read_ssh_string(&raw, &mut cur)?;
        let _kdf = read_ssh_string(&raw, &mut cur)?;
        let _kdfopts = read_ssh_string(&raw, &mut cur)?;
        if cipher != b"none" {
            return Err("Encrypted OpenSSH private key is not supported in-agent".to_string());
        }
        if cur + 4 > raw.len() {
            return Err("Truncated key count".to_string());
        }
        let nkeys = u32::from_be_bytes([raw[cur], raw[cur + 1], raw[cur + 2], raw[cur + 3]]);
        cur += 4;
        if nkeys != 1 {
            return Err(format!("Unsupported OpenSSH key count: {nkeys}"));
        }
        let pub_wire = read_ssh_string(&raw, &mut cur)?.to_vec();
        let priv_blob = read_ssh_string(&raw, &mut cur)?;

        let mut pcur = 0usize;
        if priv_blob.len() < 8 {
            return Err("Truncated private section".to_string());
        }
        if priv_blob[0..4] != priv_blob[4..8] {
            return Err("OpenSSH checkint mismatch".to_string());
        }
        pcur += 8;

        let key_type = read_ssh_string(priv_blob, &mut pcur)?;
        if key_type != b"ssh-ed25519" {
            return Err("Only ssh-ed25519 keys are supported".to_string());
        }
        let pub_slice = read_ssh_string(priv_blob, &mut pcur)?;
        let priv64_slice = read_ssh_string(priv_blob, &mut pcur)?;
        if pub_slice.len() != 32 || priv64_slice.len() != 64 {
            return Err("Invalid Ed25519 key material length".to_string());
        }
        let mut seed = [0u8; 32];
        let mut pub_raw = [0u8; 32];
        seed.copy_from_slice(&priv64_slice[..32]);
        pub_raw.copy_from_slice(pub_slice);
        Ok((seed, pub_raw, pub_wire))
    })();

    zeroize_slice(&mut raw);
    res
}

/// Ký thông điệp dữ liệu SSH bằng khóa Ed25519 và đóng gói signature blob chuẩn SSH Agent (`[string "ssh-ed25519", string sig]`).
pub fn sign_ssh_ed25519(pem: &str, data: &[u8]) -> Result<Vec<u8>, String> {
    let (mut seed, pub_raw, _) = parse_openssh_ed25519_private_key(pem)?;
    let kp = Ed25519KeyPair::from_seed_and_public_key(&seed, &pub_raw)
        .map_err(|_| "Invalid Ed25519 keypair material".to_string())?;
    zeroize_slice(&mut seed);
    let sig = kp.sign(data);

    let mut sig_blob = Vec::with_capacity(4 + 11 + 4 + 64);
    push_ssh_string(&mut sig_blob, b"ssh-ed25519");
    push_ssh_string(&mut sig_blob, sig.as_ref());
    Ok(sig_blob)
}
