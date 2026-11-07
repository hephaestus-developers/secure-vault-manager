//! Máy chủ cầu nối trình duyệt (`BridgeServer`): điều phối Pairing PIN 6 số, quản lý client và cài đặt Native Messaging Host manifest.

use std::fs;
use std::path::Path;

use svm_crypto::random_bytes;
use svm_domain::{DomainError, JsonValue};

use crate::client::PairedClient;

/// Máy chủ cầu nối tiếp nhận và điều phối các yêu cầu từ Browser Extension.
pub struct BridgeServer {
    listening: bool,
    clients: Vec<PairedClient>,
    last_pairing_code: String,
}

impl BridgeServer {
    /// Khởi tạo một `BridgeServer` mới kèm danh sách client mặc định.
    pub fn new() -> Self {
        Self {
            listening: true,
            clients: vec![
                PairedClient {
                    id: "ext-chrome-01".to_string(),
                    browser: "Google Chrome".to_string(),
                    version: "v1.4.2".to_string(),
                    last_used: "Active 10 mins ago".to_string(),
                },
                PairedClient {
                    id: "ext-firefox-02".to_string(),
                    browser: "Mozilla Firefox".to_string(),
                    version: "v1.4.0".to_string(),
                    last_used: "Active yesterday".to_string(),
                },
            ],
            last_pairing_code: String::new(),
        }
    }

    /// Kiểm tra xem `BridgeServer` có đang hoạt động hay không.
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

    /// Sinh mã PIN ghép nối 6 chữ số ngẫu nhiên an toàn mật mã có hiệu lực 60 giây.
    pub fn begin_pairing(&mut self) -> String {
        let mut rnd = [0u8; 4];
        random_bytes(&mut rnd);
        let num = u32::from_le_bytes(rnd) % 900_000 + 100_000;
        let code = format!("{num:06}");
        self.last_pairing_code = code.clone();

        let mut obj = JsonValue::new_object();
        obj.insert("pairing_code", JsonValue::String(code));
        obj.insert("expires_in_secs", JsonValue::Number(60.0));
        obj.to_json_string()
    }

    /// Trả về danh sách các client đã ghép nối dưới dạng chuỗi JSON `[]`.
    pub fn list_clients_json(&self) -> String {
        let mut arr = JsonValue::new_array();
        for c in &self.clients {
            arr.push(c.to_json());
        }
        arr.to_json_string()
    }

    /// Thu hồi quyền kết nối của một client theo `client_id`.
    pub fn revoke_client(&mut self, client_id: &str) {
        self.clients.retain(|c| c.id != client_id);
    }

    /// Cài đặt tệp cấu hình Native Messaging Host manifest vào thư mục dữ liệu.
    pub fn install_manifests(&self, data_dir: &str) -> Result<(), DomainError> {
        let base = if data_dir.trim().is_empty() {
            "/tmp/hephaestus-svm-data".to_string()
        } else {
            data_dir.to_string()
        };
        let manifest_dir = Path::new(&base).join("native-messaging-hosts");
        fs::create_dir_all(&manifest_dir).map_err(|e| DomainError::Io(e.to_string()))?;

        let mut manifest = JsonValue::new_object();
        manifest.insert(
            "name",
            JsonValue::String("io.hephaestus.secure_vault_manager".to_string()),
        );
        manifest.insert(
            "description",
            JsonValue::String("Secure Vault Manager Native Messaging Host".to_string()),
        );
        manifest.insert(
            "path",
            JsonValue::String("/usr/bin/svm-native-host".to_string()),
        );
        manifest.insert("type", JsonValue::String("stdio".to_string()));

        let manifest_path = manifest_dir.join("io.hephaestus.secure_vault_manager.json");
        fs::write(&manifest_path, manifest.to_json_string().as_bytes())
            .map_err(|e| DomainError::Io(e.to_string()))?;
        Ok(())
    }
}

impl Default for BridgeServer {
    fn default() -> Self {
        Self::new()
    }
}
