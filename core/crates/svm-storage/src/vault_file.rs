//! Bộ quản lý đọc/ghi tệp mã hóa `vault.svm`, `biometric.env`, `settings.json` và tệp đính kèm.

use std::fs::{self, File};
use std::io::{BufReader, BufWriter, Cursor, Read, Write};
use std::path::{Path, PathBuf};

const ATTACHMENT_STREAM_MAGIC: &[u8; 4] = b"SVMA";
const ATTACHMENT_CHUNK_SIZE: usize = 65_536;

fn build_chunk_aad(chunk_idx: u64) -> Vec<u8> {
    let mut aad = Vec::with_capacity(AAD_ATTACHMENT.len() + 8);
    aad.extend_from_slice(AAD_ATTACHMENT);
    aad.extend_from_slice(&chunk_idx.to_le_bytes());
    aad
}

use svm_crypto::{
    chacha20_poly1305_decrypt, chacha20_poly1305_encrypt, derive_kek, hex_decode, hex_encode,
    normalize_recovery_key, random_bytes, sha256, zeroize_slice, zeroize_string,
};
use svm_domain::{DomainError, JsonValue};
use svm_system::{keystore_delete_secret, keystore_get_secret, keystore_set_secret};

use crate::atomic::{atomic_write_file, ensure_secure_dir, resolve_default_data_dir};
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
        ensure_secure_dir(base)?;
        ensure_secure_dir(&base.join("attachments"))?;
        Ok(())
    }

    fn biometric_keystore_account(&self) -> String {
        let digest = hex_encode(&sha256(self.base_path.as_bytes()));
        format!("biometric-wrapping-key::{}", &digest[..16])
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

    /// Cập nhật lại khóa bọc Recovery Key trên `VaultHeader` hiện có (giữ nguyên DEK và Master Password).
    pub fn rewrap_recovery_key(header: &mut VaultHeader, dek: &[u8; 32], new_recovery_key: &str) {
        header.rewrap_recovery_key(dek, new_recovery_key);
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
        let mut dek_vec = chacha20_poly1305_decrypt(
            &master_kek,
            &header.master_nonce,
            AAD_MASTER_DEK,
            &header.master_wrapped_dek,
        )
        .map_err(|_| DomainError::WrongPassword)?;
        zeroize_slice(&mut master_kek);

        let mut dek = [0u8; 32];
        dek.copy_from_slice(&dek_vec[..32]);
        zeroize_slice(&mut dek_vec);

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
        let norm_rec = normalize_recovery_key(recovery_key);
        let mut recovery_kek = derive_kek(&norm_rec, &header.recovery_salt);
        let dek_res = chacha20_poly1305_decrypt(
            &recovery_kek,
            &header.recovery_nonce,
            AAD_RECOVERY_DEK,
            &header.recovery_wrapped_dek,
        );
        zeroize_slice(&mut recovery_kek);

        let mut dek_vec = match dek_res {
            Ok(v) => v,
            Err(_) => {
                let raw_norm = recovery_key.trim().to_uppercase();
                if raw_norm != norm_rec {
                    let mut fallback_kek = derive_kek(&raw_norm, &header.recovery_salt);
                    let res = chacha20_poly1305_decrypt(
                        &fallback_kek,
                        &header.recovery_nonce,
                        AAD_RECOVERY_DEK,
                        &header.recovery_wrapped_dek,
                    );
                    zeroize_slice(&mut fallback_kek);
                    res.map_err(|_| DomainError::WrongPassword)?
                } else {
                    return Err(DomainError::WrongPassword);
                }
            }
        };

        let mut dek = [0u8; 32];
        dek.copy_from_slice(&dek_vec[..32]);
        zeroize_slice(&mut dek_vec);

        let plain_bytes =
            chacha20_poly1305_decrypt(&dek, &payload_nonce, AAD_PAYLOAD, &encrypted_payload)?;
        let json_str =
            String::from_utf8(plain_bytes).map_err(|e| DomainError::Parse(e.to_string()))?;
        Ok((header, dek, json_str))
    }

    /// Lưu khóa bọc sinh trắc học vào `biometric.env` kết hợp khóa ngẫu nhiên lưu trong OS Keystore.
    pub fn save_biometric_envelope(&self, dek: &[u8; 32]) -> Result<(), DomainError> {
        self.ensure_dirs()?;
        let mut bio_key = [0u8; 32];
        random_bytes(&mut bio_key);
        let mut bio_key_hex = hex_encode(&bio_key);
        let ks_res = keystore_set_secret(&self.biometric_keystore_account(), &bio_key_hex);
        zeroize_string(&mut bio_key_hex);
        if let Err(e) = ks_res {
            zeroize_slice(&mut bio_key);
            return Err(e);
        }

        let mut nonce = [0u8; 12];
        random_bytes(&mut nonce);
        let wrapped = chacha20_poly1305_encrypt(&bio_key, &nonce, AAD_BIOMETRIC, dek);
        zeroize_slice(&mut bio_key);
        let mut out = Vec::with_capacity(4 + 12 + wrapped.len());
        out.extend_from_slice(b"SVMB");
        out.extend_from_slice(&nonce);
        out.extend_from_slice(&wrapped);
        Self::atomic_write(&self.biometric_file_path(), &out)
    }

    /// Xóa tệp khóa bọc sinh trắc học `biometric.env` và bí mật trong OS Keystore.
    pub fn remove_biometric_envelope(&self) -> Result<(), DomainError> {
        let p = self.biometric_file_path();
        if p.exists() {
            fs::remove_file(p).map_err(|e| DomainError::Io(e.to_string()))?;
        }
        let _ = keystore_delete_secret(&self.biometric_keystore_account());
        Ok(())
    }

    /// Mở khóa bằng tệp khóa bọc sinh trắc học `biometric.env` và khóa bọc từ OS Keystore.
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

        let mut bio_key_hex = keystore_get_secret(&self.biometric_keystore_account())?;
        let mut key_bytes = hex_decode(&bio_key_hex)
            .map_err(|_| DomainError::Crypto("Invalid biometric key in OS Keystore".to_string()))?;
        zeroize_string(&mut bio_key_hex);
        if key_bytes.len() != 32 {
            zeroize_slice(&mut key_bytes);
            return Err(DomainError::Crypto(
                "Invalid biometric key length in OS Keystore".to_string(),
            ));
        }
        let mut bio_key = [0u8; 32];
        bio_key.copy_from_slice(&key_bytes);
        zeroize_slice(&mut key_bytes);

        let dek_res = chacha20_poly1305_decrypt(&bio_key, &nonce, AAD_BIOMETRIC, &bio_bytes[16..]);
        zeroize_slice(&mut bio_key);
        let mut dek_vec = dek_res?;

        let mut dek = [0u8; 32];
        dek.copy_from_slice(&dek_vec[..32]);
        zeroize_slice(&mut dek_vec);

        let (header, payload_nonce, encrypted_payload) = self.read_raw_envelope()?;
        let plain_bytes =
            chacha20_poly1305_decrypt(&dek, &payload_nonce, AAD_PAYLOAD, &encrypted_payload)?;
        let json_str =
            String::from_utf8(plain_bytes).map_err(|e| DomainError::Parse(e.to_string()))?;
        Ok((header, dek, json_str))
    }

    /// Mã hóa streaming theo từng chunk 64KB từ luồng đọc nguồn xuống tệp đính kèm.
    pub fn store_attachment_stream<R: Read>(
        &self,
        dek: &[u8; 32],
        attachment_id: &str,
        mut reader: R,
    ) -> Result<u64, DomainError> {
        self.ensure_dirs()?;
        let final_path = Path::new(&self.base_path)
            .join("attachments")
            .join(format!("{attachment_id}.bin"));
        let tmp_path = Path::new(&self.base_path)
            .join("attachments")
            .join(format!("{attachment_id}.bin.tmp"));

        let out_file = File::create(&tmp_path).map_err(|e| DomainError::Io(e.to_string()))?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let _ = fs::set_permissions(&tmp_path, fs::Permissions::from_mode(0o600));
        }

        let write_res: Result<u64, DomainError> = (|| {
            let mut writer = BufWriter::new(out_file);
            writer
                .write_all(ATTACHMENT_STREAM_MAGIC)
                .map_err(|e| DomainError::Io(e.to_string()))?;
            writer
                .write_all(&(ATTACHMENT_CHUNK_SIZE as u32).to_le_bytes())
                .map_err(|e| DomainError::Io(e.to_string()))?;

            let mut buf = vec![0u8; ATTACHMENT_CHUNK_SIZE];
            let mut total_bytes: u64 = 0;
            let mut chunk_idx: u64 = 0;

            loop {
                let mut filled = 0usize;
                while filled < ATTACHMENT_CHUNK_SIZE {
                    let n = reader
                        .read(&mut buf[filled..])
                        .map_err(|e| DomainError::Io(e.to_string()))?;
                    if n == 0 {
                        break;
                    }
                    filled += n;
                }
                if filled == 0 {
                    break;
                }

                total_bytes += filled as u64;
                let mut nonce = [0u8; 12];
                random_bytes(&mut nonce);
                let aad = build_chunk_aad(chunk_idx);
                let ct = chacha20_poly1305_encrypt(dek, &nonce, &aad, &buf[..filled]);
                zeroize_slice(&mut buf[..filled]);

                writer
                    .write_all(&(ct.len() as u32).to_le_bytes())
                    .map_err(|e| DomainError::Io(e.to_string()))?;
                writer
                    .write_all(&nonce)
                    .map_err(|e| DomainError::Io(e.to_string()))?;
                writer
                    .write_all(&ct)
                    .map_err(|e| DomainError::Io(e.to_string()))?;
                chunk_idx += 1;
            }
            zeroize_slice(&mut buf);

            // Sentinel 0u32 đánh dấu kết thúc luồng chunk toàn vẹn
            writer
                .write_all(&0u32.to_le_bytes())
                .map_err(|e| DomainError::Io(e.to_string()))?;
            writer.flush().map_err(|e| DomainError::Io(e.to_string()))?;
            Ok(total_bytes)
        })();

        match write_res {
            Ok(total) => {
                fs::rename(&tmp_path, &final_path).map_err(|e| DomainError::Io(e.to_string()))?;
                Ok(total)
            }
            Err(e) => {
                let _ = fs::remove_file(&tmp_path);
                Err(e)
            }
        }
    }

    /// Mã hóa streaming trực tiếp một tệp trên đĩa thành tệp đính kèm.
    pub fn store_attachment_file(
        &self,
        dek: &[u8; 32],
        attachment_id: &str,
        src_path: &Path,
    ) -> Result<u64, DomainError> {
        let file = File::open(src_path).map_err(|_| {
            DomainError::NotFound(format!("Source file not found: {}", src_path.display()))
        })?;
        self.store_attachment_stream(dek, attachment_id, BufReader::new(file))
    }

    /// Mã hóa và lưu tệp đính kèm từ buffer bytes.
    pub fn store_attachment(
        &self,
        dek: &[u8; 32],
        attachment_id: &str,
        raw_bytes: &[u8],
    ) -> Result<(), DomainError> {
        self.store_attachment_stream(dek, attachment_id, Cursor::new(raw_bytes))?;
        Ok(())
    }

    /// Giải mã streaming tệp đính kèm ra đối tượng `Write` (hỗ trợ cả chunk 64KB lẫn định dạng cũ).
    pub fn load_attachment_to_writer<W: Write>(
        &self,
        dek: &[u8; 32],
        attachment_id: &str,
        mut writer: W,
    ) -> Result<(), DomainError> {
        let path = Path::new(&self.base_path)
            .join("attachments")
            .join(format!("{attachment_id}.bin"));
        let mut file = File::open(&path)
            .map_err(|_| DomainError::NotFound(format!("Attachment {attachment_id} not found")))?;

        let mut magic = [0u8; 4];
        if file
            .read_exact(&mut magic)
            .map_err(|_| DomainError::Crypto("Corrupted attachment file".to_string()))
            .is_ok()
            && &magic == ATTACHMENT_STREAM_MAGIC
        {
            let mut reader = BufReader::new(file);
            let mut chunk_sz_bytes = [0u8; 4];
            reader
                .read_exact(&mut chunk_sz_bytes)
                .map_err(|_| DomainError::Crypto("Corrupted attachment header".to_string()))?;
            let max_chunk_sz = u32::from_le_bytes(chunk_sz_bytes) as usize;
            if max_chunk_sz == 0 || max_chunk_sz > 16 * 1024 * 1024 {
                return Err(DomainError::Crypto(
                    "Invalid attachment chunk size".to_string(),
                ));
            }

            let mut chunk_idx: u64 = 0;
            loop {
                let mut len_buf = [0u8; 4];
                reader
                    .read_exact(&mut len_buf)
                    .map_err(|_| DomainError::Crypto("Truncated attachment stream".to_string()))?;
                let ct_len = u32::from_le_bytes(len_buf) as usize;
                if ct_len == 0 {
                    break;
                }
                if ct_len < 16 || ct_len > max_chunk_sz + 16 {
                    return Err(DomainError::Crypto(
                        "Corrupted attachment chunk length".to_string(),
                    ));
                }

                let mut nonce = [0u8; 12];
                reader
                    .read_exact(&mut nonce)
                    .map_err(|_| DomainError::Crypto("Truncated attachment nonce".to_string()))?;
                let mut ct = vec![0u8; ct_len];
                reader
                    .read_exact(&mut ct)
                    .map_err(|_| DomainError::Crypto("Truncated attachment chunk".to_string()))?;

                let aad = build_chunk_aad(chunk_idx);
                let mut plain = chacha20_poly1305_decrypt(dek, &nonce, &aad, &ct)?;
                writer
                    .write_all(&plain)
                    .map_err(|e| DomainError::Io(e.to_string()))?;
                zeroize_slice(&mut plain);
                chunk_idx += 1;
            }
            writer.flush().map_err(|e| DomainError::Io(e.to_string()))?;
            return Ok(());
        }

        // Tương thích ngược định dạng single-blob cũ
        drop(file);
        let data = fs::read(&path)
            .map_err(|_| DomainError::NotFound(format!("Attachment {attachment_id} not found")))?;
        if data.len() < 12 + 16 {
            return Err(DomainError::Crypto("Corrupted attachment file".to_string()));
        }
        let mut nonce = [0u8; 12];
        nonce.copy_from_slice(&data[0..12]);
        let mut plain = chacha20_poly1305_decrypt(dek, &nonce, AAD_ATTACHMENT, &data[12..])?;
        writer
            .write_all(&plain)
            .map_err(|e| DomainError::Io(e.to_string()))?;
        zeroize_slice(&mut plain);
        writer.flush().map_err(|e| DomainError::Io(e.to_string()))
    }

    /// Giải mã streaming tệp đính kèm trực tiếp ra tệp đích trên đĩa.
    pub fn export_attachment_file(
        &self,
        dek: &[u8; 32],
        attachment_id: &str,
        dst_path: &Path,
    ) -> Result<(), DomainError> {
        let out_file = File::create(dst_path).map_err(|e| DomainError::Io(e.to_string()))?;
        self.load_attachment_to_writer(dek, attachment_id, BufWriter::new(out_file))
    }

    /// Đọc và giải mã tệp đính kèm vào RAM (cho tệp nhỏ).
    pub fn load_attachment(
        &self,
        dek: &[u8; 32],
        attachment_id: &str,
    ) -> Result<Vec<u8>, DomainError> {
        let mut out = Vec::new();
        self.load_attachment_to_writer(dek, attachment_id, &mut out)?;
        Ok(out)
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
        def.insert("biometric_enabled", JsonValue::Bool(false));
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
