//! # svm-ssh-agent
//!
//! Mô-đun trình phục vụ giao thức OpenSSH Agent qua Unix Domain Socket / Windows Named Pipe.
//!
//! Hỗ trợ nạp các khóa Ed25519 và RSA trực tiếp trong RAM được bảo vệ, sinh khóa SSH chuẩn
//! OpenSSH wire format kèm dấu vân tay `SHA256:...` và quản lý trạng thái daemon.

pub mod keygen;
pub mod server;

pub use keygen::{generate_ssh_keypair, GeneratedSshKey};
pub use server::SshAgentServer;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_ssh_keypair() {
        let kp = generate_ssh_keypair("devops@hephaestus.io");
        assert!(kp
            .public_key_openssh
            .starts_with("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI"));
        assert!(kp.fingerprint_sha256.starts_with("SHA256:"));
        assert!(kp.private_key_pem.contains("BEGIN OPENSSH PRIVATE KEY"));
    }
}
