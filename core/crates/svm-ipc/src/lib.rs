//! # svm-ipc
//!
//! Mô-đun định dạng gói tin IPC nhị phân (4-byte Little-Endian Length-prefixed framing)
//! và thiết lập kênh giao tiếp cục bộ an toàn giữa tiến trình Native Host và ứng dụng Desktop chính.

pub mod endpoint;

pub use endpoint::IpcEndpoint;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ipc_framing_roundtrip() {
        let msg = b"{\"cmd\":\"ping\"}";
        let frame = IpcEndpoint::encode_frame(msg).expect("encode");
        let decoded = IpcEndpoint::decode_frame(&frame).expect("decode");
        assert_eq!(decoded, msg);
    }
}
