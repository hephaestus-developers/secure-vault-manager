//! # svm-bridge
//!
//! Mô-đun cầu nối trình duyệt: Lắng nghe kết nối IPC từ Extension, xác thực bắt cặp
//! mã PIN 6 số (Pairing), bắt tay mật mã X25519 E2EE và máy chủ HTTP Axum dự phòng cục bộ.

/// Máy chủ cầu nối tiếp nhận và điều phối các yêu cầu từ Browser Extension.
pub struct BridgeServer {
    listening: bool,
}

impl BridgeServer {
    /// Khởi tạo một BridgeServer mới ở trạng thái chưa lắng nghe.
    pub fn new() -> Self {
        Self { listening: false }
    }

    /// Kiểm tra xem BridgeServer có đang hoạt động hay không.
    pub fn is_listening(&self) -> bool {
        self.listening
    }

    /// Bắt đầu lắng nghe các kết nối từ Extension.
    pub fn start(&mut self) {
        self.listening = true;
    }

    /// Dừng lắng nghe và đóng toàn bộ các phiên kết nối hiện thời.
    pub fn stop(&mut self) {
        self.listening = false;
    }
}

impl Default for BridgeServer {
    fn default() -> Self {
        Self::new()
    }
}
