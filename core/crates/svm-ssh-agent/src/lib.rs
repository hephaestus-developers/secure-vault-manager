//! # svm-ssh-agent
//!
//! Mô-đun trình phục vụ giao thức OpenSSH Agent qua Unix Domain Socket / Windows Named Pipe.
//!
//! Hỗ trợ nạp các khóa Ed25519 và RSA trực tiếp trong RAM được bảo vệ, sinh khóa SSH chuẩn
//! OpenSSH wire format kèm dấu vân tay `SHA256:...` và quản lý trạng thái daemon.

pub mod keygen;
pub mod server;

pub use keygen::{
    build_openssh_ed25519_pub_wire, generate_ssh_keypair, parse_openssh_ed25519_private_key,
    sign_ssh_ed25519, GeneratedSshKey,
};
pub use server::{handle_agent_message, SshAgentServer};

#[cfg(test)]
mod tests {
    use super::*;
    use ring::signature::{UnparsedPublicKey, ED25519};
    use svm_crypto::generate_uuid_v4;
    use svm_domain::{Category, VaultItem};

    #[test]
    fn test_generate_ssh_keypair() {
        let kp = generate_ssh_keypair("devops@hephaestus.io");
        assert!(kp
            .public_key_openssh
            .starts_with("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI"));
        assert!(kp.fingerprint_sha256.starts_with("SHA256:"));
        assert!(kp.private_key_pem.contains("BEGIN OPENSSH PRIVATE KEY"));

        let (_seed, pub_raw, pub_wire) =
            parse_openssh_ed25519_private_key(&kp.private_key_pem).expect("parse openssh pem");
        assert_eq!(pub_wire, build_openssh_ed25519_pub_wire(&pub_raw));

        // Sign a challenge and verify real Ed25519 signature with ring::signature::ED25519
        let msg = b"git-commit-payload-verify";
        let sig_blob = sign_ssh_ed25519(&kp.private_key_pem, msg).expect("sign");
        // sig_blob: [4B "ssh-ed25519"][11B][4B len=64][64B sig]
        let raw_sig = &sig_blob[4 + 11 + 4..];
        assert_eq!(raw_sig.len(), 64);
        UnparsedPublicKey::new(&ED25519, &pub_raw)
            .verify(msg, raw_sig)
            .expect("valid Ed25519 signature");
    }

    #[cfg(unix)]
    #[test]
    fn test_ssh_agent_unix_socket_identities_and_sign() {
        use std::io::{Read, Write};
        use std::os::unix::net::UnixStream;
        use std::time::Duration;

        let sock_path = format!("/tmp/svm_test_ssh_{}.sock", generate_uuid_v4());
        let kp = generate_ssh_keypair("ci@hephaestus.io");
        let item = VaultItem {
            id: "ssh-test-1".to_string(),
            title: "CI Deploy Key".to_string(),
            category: Category::SshKey,
            ssh_public_key: kp.public_key_openssh.clone(),
            ssh_fingerprint: kp.fingerprint_sha256.clone(),
            notes: kp.private_key_pem.clone(),
            ..Default::default()
        };

        let mut agent = SshAgentServer::with_socket_path(&sock_path);
        agent.sync_keys(true, &[item], false);
        agent.start();
        std::thread::sleep(Duration::from_millis(60));

        let mut stream = UnixStream::connect(&sock_path).expect("connect ssh socket");
        // SSH_AGENTC_REQUEST_IDENTITIES (11)
        let req = [0u8, 0, 0, 1, 11];
        stream.write_all(&req).expect("write req");
        let mut rlen = [0u8; 4];
        stream.read_exact(&mut rlen).expect("read len");
        let mut rbody = vec![0u8; u32::from_be_bytes(rlen) as usize];
        stream.read_exact(&mut rbody).expect("read body");
        assert_eq!(rbody[0], 12); // SSH_AGENT_IDENTITIES_ANSWER
        let count = u32::from_be_bytes([rbody[1], rbody[2], rbody[3], rbody[4]]);
        assert_eq!(count, 1);

        // Extract pub_wire from identities response & send SSH_AGENTC_SIGN_REQUEST (13)
        let klen = u32::from_be_bytes([rbody[5], rbody[6], rbody[7], rbody[8]]) as usize;
        let pub_wire = &rbody[9..9 + klen];
        let challenge = b"ssh-auth-challenge-data";

        let mut sign_msg = vec![13u8];
        sign_msg.extend_from_slice(&(pub_wire.len() as u32).to_be_bytes());
        sign_msg.extend_from_slice(pub_wire);
        sign_msg.extend_from_slice(&(challenge.len() as u32).to_be_bytes());
        sign_msg.extend_from_slice(challenge);
        sign_msg.extend_from_slice(&0u32.to_be_bytes());

        stream
            .write_all(&(sign_msg.len() as u32).to_be_bytes())
            .expect("write sign len");
        stream.write_all(&sign_msg).expect("write sign msg");

        stream.read_exact(&mut rlen).expect("read sign resp len");
        let mut sbody = vec![0u8; u32::from_be_bytes(rlen) as usize];
        stream.read_exact(&mut sbody).expect("read sign resp body");
        assert_eq!(sbody[0], 14); // SSH_AGENT_SIGN_RESPONSE

        agent.stop();
        let _ = std::fs::remove_file(&sock_path);
    }
}
