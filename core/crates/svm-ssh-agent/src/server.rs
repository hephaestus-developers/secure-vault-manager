//! Trình phục vụ daemon thực tương thích giao thức OpenSSH Agent (`draft-miller-ssh-agent`).

use std::io::{Read, Write};
use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread::{self, JoinHandle};
use std::time::Duration;

#[cfg(unix)]
use std::os::unix::net::UnixListener;

use svm_crypto::zeroize_string;
use svm_domain::{Category, JsonValue, VaultItem};

use crate::keygen::{parse_openssh_ed25519_private_key, sign_ssh_ed25519};

const SSH_AGENT_FAILURE: u8 = 5;
const SSH_AGENTC_REQUEST_IDENTITIES: u8 = 11;
const SSH_AGENT_IDENTITIES_ANSWER: u8 = 12;
const SSH_AGENTC_SIGN_REQUEST: u8 = 13;
const SSH_AGENT_SIGN_RESPONSE: u8 = 14;

/// Khóa SSH đang được giữ trong bộ nhớ của SSH Agent.
#[derive(Clone)]
pub struct LoadedSshKey {
    pub public_wire: Vec<u8>,
    pub comment: String,
    pub fingerprint: String,
    pub private_pem: String,
}

impl Drop for LoadedSshKey {
    fn drop(&mut self) {
        zeroize_string(&mut self.private_pem);
    }
}

/// Trạng thái chia sẻ giữa tiến trình chính và luồng lắng nghe socket SSH Agent.
pub struct SshAgentSharedState {
    pub unlocked: AtomicBool,
    pub prompt_approval: AtomicBool,
    pub keys: Mutex<Vec<LoadedSshKey>>,
    pub approval_cb: Mutex<Option<Arc<dyn Fn(&str, &str) -> bool + Send + Sync>>>,
}

fn push_string(buf: &mut Vec<u8>, data: &[u8]) {
    buf.extend_from_slice(&(data.len() as u32).to_be_bytes());
    buf.extend_from_slice(data);
}

fn read_string<'a>(data: &'a [u8], cur: &mut usize) -> Option<&'a [u8]> {
    if *cur + 4 > data.len() {
        return None;
    }
    let len =
        u32::from_be_bytes([data[*cur], data[*cur + 1], data[*cur + 2], data[*cur + 3]]) as usize;
    *cur += 4;
    if *cur + len > data.len() {
        return None;
    }
    let slice = &data[*cur..*cur + len];
    *cur += len;
    Some(slice)
}

/// Xử lý một bản tin OpenSSH Agent đơn lẻ và trả về gói phản hồi wire format `[type][payload]`.
pub fn handle_agent_message(msg: &[u8], shared: &SshAgentSharedState) -> Vec<u8> {
    let Some(&msg_type) = msg.first() else {
        return vec![SSH_AGENT_FAILURE];
    };
    let body = &msg[1..];
    let is_unlocked = shared.unlocked.load(Ordering::SeqCst);

    match msg_type {
        SSH_AGENTC_REQUEST_IDENTITIES => {
            if !is_unlocked {
                let mut out = Vec::with_capacity(5);
                out.push(SSH_AGENT_IDENTITIES_ANSWER);
                out.extend_from_slice(&0u32.to_be_bytes());
                return out;
            }
            let guard = shared.keys.lock().unwrap_or_else(|e| e.into_inner());
            let mut out = Vec::with_capacity(16 + guard.len() * 96);
            out.push(SSH_AGENT_IDENTITIES_ANSWER);
            out.extend_from_slice(&(guard.len() as u32).to_be_bytes());
            for k in guard.iter() {
                push_string(&mut out, &k.public_wire);
                push_string(&mut out, k.comment.as_bytes());
            }
            out
        }
        SSH_AGENTC_SIGN_REQUEST => {
            if !is_unlocked {
                return vec![SSH_AGENT_FAILURE];
            }
            let mut cur = 0usize;
            let Some(key_blob) = read_string(body, &mut cur) else {
                return vec![SSH_AGENT_FAILURE];
            };
            let Some(data_to_sign) = read_string(body, &mut cur) else {
                return vec![SSH_AGENT_FAILURE];
            };
            let matched = {
                let guard = shared.keys.lock().unwrap_or_else(|e| e.into_inner());
                guard.iter().find(|k| k.public_wire == key_blob).cloned()
            };
            let Some(key) = matched else {
                return vec![SSH_AGENT_FAILURE];
            };

            if shared.prompt_approval.load(Ordering::SeqCst) {
                let cb_opt = shared
                    .approval_cb
                    .lock()
                    .unwrap_or_else(|e| e.into_inner())
                    .clone();
                if let Some(cb) = cb_opt {
                    if !cb(&key.comment, &key.fingerprint) {
                        return vec![SSH_AGENT_FAILURE];
                    }
                }
            }

            match sign_ssh_ed25519(&key.private_pem, data_to_sign) {
                Ok(sig_blob) => {
                    let mut out = Vec::with_capacity(5 + sig_blob.len());
                    out.push(SSH_AGENT_SIGN_RESPONSE);
                    push_string(&mut out, &sig_blob);
                    out
                }
                Err(_) => vec![SSH_AGENT_FAILURE],
            }
        }
        _ => vec![SSH_AGENT_FAILURE],
    }
}

fn serve_stream<S: Read + Write>(mut stream: S, shared: &SshAgentSharedState) {
    loop {
        let mut len_buf = [0u8; 4];
        if stream.read_exact(&mut len_buf).is_err() {
            break;
        }
        let msg_len = u32::from_be_bytes(len_buf) as usize;
        if msg_len == 0 || msg_len > 1024 * 1024 {
            break;
        }
        let mut msg = vec![0u8; msg_len];
        if stream.read_exact(&mut msg).is_err() {
            break;
        }
        let resp = handle_agent_message(&msg, shared);
        let resp_len = (resp.len() as u32).to_be_bytes();
        if stream.write_all(&resp_len).is_err()
            || stream.write_all(&resp).is_err()
            || stream.flush().is_err()
        {
            break;
        }
    }
}

/// Trình phục vụ daemon tương thích chuẩn OpenSSH Agent.
pub struct SshAgentServer {
    running: Arc<AtomicBool>,
    socket_path: String,
    shared: Arc<SshAgentSharedState>,
    listener_handle: Option<JoinHandle<()>>,
}

impl SshAgentServer {
    /// Khởi tạo một thực thể SSH Agent Server mới với đường dẫn socket mặc định.
    pub fn new() -> Self {
        let sock = if let Ok(runtime_dir) = std::env::var("XDG_RUNTIME_DIR") {
            format!("{runtime_dir}/svm/ssh-agent.sock")
        } else {
            let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".to_string());
            format!("{home}/.svm/ssh-agent.sock")
        };
        Self::with_socket_path(sock)
    }

    /// Khởi tạo `SshAgentServer` với đường dẫn socket tùy chỉnh (phục vụ kiểm thử hoặc cấu hình riêng).
    pub fn with_socket_path(socket_path: impl Into<String>) -> Self {
        Self {
            running: Arc::new(AtomicBool::new(false)),
            socket_path: socket_path.into(),
            shared: Arc::new(SshAgentSharedState {
                unlocked: AtomicBool::new(false),
                prompt_approval: AtomicBool::new(true),
                keys: Mutex::new(Vec::new()),
                approval_cb: Mutex::new(None),
            }),
            listener_handle: None,
        }
    }

    /// Tham chiếu trạng thái chia sẻ (phục vụ kiểm thử hoặc gắn callback phê duyệt).
    pub fn shared_state(&self) -> Arc<SshAgentSharedState> {
        Arc::clone(&self.shared)
    }

    /// Đăng ký callback yêu cầu người dùng phê duyệt trước khi ký yêu cầu SSH.
    pub fn set_approval_callback<F>(&self, cb: F)
    where
        F: Fn(&str, &str) -> bool + Send + Sync + 'static,
    {
        let mut guard = self
            .shared
            .approval_cb
            .lock()
            .unwrap_or_else(|e| e.into_inner());
        *guard = Some(Arc::new(cb));
    }

    /// Đồng bộ danh sách khóa SSH hợp lệ từ Vault đang mở khóa vào RAM của Agent.
    pub fn sync_keys(&self, unlocked: bool, items: &[VaultItem], prompt_approval: bool) {
        self.shared.unlocked.store(unlocked, Ordering::SeqCst);
        self.shared
            .prompt_approval
            .store(prompt_approval, Ordering::SeqCst);

        let mut loaded = Vec::new();
        if unlocked {
            for it in items {
                if it.trashed || it.category != Category::SshKey {
                    continue;
                }
                let pem = if it.notes.contains("BEGIN OPENSSH PRIVATE KEY") {
                    it.notes.as_str()
                } else if it.password.contains("BEGIN OPENSSH PRIVATE KEY") {
                    it.password.as_str()
                } else {
                    continue;
                };
                if let Ok((_, _, pub_wire)) = parse_openssh_ed25519_private_key(pem) {
                    loaded.push(LoadedSshKey {
                        public_wire: pub_wire,
                        comment: if it.title.is_empty() {
                            it.username.clone()
                        } else {
                            it.title.clone()
                        },
                        fingerprint: it.ssh_fingerprint.clone(),
                        private_pem: pem.to_string(),
                    });
                }
            }
        }
        let mut guard = self.shared.keys.lock().unwrap_or_else(|e| e.into_inner());
        *guard = loaded;
    }

    /// Kiểm tra xem SSH Agent daemon có đang lắng nghe kết nối hay không.
    pub fn is_running(&self) -> bool {
        self.running.load(Ordering::SeqCst)
    }

    /// Đường dẫn Unix Domain Socket của SSH Agent.
    pub fn socket_path(&self) -> &str {
        &self.socket_path
    }

    /// Khởi động luồng lắng nghe kết nối Unix Domain Socket của SSH Agent.
    pub fn start(&mut self) {
        if self.running.load(Ordering::SeqCst) {
            return;
        }
        self.running.store(true, Ordering::SeqCst);

        #[cfg(unix)]
        {
            let sock_path = self.socket_path.clone();
            if let Some(parent) = Path::new(&sock_path).parent() {
                let _ = std::fs::create_dir_all(parent);
                #[cfg(unix)]
                {
                    use std::os::unix::fs::PermissionsExt;
                    let _ =
                        std::fs::set_permissions(parent, std::fs::Permissions::from_mode(0o700));
                }
            }
            let _ = std::fs::remove_file(&sock_path);

            let Ok(listener) = UnixListener::bind(&sock_path) else {
                self.running.store(false, Ordering::SeqCst);
                return;
            };
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                let _ =
                    std::fs::set_permissions(&sock_path, std::fs::Permissions::from_mode(0o600));
            }
            let _ = listener.set_nonblocking(true);

            let running = Arc::clone(&self.running);
            let shared = Arc::clone(&self.shared);
            self.listener_handle = Some(thread::spawn(move || {
                while running.load(Ordering::SeqCst) {
                    match listener.accept() {
                        Ok((stream, _)) => {
                            let _ = stream.set_nonblocking(false);
                            let shared_conn = Arc::clone(&shared);
                            thread::spawn(move || serve_stream(stream, &shared_conn));
                        }
                        Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                            thread::sleep(Duration::from_millis(50));
                        }
                        Err(_) => break,
                    }
                }
            }));
        }
    }

    /// Dừng luồng lắng nghe socket và thu hồi khóa đang giữ trong Agent.
    pub fn stop(&mut self) {
        self.running.store(false, Ordering::SeqCst);
        if let Some(handle) = self.listener_handle.take() {
            let _ = handle.join();
        }
        let _ = std::fs::remove_file(&self.socket_path);
    }

    /// Trả về trạng thái SSH Agent dưới dạng JSON kèm số lượng khóa SSH đang có trong kho.
    pub fn status_json(&self, items: &[VaultItem]) -> String {
        let loaded_keys = items
            .iter()
            .filter(|it| !it.trashed && it.category == Category::SshKey)
            .count();
        let mut obj = JsonValue::new_object();
        obj.insert("running", JsonValue::Bool(self.is_running()));
        obj.insert("loaded_keys", JsonValue::Number(loaded_keys as f64));
        obj.insert("socket_path", JsonValue::String(self.socket_path.clone()));
        obj.to_json_string()
    }
}

impl Drop for SshAgentServer {
    fn drop(&mut self) {
        self.stop();
    }
}

impl Default for SshAgentServer {
    fn default() -> Self {
        Self::new()
    }
}
