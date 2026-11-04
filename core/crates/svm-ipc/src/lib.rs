//! # svm-ipc
//!
//! Mô-đun định dạng gói tin IPC nhị phân (Length-prefixed framing) và thiết lập
//! kênh giao tiếp cục bộ an toàn giữa tiến trình Native Host và ứng dụng Desktop chính.

/// Điểm cuối (endpoint) đại diện cho kênh truyền IPC qua Unix Socket hoặc Named Pipe.
pub struct IpcEndpoint {
    /// Đường dẫn tệp socket trên Linux/macOS hoặc tên Named Pipe trên Windows.
    pub socket_path: String,
}

impl IpcEndpoint {
    /// Khởi tạo một IpcEndpoint mới với đường dẫn socket chỉ định.
    ///
    /// # Arguments
    /// * `path` - Chuỗi đường dẫn socket hệ thống.
    pub fn new(path: impl Into<String>) -> Self {
        Self {
            socket_path: path.into(),
        }
    }
}
