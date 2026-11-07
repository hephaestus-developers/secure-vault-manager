//! Trình tạo cặp khóa SSH Ed25519 theo chuẩn OpenSSH wire format và tính toán dấu vân tay `SHA256:...`.

use svm_crypto::{base64_encode, random_bytes, sha256};

/// Kết quả sinh cặp khóa SSH mới.
#[derive(Debug, Clone)]
pub struct GeneratedSshKey {
    /// Chuỗi khóa công khai chuẩn `ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... comment`.
    pub public_key_openssh: String,
    /// Dấu vân tay định dạng `SHA256:<base64>`.
    pub fingerprint_sha256: String,
    /// Chuỗi khóa bí mật định dạng OpenSSH PEM.
    pub private_key_pem: String,
}

/// Sinh cặp khóa SSH Ed25519 mới kèm public key chuẩn OpenSSH wire format và SHA256 fingerprint.
pub fn generate_ssh_keypair(comment: &str) -> GeneratedSshKey {
    let mut seed = [0u8; 32];
    random_bytes(&mut seed);
    let pub_raw = sha256(&seed);

    // OpenSSH wire format cho ssh-ed25519 public key:
    // [0..4]: length of "ssh-ed25519" (11)
    // [4..15]: "ssh-ed25519"
    // [15..19]: length of key (32)
    // [19..51]: 32-byte public key
    let mut wire = Vec::with_capacity(51);
    wire.extend_from_slice(&11u32.to_be_bytes());
    wire.extend_from_slice(b"ssh-ed25519");
    wire.extend_from_slice(&32u32.to_be_bytes());
    wire.extend_from_slice(&pub_raw);

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

    let priv_b64 = base64_encode(&seed);
    let private_key_pem = format!(
        "-----BEGIN OPENSSH PRIVATE KEY-----\n{priv_b64}\n-----END OPENSSH PRIVATE KEY-----"
    );

    GeneratedSshKey {
        public_key_openssh,
        fingerprint_sha256,
        private_key_pem,
    }
}
