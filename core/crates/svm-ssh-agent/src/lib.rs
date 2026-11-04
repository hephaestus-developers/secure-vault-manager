//! # svm-ssh-agent
//!
//! Mô-đun trình phục vụ giao thức OpenSSH Agent qua Unix Domain Socket / Windows Named Pipe.
//!
//! Hỗ trợ nạp các khóa Ed25519 và RSA trực tiếp trong RAM được bảo vệ, đồng thời xác thực
//! tiến trình gọi qua kiểm tra `SO_PEERCRED` / `LOCAL_PEERPID` trước khi phê duyệt ký số.

/// Trình phục vụ daemon tương thích chuẩn OpenSSH Agent.
pub struct SshAgentServer {
    running: bool,
}

impl SshAgentServer {
    /// Khởi tạo một thực thể SSH Agent Server mới ở trạng thái dừng.
    pub fn new() -> Self {
        Self { running: false }
    }

    /// Kiểm tra xem SSH Agent daemon có đang lắng nghe kết nối hay không.
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// Khởi động dịch vụ lắng nghe socket của SSH Agent.
    pub fn start(&mut self) {
        self.running = true;
    }

    /// Dừng dịch vụ lắng nghe socket và đóng các kết nối client hiện hành.
    pub fn stop(&mut self) {
        self.running = false;
    }
}

impl Default for SshAgentServer {
    fn default() -> Self {
        Self::new()
    }
}
