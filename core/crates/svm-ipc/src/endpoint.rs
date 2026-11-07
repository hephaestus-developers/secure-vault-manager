//! Điểm cuối kết nối IPC (`IpcEndpoint`) và đóng gói khung bản tin 4-byte Little-Endian.

use svm_domain::DomainError;

/// Điểm cuối (endpoint) đại diện cho kênh truyền IPC qua Unix Socket hoặc Named Pipe.
pub struct IpcEndpoint {
    /// Đường dẫn tệp socket trên Linux/macOS hoặc tên Named Pipe trên Windows.
    pub socket_path: String,
}

impl IpcEndpoint {
    /// Khởi tạo một `IpcEndpoint` mới với đường dẫn socket chỉ định.
    ///
    /// # Arguments
    /// * `path` - Chuỗi đường dẫn socket hệ thống.
    pub fn new(path: impl Into<String>) -> Self {
        Self {
            socket_path: path.into(),
        }
    }

    /// Đóng gói bản tin JSON với tiền tố độ dài 4-byte Little-Endian chuẩn Native Messaging.
    pub fn encode_frame(payload: &[u8]) -> Result<Vec<u8>, DomainError> {
        if payload.len() > 10 * 1024 * 1024 {
            return Err(DomainError::InvalidArg(
                "IPC frame exceeds 10MB limit".to_string(),
            ));
        }
        let len = payload.len() as u32;
        let mut frame = Vec::with_capacity(4 + payload.len());
        frame.extend_from_slice(&len.to_le_bytes());
        frame.extend_from_slice(payload);
        Ok(frame)
    }

    /// Giải mã một khung bản tin 4-byte Little-Endian.
    pub fn decode_frame(frame: &[u8]) -> Result<&[u8], DomainError> {
        if frame.len() < 4 {
            return Err(DomainError::Parse("IPC frame header too short".to_string()));
        }
        let len = u32::from_le_bytes([frame[0], frame[1], frame[2], frame[3]]) as usize;
        if frame.len() < 4 + len {
            return Err(DomainError::Parse(
                "Incomplete IPC frame payload".to_string(),
            ));
        }
        Ok(&frame[4..4 + len])
    }
}
