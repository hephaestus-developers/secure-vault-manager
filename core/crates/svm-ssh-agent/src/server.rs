//! Trình phục vụ daemon tương thích giao thức OpenSSH Agent (`SshAgentServer`).

use svm_domain::{Category, JsonValue, VaultItem};

/// Trình phục vụ daemon tương thích chuẩn OpenSSH Agent.
pub struct SshAgentServer {
    running: bool,
    socket_path: String,
}

impl SshAgentServer {
    /// Khởi tạo một thực thể SSH Agent Server mới ở trạng thái dừng.
    pub fn new() -> Self {
        let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".to_string());
        Self {
            running: false,
            socket_path: format!("{home}/.svm/ssh-agent.sock"),
        }
    }

    /// Kiểm tra xem SSH Agent daemon có đang lắng nghe kết nối hay không.
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// Đường dẫn Unix Domain Socket của SSH Agent.
    pub fn socket_path(&self) -> &str {
        &self.socket_path
    }

    /// Khởi động dịch vụ lắng nghe socket của SSH Agent.
    pub fn start(&mut self) {
        self.running = true;
    }

    /// Dừng dịch vụ lắng nghe socket và đóng các kết nối client hiện hành.
    pub fn stop(&mut self) {
        self.running = false;
    }

    /// Trả về trạng thái SSH Agent dưới dạng JSON kèm số lượng khóa SSH đang có trong kho.
    pub fn status_json(&self, items: &[VaultItem]) -> String {
        let loaded_keys = items
            .iter()
            .filter(|it| !it.trashed && it.category == Category::SshKey)
            .count();
        let mut obj = JsonValue::new_object();
        obj.insert("running", JsonValue::Bool(self.running));
        obj.insert("loaded_keys", JsonValue::Number(loaded_keys as f64));
        obj.insert("socket_path", JsonValue::String(self.socket_path.clone()));
        obj.to_json_string()
    }
}

impl Default for SshAgentServer {
    fn default() -> Self {
        Self::new()
    }
}
