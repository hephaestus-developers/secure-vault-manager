//! Máy chủ cầu nối trình duyệt (`BridgeServer`): điều phối Pairing PIN 6 số, quản lý client,
//! lắng nghe kết nối IPC socket cục bộ và cài đặt Native Messaging Host manifest.

use std::fs;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

#[cfg(unix)]
use std::os::unix::net::{UnixListener, UnixStream};

use svm_crypto::{generate_uuid_v4, random_bytes};
use svm_domain::{DomainError, JsonValue};
use svm_ipc::IpcEndpoint;

use crate::client::PairedClient;

/// Xác định đường dẫn Unix Domain Socket của Bridge Server.
pub fn resolve_socket_path() -> PathBuf {
    if let Ok(runtime_dir) = std::env::var("XDG_RUNTIME_DIR") {
        let dir = PathBuf::from(runtime_dir).join("svm");
        if fs::create_dir_all(&dir).is_ok() {
            return dir.join("bridge.sock");
        }
    }
    if let Ok(home) = std::env::var("HOME") {
        let dir = PathBuf::from(home).join(".svm");
        if fs::create_dir_all(&dir).is_ok() {
            return dir.join("bridge.sock");
        }
    }
    let dir = PathBuf::from("/tmp/hephaestus-svm-data");
    let _ = fs::create_dir_all(&dir);
    dir.join("bridge.sock")
}

/// Dò tìm đường dẫn binary thực thi của `svm-native-host`.
pub fn detect_native_host_binary() -> PathBuf {
    // 1. Kiểm tra cùng thư mục với tiến trình hiện hành
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            let candidate = parent.join("svm-native-host");
            if candidate.exists() {
                return candidate;
            }
            let candidate_core = parent.join("core").join("svm-native-host");
            if candidate_core.exists() {
                return candidate_core;
            }
        }
    }
    // 2. Kiểm tra thư mục build / target
    for rel_path in &[
        "core/target/debug/svm-native-host",
        "core/target/release/svm-native-host",
        "build/debug/core/svm-native-host",
    ] {
        let p = PathBuf::from(rel_path);
        if p.exists() {
            if let Ok(abs) = fs::canonicalize(&p) {
                return abs;
            }
            return p;
        }
    }
    // 3. ~/.local/bin
    if let Ok(home) = std::env::var("HOME") {
        let p = PathBuf::from(home).join(".local/bin/svm-native-host");
        if p.exists() {
            return p;
        }
    }
    // 4. /usr/local/bin hoặc /usr/bin
    let p_usr_local = PathBuf::from("/usr/local/bin/svm-native-host");
    if p_usr_local.exists() {
        return p_usr_local;
    }
    PathBuf::from("/usr/bin/svm-native-host")
}

/// Máy chủ cầu nối tiếp nhận và điều phối các yêu cầu từ Browser Extension.
pub struct BridgeServer {
    listening: bool,
    data_dir: String,
    clients: Vec<PairedClient>,
    pairing_pin: Option<(String, Instant)>,
    shutdown_signal: Arc<AtomicBool>,
    listener_thread: Option<std::thread::JoinHandle<()>>,
}

impl BridgeServer {
    /// Khởi tạo một `BridgeServer` mới gắn với thư mục dữ liệu.
    pub fn new(data_dir: impl Into<String>) -> Self {
        let dir = data_dir.into();
        let mut s = Self {
            listening: false,
            data_dir: dir,
            clients: Vec::new(),
            pairing_pin: None,
            shutdown_signal: Arc::new(AtomicBool::new(false)),
            listener_thread: None,
        };
        s.load_clients();
        s
    }

    /// Khởi tạo `BridgeServer` với thư mục dữ liệu mặc định.
    pub fn with_default_dir() -> Self {
        Self::new("")
    }

    /// Kiểm tra xem `BridgeServer` có đang lắng nghe hay không.
    pub fn is_listening(&self) -> bool {
        self.listening
    }

    /// Khởi động socket listener và xử lý các yêu cầu qua `handler`.
    pub fn start<F>(&mut self, handler: F)
    where
        F: Fn(&str) -> String + Send + Sync + 'static,
    {
        if self.listening {
            return;
        }
        self.listening = true;
        self.shutdown_signal.store(false, Ordering::SeqCst);

        let shutdown = Arc::clone(&self.shutdown_signal);
        let handler: Arc<dyn Fn(&str) -> String + Send + Sync> = Arc::new(handler);

        #[cfg(unix)]
        {
            let candidates = {
                let mut c = Vec::new();
                if let Ok(runtime_dir) = std::env::var("XDG_RUNTIME_DIR") {
                    let dir = PathBuf::from(runtime_dir).join("svm");
                    if fs::create_dir_all(&dir).is_ok() {
                        c.push(dir.join("bridge.sock"));
                    }
                }
                if let Ok(home) = std::env::var("HOME") {
                    let dir = PathBuf::from(home).join(".svm");
                    if fs::create_dir_all(&dir).is_ok() {
                        c.push(dir.join("bridge.sock"));
                    }
                }
                let dir = PathBuf::from("/tmp/hephaestus-svm-data");
                let _ = fs::create_dir_all(&dir);
                c.push(dir.join("bridge.sock"));
                c
            };

            let mut bound_listener = None;
            let mut bound_path = None;

            for socket_path in candidates {
                if socket_path.exists() {
                    let _ = fs::remove_file(&socket_path);
                }
                if let Ok(listener) = UnixListener::bind(&socket_path) {
                    let _ = listener.set_nonblocking(true);
                    bound_listener = Some(listener);
                    bound_path = Some(socket_path);
                    break;
                }
            }

            if let (Some(listener), Some(socket_path)) = (bound_listener, bound_path) {
                let handle = std::thread::spawn(move || {
                    while !shutdown.load(Ordering::SeqCst) {
                        match listener.accept() {
                            Ok((mut stream, _)) => {
                                let _ = stream.set_nonblocking(false);
                                let h = Arc::clone(&handler);
                                std::thread::spawn(move || {
                                    handle_client_stream(&mut stream, &h);
                                });
                            }
                            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                                std::thread::sleep(Duration::from_millis(30));
                            }
                            Err(_) => {
                                break;
                            }
                        }
                    }
                    let _ = fs::remove_file(&socket_path);
                });
                self.listener_thread = Some(handle);
            }
        }
    }

    /// Dừng lắng nghe socket và đóng các kết nối.
    pub fn stop(&mut self) {
        if !self.listening {
            return;
        }
        self.listening = false;
        self.shutdown_signal.store(true, Ordering::SeqCst);
        if let Some(handle) = self.listener_thread.take() {
            let _ = handle.join();
        }
        #[cfg(unix)]
        {
            let socket_path = resolve_socket_path();
            if socket_path.exists() {
                let _ = fs::remove_file(&socket_path);
            }
        }
    }

    /// Sinh mã PIN ghép nối 6 chữ số ngẫu nhiên có hiệu lực trong 60 giây.
    pub fn begin_pairing(&mut self) -> String {
        let mut rnd = [0u8; 4];
        random_bytes(&mut rnd);
        let num = u32::from_le_bytes(rnd) % 900_000 + 100_000;
        let code = format!("{num:06}");
        self.pairing_pin = Some((code.clone(), Instant::now()));

        let mut obj = JsonValue::new_object();
        obj.insert("pairing_code", JsonValue::String(code));
        obj.insert("expires_in_secs", JsonValue::Number(60.0));
        obj.to_json_string()
    }

    /// Lấy mã PIN ghép nối đang hoạt động (nếu có và còn hạn).
    pub fn current_pairing_pin(&self) -> Option<String> {
        if let Some((ref code, created_at)) = self.pairing_pin {
            if created_at.elapsed() <= Duration::from_secs(60) {
                return Some(code.clone());
            }
        }
        None
    }

    /// Xác nhận mã PIN ghép nối từ Extension và cấp phát `PairedClient`.
    pub fn confirm_pairing(
        &mut self,
        code: &str,
        browser: &str,
        version: &str,
    ) -> Result<PairedClient, DomainError> {
        let Some((ref valid_pin, created_at)) = self.pairing_pin else {
            return Err(DomainError::InvalidArg(
                "Không có mã PIN ghép nối nào đang chờ".to_string(),
            ));
        };
        if created_at.elapsed() > Duration::from_secs(60) {
            self.pairing_pin = None;
            return Err(DomainError::InvalidArg(
                "Mã PIN ghép nối đã hết hạn (quá 60 giây)".to_string(),
            ));
        }
        if valid_pin != code.trim() {
            return Err(DomainError::InvalidArg(
                "Mã PIN ghép nối không chính xác".to_string(),
            ));
        }
        self.pairing_pin = None;
        let client = PairedClient {
            id: format!("ext-{}", generate_uuid_v4()),
            browser: if browser.is_empty() {
                "WebExtension".to_string()
            } else {
                browser.to_string()
            },
            version: if version.is_empty() {
                "v1.0.0".to_string()
            } else {
                version.to_string()
            },
            last_used: "Vừa mới xong".to_string(),
            session_token: format!("svm-sess-{}", generate_uuid_v4()),
        };
        self.clients.push(client.clone());
        self.save_clients();
        Ok(client)
    }

    /// Đường dẫn tệp lưu trữ danh sách client `bridge_clients.json`.
    fn clients_file_path(&self) -> PathBuf {
        let base = if self.data_dir.trim().is_empty() {
            "/tmp/hephaestus-svm-data".to_string()
        } else {
            self.data_dir.clone()
        };
        let _ = fs::create_dir_all(&base);
        PathBuf::from(base).join("bridge_clients.json")
    }

    /// Tải danh sách client đã lưu từ đĩa.
    pub fn load_clients(&mut self) {
        let path = self.clients_file_path();
        if !path.exists() {
            self.clients = Vec::new();
            return;
        }
        if let Ok(content) = fs::read_to_string(&path) {
            if let Ok(json) = JsonValue::parse(&content) {
                if let Some(arr) = json.as_array() {
                    let mut loaded = Vec::new();
                    for val in arr {
                        if let Some(client) = PairedClient::from_json(val) {
                            loaded.push(client);
                        }
                    }
                    self.clients = loaded;
                    return;
                }
            }
        }
        self.clients = Vec::new();
    }

    /// Ghi danh sách client hiện tại vào đĩa.
    pub fn save_clients(&self) {
        let path = self.clients_file_path();
        let mut arr = JsonValue::new_array();
        for c in &self.clients {
            arr.push(c.to_json());
        }
        let _ = fs::write(path, arr.to_json_string().as_bytes());
    }

    /// Trả về danh sách client dưới dạng chuỗi JSON `[]`.
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
        self.save_clients();
    }

    /// Kiểm tra xem đã có bất kỳ tiện ích mở rộng (client) nào được ghép nối hợp lệ hay chưa.
    pub fn has_paired_clients(&self) -> bool {
        !self.clients.is_empty()
    }

    /// Trả về tham chiếu danh sách clients đã ghép nối.
    pub fn clients(&self) -> &[PairedClient] {
        &self.clients
    }

    /// Xác thực xem một client có tồn tại và token phiên có hợp lệ hay không.
    pub fn is_client_authorized(&self, session_token: &str, client_id: &str) -> bool {
        if self.clients.is_empty() {
            return false;
        }
        if !session_token.is_empty() {
            return self
                .clients
                .iter()
                .any(|c| c.session_token == session_token);
        }
        if !client_id.is_empty() {
            return self.clients.iter().any(|c| c.id == client_id);
        }
        true
    }

    /// Tự động dò tìm các ID extension Secure Vault Manager đã được cài đặt vào trình duyệt.
    fn detect_chrome_extension_origins(&self, home: &str) -> Vec<String> {
        let mut ids = vec![
            "nhkjlnifnoicniecninomjjckhnbkmem".to_string(),
            "cblgnkccngfnmepkgkgljklmjgfkcfhd".to_string(),
            "dkmcefgkjjfkhdlnodfkblffchgajhgn".to_string(),
        ];

        let candidate_prefs = [
            format!("{home}/.config/google-chrome/Default/Preferences"),
            format!("{home}/.config/google-chrome/Profile 1/Preferences"),
            format!("{home}/.config/google-chrome/Profile 2/Preferences"),
            format!("{home}/.config/chromium/Default/Preferences"),
            format!("{home}/.config/BraveSoftware/Brave-Browser/Default/Preferences"),
            format!("{home}/.config/microsoft-edge/Default/Preferences"),
            format!("{home}/.var/app/com.google.Chrome/config/google-chrome/Default/Preferences"),
            format!("{home}/.var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser/Default/Preferences"),
            format!("{home}/snap/chromium/current/.config/chromium/Default/Preferences"),
        ];

        for pref_file in &candidate_prefs {
            if let Ok(content) = fs::read_to_string(pref_file) {
                if let Ok(json) = JsonValue::parse(&content) {
                    if let Some(ext_settings) =
                        json.get("extensions").and_then(|e| e.get("settings"))
                    {
                        if let Some(obj) = ext_settings.as_object() {
                            for (ext_id, val) in obj {
                                let path_str = val.get_str_or("path", "").to_lowercase();
                                let name_str = val
                                    .get("manifest")
                                    .and_then(|m| m.get("name"))
                                    .and_then(|n| n.as_str())
                                    .unwrap_or("")
                                    .to_lowercase();
                                if path_str.contains("secure-vault-manager")
                                    || path_str.contains("dist/chrome")
                                    || path_str.contains("dist/firefox")
                                    || name_str.contains("secure vault manager")
                                {
                                    if ext_id.len() == 32
                                        && ext_id.chars().all(|c| c.is_ascii_lowercase())
                                        && !ids.contains(ext_id)
                                    {
                                        ids.push(ext_id.clone());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ids.into_iter()
            .map(|id| format!("chrome-extension://{id}/"))
            .collect()
    }

    /// Cài đặt tệp cấu hình Native Messaging Host manifest vào thư mục cấu hình trình duyệt người dùng.
    pub fn install_manifests(&self, data_dir: &str) -> Result<Vec<String>, DomainError> {
        let host_bin = detect_native_host_binary();
        let host_bin_str = host_bin.to_string_lossy().to_string();

        let mut installed_paths = Vec::new();
        let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".to_string());

        let candidate_dirs = [
            // Google Chrome
            format!("{home}/.config/google-chrome/NativeMessagingHosts"),
            // Chromium
            format!("{home}/.config/chromium/NativeMessagingHosts"),
            // Brave Browser
            format!("{home}/.config/BraveSoftware/Brave-Browser/NativeMessagingHosts"),
            // Mozilla Firefox
            format!("{home}/.mozilla/native-messaging-hosts"),
            // Microsoft Edge
            format!("{home}/.config/microsoft-edge/NativeMessagingHosts"),
            // macOS Chrome
            format!("{home}/Library/Application Support/Google/Chrome/NativeMessagingHosts"),
            // macOS Firefox
            format!("{home}/Library/Application Support/Mozilla/NativeMessagingHosts"),
        ];

        let manifest_name = "io.hephaestus.secure_vault_manager.json";

        // Manifest cho các trình duyệt Chromium (Chrome, Brave, Edge, Chromium)
        let mut chrome_manifest = JsonValue::new_object();
        chrome_manifest.insert(
            "name",
            JsonValue::String("io.hephaestus.secure_vault_manager".to_string()),
        );
        chrome_manifest.insert(
            "description",
            JsonValue::String("Secure Vault Manager Native Messaging Host".to_string()),
        );
        chrome_manifest.insert("path", JsonValue::String(host_bin_str.clone()));
        chrome_manifest.insert("type", JsonValue::String("stdio".to_string()));
        let detected_origins = self.detect_chrome_extension_origins(&home);
        let mut origins = JsonValue::new_array();
        for orig in detected_origins {
            origins.push(JsonValue::String(orig));
        }
        chrome_manifest.insert("allowed_origins", origins);
        let chrome_json = chrome_manifest.to_json_string();

        // Manifest cho Mozilla Firefox
        let mut firefox_manifest = JsonValue::new_object();
        firefox_manifest.insert(
            "name",
            JsonValue::String("io.hephaestus.secure_vault_manager".to_string()),
        );
        firefox_manifest.insert(
            "description",
            JsonValue::String("Secure Vault Manager Native Messaging Host".to_string()),
        );
        firefox_manifest.insert("path", JsonValue::String(host_bin_str));
        firefox_manifest.insert("type", JsonValue::String("stdio".to_string()));
        let mut extensions = JsonValue::new_array();
        extensions.push(JsonValue::String(
            "extension@secure-vault.hephaestus.io".to_string(),
        ));
        extensions.push(JsonValue::String(
            "secure-vault-manager@hephaestus.io".to_string(),
        ));
        firefox_manifest.insert("allowed_extensions", extensions);
        let firefox_json = firefox_manifest.to_json_string();

        for dir_str in &candidate_dirs {
            let p = Path::new(dir_str);
            if let Some(parent) = p.parent() {
                if parent.exists() {
                    if fs::create_dir_all(p).is_ok() {
                        let manifest_file = p.join(manifest_name);
                        let content = if dir_str.contains("mozilla") {
                            &firefox_json
                        } else {
                            &chrome_json
                        };
                        if fs::write(&manifest_file, content.as_bytes()).is_ok() {
                            installed_paths.push(manifest_file.to_string_lossy().to_string());
                        }
                    }
                }
            }
        }

        // Luôn ghi thêm vào thư mục dữ liệu cục bộ làm dự phòng
        let fallback_base = if data_dir.trim().is_empty() {
            "/tmp/hephaestus-svm-data"
        } else {
            data_dir
        };
        let fallback_dir = Path::new(fallback_base).join("native-messaging-hosts");
        let _ = fs::create_dir_all(&fallback_dir);
        let fallback_file = fallback_dir.join(manifest_name);
        let _ = fs::write(&fallback_file, chrome_json.as_bytes());
        installed_paths.push(fallback_file.to_string_lossy().to_string());

        Ok(installed_paths)
    }
}

impl Default for BridgeServer {
    fn default() -> Self {
        Self::with_default_dir()
    }
}

#[cfg(unix)]
fn handle_client_stream(
    stream: &mut UnixStream,
    handler: &Arc<dyn Fn(&str) -> String + Send + Sync>,
) {
    loop {
        let mut len_buf = [0u8; 4];
        if stream.read_exact(&mut len_buf).is_err() {
            break;
        }
        let req_len = u32::from_le_bytes(len_buf) as usize;
        if req_len == 0 || req_len > 10 * 1024 * 1024 {
            break;
        }
        let mut payload = vec![0u8; req_len];
        if stream.read_exact(&mut payload).is_err() {
            break;
        }
        let req_str = match String::from_utf8(payload) {
            Ok(s) => s,
            Err(_) => break,
        };
        let resp_str = handler(&req_str);
        if let Ok(frame) = IpcEndpoint::encode_frame(resp_str.as_bytes()) {
            if stream.write_all(&frame).is_err() || stream.flush().is_err() {
                break;
            }
        } else {
            break;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_install_manifests() {
        let server = BridgeServer::new("");
        let paths = server.install_manifests("").unwrap();
        println!("INSTALLED PATHS: {:#?}", paths);
        for p in &paths {
            println!("FILE {p}: {}", fs::read_to_string(p).unwrap_or_default());
        }
        assert!(!paths.is_empty());
    }
}
