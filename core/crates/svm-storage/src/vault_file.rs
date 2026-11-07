//! Bộ quản lý đọc/ghi tệp mã hóa `vault.svm`, `biometric.env`, `settings.json` và tệp đính kèm.

use std::fs;
use std::path::{Path, PathBuf};

use svm_crypto::{
    chacha20_poly1305_decrypt, chacha20_poly1305_encrypt, derive_kek, random_bytes, sha256,
    zeroize_slice,
};
use svm_domain::{DomainError, JsonValue};

use crate::atomic::{atomic_write_file, resolve_default_data_dir};
use crate::header::{
    VaultHeader, AAD_ATTACHMENT, AAD_BIOMETRIC, AAD_MASTER_DEK, AAD_PAYLOAD, AAD_RECOVERY_DEK,
    HEADER_LEN, VAULT_MAGIC, VAULT_VERSION,
};

/// Bộ quản lý hệ thống tệp lưu trữ của kho dữ liệu.
#[derive(Debug, Clone)]
pub struct VaultStorage {
    /// Đường dẫn thư mục gốc chứa các tệp kho dữ liệu.
    pub base_path: String,
}

impl VaultStorage {
    /// Khởi tạo một đối tượng lưu trữ mới với đường dẫn chỉ định.
    ///
    /// # Arguments
    /// * `base_path` - Đường dẫn thư mục dữ liệu trên hệ điều hành.
    pub fn new(base_path: impl Into<String>) -> Self {
        let raw = base_path.into();
        let resolved = if raw.trim().is_empty() {
            Self::default_data_dir()
        } else {
            raw
        };
        Self {
            base_path: resolved,
        }
    }

    /// Xác định thư mục dữ liệu mặc định của người dùng trên hệ điều hành.
    pub fn default_data_dir() -> String {
        resolve_default_data_dir()
    }

    fn ensure_dirs(&self) -> Result<(), DomainError> {
        let base = Path::new(&self.base_path);
        fs::create_dir_all(base).map_err(|e| DomainError::Io(e.to_string()))?;
        fs::create_dir_all(base.join("attachments")).map_err(|e| DomainError::Io(e.to_string()))?;
        Ok(())
    }

    /// Đường dẫn tới tệp nhị phân kho mã hóa `vault.svm`.
    pub fn vault_file_path(&self) -> PathBuf {
        Path::new(&self.base_path).join("vault.svm")
    }

    /// Đường dẫn tới tệp sinh trắc học `biometric.env`.
    pub fn biometric_file_path(&self) -> PathBuf {
        Path::new(&self.base_path).join("biometric.env")
    }

    /// Đường dẫn tới tệp cấu hình `settings.json`.
    pub fn settings_file_path(&self) -> PathBuf {
        Path::new(&self.base_path).join("settings.json")
    }

    /// Đường dẫn tới tệp danh sách client trình duyệt `bridge_clients.json`.
    pub fn clients_file_path(&self) -> PathBuf {
        Path::new(&self.base_path).join("bridge_clients.json")
    }

    /// Kiểm tra xem tệp kho `vault.svm` đã tồn tại trên đĩa hay chưa.
    pub fn vault_exists(&self) -> bool {
        self.vault_file_path().exists()
    }

    /// Ghi nguyên tử (Atomic Write) dữ liệu ra tệp thông qua tệp tạm `.tmp` rồi đổi tên.
    pub fn atomic_write(path: &Path, bytes: &[u8]) -> Result<(), DomainError> {
        atomic_write_file(path, bytes)
    }

    /// Tạo mới hoàn toàn cấu trúc bọc khóa `VaultHeader` cho DEK từ Master Password và Recovery Key.
    pub fn create_header(dek: &[u8; 32], master_password: &str, recovery_key: &str) -> VaultHeader {
        VaultHeader::create(dek, master_password, recovery_key)
    }

    /// Cập nhật lại khóa bọc Master Password trên `VaultHeader` hiện có (giữ nguyên DEK và Recovery Key).
    pub fn rewrap_master_password(header: &mut VaultHeader, dek: &[u8; 32], new_password: &str) {
        header.rewrap_master_password(dek, new_password);
    }

    /// Mã hóa payload JSON bằng DEK và lưu nguyên tử xuống tệp `vault.svm`.
    pub fn save_vault_payload(
        &self,
        header: &VaultHeader,
        dek: &[u8; 32],
        payload_json: &str,
    ) -> Result<(), DomainError> {
        self.ensure_dirs()?;
        let mut payload_nonce = [0u8; 12];
        random_bytes(&mut payload_nonce);

        let encrypted_payload =
            chacha20_poly1305_encrypt(dek, &payload_nonce, AAD_PAYLOAD, payload_json.as_bytes());

        let mut file_bytes = Vec::with_capacity(HEADER_LEN + encrypted_payload.len());
        file_bytes.extend_from_slice(VAULT_MAGIC);
        file_bytes.push(VAULT_VERSION);
        file_bytes.extend_from_slice(&header.master_salt);
        file_bytes.extend_from_slice(&header.master_nonce);
        file_bytes.extend_from_slice(&header.master_wrapped_dek);
        file_bytes.extend_from_slice(&header.recovery_salt);
        file_bytes.extend_from_slice(&header.recovery_nonce);
        file_bytes.extend_from_slice(&header.recovery_wrapped_dek);
        file_bytes.extend_from_slice(&payload_nonce);
        file_bytes.extend_from_slice(&encrypted_payload);

        Self::atomic_write(&self.vault_file_path(), &file_bytes)
    }

    /// Đọc tệp `vault.svm` và tách `VaultHeader`, `payload_nonce`, cùng `encrypted_payload`.
    fn read_raw_envelope(&self) -> Result<(VaultHeader, [u8; 12], Vec<u8>), DomainError> {
        let path = self.vault_file_path();
        let bytes = fs::read(&path).map_err(|e| DomainError::Io(e.to_string()))?;
        if bytes.len() < HEADER_LEN + 16 {
            return Err(DomainError::Parse(
                "Corrupted vault.svm file (too short)".to_string(),
            ));
        }
        if &bytes[0..4] != VAULT_MAGIC || bytes[4] != VAULT_VERSION {
            return Err(DomainError::Parse(
                "Invalid vault.svm magic or version".to_string(),
            ));
        }

        let mut master_salt = [0u8; 16];
        master_salt.copy_from_slice(&bytes[5..21]);
        let mut master_nonce = [0u8; 12];
        master_nonce.copy_from_slice(&bytes[21..33]);
        let mut master_wrapped_dek = [0u8; 48];
        master_wrapped_dek.copy_from_slice(&bytes[33..81]);

        let mut recovery_salt = [0u8; 16];
        recovery_salt.copy_from_slice(&bytes[81..97]);
        let mut recovery_nonce = [0u8; 12];
        recovery_nonce.copy_from_slice(&bytes[97..109]);
        let mut recovery_wrapped_dek = [0u8; 48];
        recovery_wrapped_dek.copy_from_slice(&bytes[109..157]);

        let mut payload_nonce = [0u8; 12];
        payload_nonce.copy_from_slice(&bytes[157..169]);

        let encrypted_payload = bytes[169..].to_vec();

        Ok((
            VaultHeader {
                master_salt,
                master_nonce,
                master_wrapped_dek,
                recovery_salt,
                recovery_nonce,
                recovery_wrapped_dek,
            },
            payload_nonce,
            encrypted_payload,
        ))
    }

    /// Mở khóa và giải mã tệp `vault.svm` bằng Master Password.
    pub fn unlock_with_password(
        &self,
        password: &str,
    ) -> Result<(VaultHeader, [u8; 32], String), DomainError> {
        let (header, payload_nonce, encrypted_payload) = self.read_raw_envelope()?;
        let mut master_kek = derive_kek(password, &header.master_salt);
        let dek_vec = chacha20_poly1305_decrypt(
            &master_kek,
            &header.master_nonce,
            AAD_MASTER_DEK,
            &header.master_wrapped_dek,
        )
        .map_err(|_| DomainError::WrongPassword)?;
        zeroize_slice(&mut master_kek);

        let mut dek = [0u8; 32];
        dek.copy_from_slice(&dek_vec[..32]);

        let plain_bytes =
            chacha20_poly1305_decrypt(&dek, &payload_nonce, AAD_PAYLOAD, &encrypted_payload)?;
        let json_str =
            String::from_utf8(plain_bytes).map_err(|e| DomainError::Parse(e.to_string()))?;
        Ok((header, dek, json_str))
    }

    /// Mở khóa và giải mã tệp `vault.svm` bằng Emergency Recovery Key.
    pub fn unlock_with_recovery(
        &self,
        recovery_key: &str,
    ) -> Result<(VaultHeader, [u8; 32], String), DomainError> {
        let (header, payload_nonce, encrypted_payload) = self.read_raw_envelope()?;
        let norm_rec = recovery_key.trim().to_uppercase();
        let mut recovery_kek = derive_kek(&norm_rec, &header.recovery_salt);
        let dek_vec = chacha20_poly1305_decrypt(
            &recovery_kek,
            &header.recovery_nonce,
            AAD_RECOVERY_DEK,
            &header.recovery_wrapped_dek,
        )
        .map_err(|_| DomainError::WrongPassword)?;
        zeroize_slice(&mut recovery_kek);

        let mut dek = [0u8; 32];
        dek.copy_from_slice(&dek_vec[..32]);

        let plain_bytes =
            chacha20_poly1305_decrypt(&dek, &payload_nonce, AAD_PAYLOAD, &encrypted_payload)?;
        let json_str =
            String::from_utf8(plain_bytes).map_err(|e| DomainError::Parse(e.to_string()))?;
        Ok((header, dek, json_str))
    }

    /// Lưu khóa bọc sinh trắc học vào `biometric.env`.
    pub fn save_biometric_envelope(&self, dek: &[u8; 32]) -> Result<(), DomainError> {
        self.ensure_dirs()?;
        let bio_key = sha256(format!("svm-bio-keystore::{}", self.base_path).as_bytes());
        let mut nonce = [0u8; 12];
        random_bytes(&mut nonce);
        let wrapped = chacha20_poly1305_encrypt(&bio_key, &nonce, AAD_BIOMETRIC, dek);
        let mut out = Vec::with_capacity(4 + 12 + wrapped.len());
        out.extend_from_slice(b"SVMB");
        out.extend_from_slice(&nonce);
        out.extend_from_slice(&wrapped);
        Self::atomic_write(&self.biometric_file_path(), &out)
    }

    /// Xóa tệp khóa bọc sinh trắc học `biometric.env`.
    pub fn remove_biometric_envelope(&self) -> Result<(), DomainError> {
        let p = self.biometric_file_path();
        if p.exists() {
            fs::remove_file(p).map_err(|e| DomainError::Io(e.to_string()))?;
        }
        Ok(())
    }

    /// Mở khóa bằng tệp khóa bọc sinh trắc học `biometric.env`.
    pub fn unlock_with_biometric(&self) -> Result<(VaultHeader, [u8; 32], String), DomainError> {
        let bio_path = self.biometric_file_path();
        let bio_bytes = fs::read(&bio_path)
            .map_err(|_| DomainError::NotFound("Biometric unlock not enabled".to_string()))?;
        if bio_bytes.len() < 4 + 12 + 48 || &bio_bytes[0..4] != b"SVMB" {
            return Err(DomainError::Crypto(
                "Invalid biometric envelope".to_string(),
            ));
        }
        let mut nonce = [0u8; 12];
        nonce.copy_from_slice(&bio_bytes[4..16]);
        let bio_key = sha256(format!("svm-bio-keystore::{}", self.base_path).as_bytes());
        let dek_vec = chacha20_poly1305_decrypt(&bio_key, &nonce, AAD_BIOMETRIC, &bio_bytes[16..])?;

        let mut dek = [0u8; 32];
        dek.copy_from_slice(&dek_vec[..32]);

        let (header, payload_nonce, encrypted_payload) = self.read_raw_envelope()?;
        let plain_bytes =
            chacha20_poly1305_decrypt(&dek, &payload_nonce, AAD_PAYLOAD, &encrypted_payload)?;
        let json_str =
            String::from_utf8(plain_bytes).map_err(|e| DomainError::Parse(e.to_string()))?;
        Ok((header, dek, json_str))
    }

    /// Mã hóa và lưu tệp đính kèm.
    pub fn store_attachment(
        &self,
        dek: &[u8; 32],
        attachment_id: &str,
        raw_bytes: &[u8],
    ) -> Result<(), DomainError> {
        self.ensure_dirs()?;
        let mut nonce = [0u8; 12];
        random_bytes(&mut nonce);
        let ct = chacha20_poly1305_encrypt(dek, &nonce, AAD_ATTACHMENT, raw_bytes);
        let mut data = Vec::with_capacity(12 + ct.len());
        data.extend_from_slice(&nonce);
        data.extend_from_slice(&ct);
        let path = Path::new(&self.base_path)
            .join("attachments")
            .join(format!("{attachment_id}.bin"));
        Self::atomic_write(&path, &data)
    }

    /// Đọc và giải mã tệp đính kèm.
    pub fn load_attachment(
        &self,
        dek: &[u8; 32],
        attachment_id: &str,
    ) -> Result<Vec<u8>, DomainError> {
        let path = Path::new(&self.base_path)
            .join("attachments")
            .join(format!("{attachment_id}.bin"));
        let data = fs::read(&path)
            .map_err(|_| DomainError::NotFound(format!("Attachment {attachment_id} not found")))?;
        if data.len() < 12 + 16 {
            return Err(DomainError::Crypto("Corrupted attachment file".to_string()));
        }
        let mut nonce = [0u8; 12];
        nonce.copy_from_slice(&data[0..12]);
        chacha20_poly1305_decrypt(dek, &nonce, AAD_ATTACHMENT, &data[12..])
    }

    /// Xóa tệp đính kèm trên đĩa.
    pub fn delete_attachment_file(&self, attachment_id: &str) -> Result<(), DomainError> {
        let path = Path::new(&self.base_path)
            .join("attachments")
            .join(format!("{attachment_id}.bin"));
        if path.exists() {
            fs::remove_file(&path).map_err(|e| DomainError::Io(e.to_string()))?;
        }
        Ok(())
    }

    /// Đọc cấu hình `settings.json` từ đĩa (hoặc trả về mặc định nếu chưa có).
    pub fn load_settings(&self) -> String {
        let path = self.settings_file_path();
        if let Ok(content) = fs::read_to_string(&path) {
            if JsonValue::parse(&content).is_ok() {
                return content;
            }
        }
        let mut def = JsonValue::new_object();
        def.insert("auto_lock_minutes", JsonValue::Number(15.0));
        def.insert("theme", JsonValue::String("dark".to_string()));
        def.insert("language", JsonValue::String("en".to_string()));
        def.insert("clipboard_clear_seconds", JsonValue::Number(30.0));
        def.insert("clear_clipboard_on_lock", JsonValue::Bool(true));
        def.insert("lock_on_sleep", JsonValue::Bool(true));
        def.insert("biometric_enabled", JsonValue::Bool(true));
        def.insert("browser_bridge_enabled", JsonValue::Bool(true));
        def.insert("ssh_agent_enabled", JsonValue::Bool(false));
        def.insert("ssh_prompt_approval", JsonValue::Bool(true));
        def.insert("auto_empty_trash", JsonValue::Bool(true));
        def.to_json_string()
    }

    /// Lưu cấu hình `settings.json` xuống đĩa.
    pub fn save_settings(&self, json: &str) -> Result<(), DomainError> {
        let _ = JsonValue::parse(json).map_err(DomainError::Parse)?;
        self.ensure_dirs()?;
        Self::atomic_write(&self.settings_file_path(), json.as_bytes())
    }
}
