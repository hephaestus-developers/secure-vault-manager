//! # stdio
//!
//! Giao thức đóng khung bản tin Native Messaging (4-byte Little-Endian Length-Prefixed Framing)
//! trên luồng `stdin` và `stdout` theo đặc tả Chrome / Firefox WebExtension Native Messaging.

use std::io::{self, Read, Write};
use svm_domain::DomainError;
use svm_ipc::IpcEndpoint;

/// Giới hạn kích thước tối đa của bản tin nhận từ trình duyệt (4 MB, đồng bộ với khung IPC).
pub const MAX_INBOUND_MESSAGE_BYTES: usize = 4 * 1024 * 1024;

/// Giới hạn kích thước tối đa của bản tin gửi về trình duyệt (10 MB).
pub const MAX_OUTBOUND_MESSAGE_BYTES: usize = 10 * 1024 * 1024;

/// Đọc một khung bản tin JSON từ luồng đầu vào (`Read`).
/// Trả về `Ok(None)` khi gặp `EOF` (trình duyệt đóng cổng kết nối).
pub fn read_native_message<R: Read>(reader: &mut R) -> Result<Option<String>, DomainError> {
    let mut len_buf = [0u8; 4];
    match reader.read_exact(&mut len_buf) {
        Ok(()) => {}
        Err(e) if e.kind() == io::ErrorKind::UnexpectedEof => return Ok(None),
        Err(e) => return Err(DomainError::Io(format!("Failed to read header: {e}"))),
    }

    let msg_len = u32::from_le_bytes(len_buf) as usize;
    if msg_len == 0 {
        return Ok(Some(String::new()));
    }
    if msg_len > MAX_INBOUND_MESSAGE_BYTES {
        return Err(DomainError::InvalidArg(format!(
            "Inbound Native Messaging payload exceeds 1MB limit ({msg_len} bytes)"
        )));
    }

    let mut payload = vec![0u8; msg_len];
    reader
        .read_exact(&mut payload)
        .map_err(|e| DomainError::Io(format!("Failed to read message body: {e}")))?;

    String::from_utf8(payload)
        .map(Some)
        .map_err(|_| DomainError::Parse("Native message is not valid UTF-8".to_string()))
}

/// Ghi một khung bản tin JSON ra luồng đầu ra (`Write`) với tiền tố 4-byte Little-Endian.
pub fn write_native_message<W: Write>(
    writer: &mut W,
    json_payload: &str,
) -> Result<(), DomainError> {
    let bytes = json_payload.as_bytes();
    if bytes.len() > MAX_OUTBOUND_MESSAGE_BYTES {
        return Err(DomainError::InvalidArg(
            "Outbound Native Messaging payload exceeds 10MB limit".to_string(),
        ));
    }
    let frame = IpcEndpoint::encode_frame(bytes)?;
    writer
        .write_all(&frame)
        .map_err(|e| DomainError::Io(format!("Failed to write frame: {e}")))?;
    writer
        .flush()
        .map_err(|e| DomainError::Io(format!("Failed to flush stdout: {e}")))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[test]
    fn test_stdio_native_message_roundtrip() {
        let mut buffer = Vec::new();
        let original = r#"{"requestId":"req-1","action":"get_vault_status"}"#;
        write_native_message(&mut buffer, original).expect("write");

        let mut cursor = Cursor::new(buffer);
        let decoded = read_native_message(&mut cursor)
            .expect("read")
            .expect("some message");
        assert_eq!(decoded, original);

        // Next read should return None (EOF)
        let eof = read_native_message(&mut cursor).expect("eof");
        assert!(eof.is_none());
    }
}
