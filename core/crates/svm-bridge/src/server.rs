//! Máy chủ cầu nối trình duyệt (`BridgeServer`): quản lý phiên kết nối trình duyệt,
//! lắng nghe kết nối IPC socket cục bộ và cài đặt Native Messaging Host manifest.

use std::fs;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

#[cfg(unix)]
use std::os::unix::net::UnixListener;

use svm_crypto::generate_uuid_v4;
use svm_domain::{DomainError, JsonValue};
use svm_ipc::IpcEndpoint;

use crate::client::PairedClient;

/// Tên Named Pipe IPC trên Windows (cô lập theo tên người dùng hiện tại).
pub fn resolve_windows_pipe_name() -> String {
    let user = std::env::var("USERNAME")
        .or_else(|_| std::env::var("USER"))
        .unwrap_or_else(|_| "default".to_string());
    let sanitized: String = user
        .chars()
        .map(|c| if c.is_ascii_alphanumeric() { c } else { '_' })
        .collect();
    format!(r"\\.\pipe\hephaestus-svm-bridge-{sanitized}")
}

/// Xác định đường dẫn Unix Domain Socket hoặc Named Pipe của Bridge Server.
pub fn resolve_socket_path() -> PathBuf {
    #[cfg(windows)]
    {
        return PathBuf::from(resolve_windows_pipe_name());
    }
    #[cfg(not(windows))]
    {
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
}

/// Dò tìm đường dẫn binary thực thi của `svm-native-host`.
pub fn detect_native_host_binary() -> PathBuf {
    let bin_name = if cfg!(windows) {
        "svm-native-host.exe"
    } else {
        "svm-native-host"
    };

    // 1. Kiểm tra cùng thư mục với tiến trình hiện hành
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            let candidate = parent.join(bin_name);
            if candidate.exists() {
                return candidate;
            }
            let candidate_core = parent.join("core").join(bin_name);
            if candidate_core.exists() {
                return candidate_core;
            }
        }
    }
    // 2. Kiểm tra thư mục build / target
    for rel_dir in &[
        "core/target/debug",
        "core/target/release",
        "build/debug/core",
        "build/release/core",
    ] {
        let p = PathBuf::from(rel_dir).join(bin_name);
        if p.exists() {
            if let Ok(abs) = fs::canonicalize(&p) {
                return abs;
            }
            return p;
        }
    }
    #[cfg(windows)]
    {
        if let Ok(local_app) = std::env::var("LOCALAPPDATA") {
            let p = PathBuf::from(local_app)
                .join("Hephaestus")
                .join("SecureVaultManager")
                .join(bin_name);
            if p.exists() {
                return p;
            }
        }
        return PathBuf::from(bin_name);
    }
    #[cfg(not(windows))]
    {
        // 3. ~/.local/bin
        if let Ok(home) = std::env::var("HOME") {
            let p = PathBuf::from(home).join(".local/bin").join(bin_name);
            if p.exists() {
                return p;
            }
        }
        // 4. macOS app bundle hoặc /usr/local/bin / /usr/bin
        #[cfg(target_os = "macos")]
        {
            let p_app = PathBuf::from(
                "/Applications/Secure Vault Manager.app/Contents/MacOS/svm-native-host",
            );
            if p_app.exists() {
                return p_app;
            }
        }
        let p_usr_local = PathBuf::from("/usr/local/bin").join(bin_name);
        if p_usr_local.exists() {
            return p_usr_local;
        }
        PathBuf::from("/usr/bin").join(bin_name)
    }
}

/// Máy chủ cầu nối tiếp nhận và điều phối các yêu cầu từ Browser Extension.
pub struct BridgeServer {
    listening: bool,
    data_dir: String,
    clients: Vec<PairedClient>,
    revoked_browsers: Vec<String>,
    bound_socket_path: Option<PathBuf>,
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
            revoked_browsers: Vec::new(),
            bound_socket_path: None,
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
            use std::os::unix::fs::PermissionsExt;

            let candidates = {
                let mut c = Vec::new();
                if let Ok(runtime_dir) = std::env::var("XDG_RUNTIME_DIR") {
                    let dir = PathBuf::from(runtime_dir).join("svm");
                    if fs::create_dir_all(&dir).is_ok() {
                        let _ = fs::set_permissions(&dir, fs::Permissions::from_mode(0o700));
                        c.push(dir.join("bridge.sock"));
                    }
                }
                if let Ok(home) = std::env::var("HOME") {
                    let dir = PathBuf::from(home).join(".svm");
                    if fs::create_dir_all(&dir).is_ok() {
                        let _ = fs::set_permissions(&dir, fs::Permissions::from_mode(0o700));
                        c.push(dir.join("bridge.sock"));
                    }
                }
                // Dùng thư mục theo USER trong tmp để tránh xung đột quyền giữa các user
                let user_suffix = std::env::var("USER")
                    .or_else(|_| std::env::var("LOGNAME"))
                    .unwrap_or_else(|_| "user".to_string());
                let dir = PathBuf::from(format!("/tmp/hephaestus-svm-data-{user_suffix}"));
                if fs::create_dir_all(&dir).is_ok() {
                    let _ = fs::set_permissions(&dir, fs::Permissions::from_mode(0o700));
                    c.push(dir.join("bridge.sock"));
                }
                c
            };

            let mut bound_listener = None;
            let mut bound_path = None;

            for socket_path in candidates {
                if socket_path.exists() {
                    let _ = fs::remove_file(&socket_path);
                }
                if let Ok(listener) = UnixListener::bind(&socket_path) {
                    let _ = fs::set_permissions(&socket_path, fs::Permissions::from_mode(0o600));
                    let _ = listener.set_nonblocking(true);
                    bound_listener = Some(listener);
                    bound_path = Some(socket_path);
                    break;
                }
            }

            if let (Some(listener), Some(socket_path)) = (bound_listener, bound_path) {
                self.bound_socket_path = Some(socket_path.clone());
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
            } else {
                self.listening = false;
            }
        }

        #[cfg(windows)]
        {
            use std::os::windows::io::FromRawHandle;
            use windows_sys::Win32::Foundation::{
                CloseHandle, GetLastError, ERROR_PIPE_CONNECTED, INVALID_HANDLE_VALUE,
            };
            use windows_sys::Win32::Storage::FileSystem::PIPE_ACCESS_DUPLEX;
            use windows_sys::Win32::System::Pipes::{
                ConnectNamedPipe, CreateNamedPipeW, PIPE_TYPE_BYTE, PIPE_WAIT,
            };

            let pipe_name = resolve_windows_pipe_name();
            self.bound_socket_path = Some(PathBuf::from(&pipe_name));
            let wide_name: Vec<u16> = pipe_name.encode_utf16().chain(std::iter::once(0)).collect();

            let handle = std::thread::spawn(move || {
                while !shutdown.load(Ordering::SeqCst) {
                    let h_pipe = unsafe {
                        CreateNamedPipeW(
                            wide_name.as_ptr(),
                            PIPE_ACCESS_DUPLEX,
                            PIPE_TYPE_BYTE | PIPE_WAIT,
                            64,
                            65536,
                            65536,
                            0,
                            core::ptr::null(),
                        )
                    };
                    if h_pipe == INVALID_HANDLE_VALUE {
                        std::thread::sleep(Duration::from_millis(50));
                        continue;
                    }
                    let ok = unsafe { ConnectNamedPipe(h_pipe, core::ptr::null_mut()) };
                    let connected = ok != 0 || unsafe { GetLastError() } == ERROR_PIPE_CONNECTED;
                    if shutdown.load(Ordering::SeqCst) || !connected {
                        unsafe {
                            CloseHandle(h_pipe);
                        }
                        if shutdown.load(Ordering::SeqCst) {
                            break;
                        }
                        continue;
                    }
                    let mut file = unsafe { fs::File::from_raw_handle(h_pipe as _) };
                    let h_cb = Arc::clone(&handler);
                    std::thread::spawn(move || {
                        handle_client_stream(&mut file, &h_cb);
                    });
                }
            });
            self.listener_thread = Some(handle);
        }
    }

    /// Dừng lắng nghe socket và đóng các kết nối.
    pub fn stop(&mut self) {
        if !self.listening {
            return;
        }
        self.listening = false;
        self.shutdown_signal.store(true, Ordering::SeqCst);
        #[cfg(windows)]
        {
            if let Some(ref pipe_path) = self.bound_socket_path {
                // Đánh thức ConnectNamedPipe đang chờ bằng một kết nối rỗng
                let _ = fs::OpenOptions::new()
                    .read(true)
                    .write(true)
                    .open(pipe_path);
            }
        }
        if let Some(handle) = self.listener_thread.take() {
            let _ = handle.join();
        }
        #[cfg(unix)]
        {
            if let Some(socket_path) = self.bound_socket_path.take() {
                if socket_path.exists() {
                    let _ = fs::remove_file(&socket_path);
                }
            }
        }
        #[cfg(windows)]
        {
            self.bound_socket_path = None;
        }
    }

    /// Ghi nhận hoặc cập nhật phiên trình duyệt đang hoạt động (Active Session).
    /// Trả về `(PairedClient, bool)` trong đó `bool` là true nếu đây là client mới hoặc vừa được kích hoạt lại.
    pub fn record_active_session(&mut self, browser: &str, version: &str) -> (PairedClient, bool) {
        let b = if browser.trim().is_empty() {
            "Google Chrome"
        } else {
            browser.trim()
        };
        let v = if version.trim().is_empty() {
            "v1.0.0"
        } else {
            version.trim()
        };

        if let Some(client) = self.clients.iter_mut().find(|c| c.browser == b) {
            client.version = v.to_string();
            client.last_used = "Vừa mới xong".to_string();
            let updated = client.clone();
            self.save_clients();
            return (updated, false);
        }

        let client = PairedClient {
            id: format!("ext-{}", generate_uuid_v4()),
            browser: b.to_string(),
            version: v.to_string(),
            last_used: "Vừa mới xong".to_string(),
            session_token: format!("svm-sess-{}", generate_uuid_v4()),
        };
        self.clients.push(client.clone());
        self.save_clients();
        (client, true)
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
        for c in &self.clients {
            if c.id == client_id && !self.revoked_browsers.contains(&c.browser) {
                self.revoked_browsers.push(c.browser.clone());
            }
        }
        self.clients.retain(|c| c.id != client_id);
        self.save_clients();
    }

    /// Kiểm tra trình duyệt đã bị người dùng thu hồi quyền trong phiên hiện tại hay chưa.
    pub fn is_browser_revoked(&self, browser: &str) -> bool {
        let b = browser.trim();
        !b.is_empty() && self.revoked_browsers.iter().any(|r| r == b)
    }

    /// Cho phép trình duyệt kết nối lại sau khi mở khóa chủ động.
    pub fn clear_browser_revocation(&mut self, browser: &str) {
        let b = browser.trim();
        self.revoked_browsers.retain(|r| r != b);
    }

    /// Trả về tham chiếu danh sách các trình duyệt đã kết nối.
    pub fn clients(&self) -> &[PairedClient] {
        &self.clients
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
                                let is_svm_extension = path_str.contains("secure-vault-manager")
                                    || path_str.contains("dist/chrome")
                                    || path_str.contains("dist/firefox")
                                    || name_str.contains("secure vault manager");
                                if is_svm_extension
                                    && ext_id.len() == 32
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

        ids.into_iter()
            .map(|id| format!("chrome-extension://{id}/"))
            .collect()
    }

    /// Cài đặt tệp cấu hình Native Messaging Host manifest vào thư mục cấu hình trình duyệt người dùng.
    pub fn install_manifests(&self, data_dir: &str) -> Result<Vec<String>, DomainError> {
        let host_bin = detect_native_host_binary();
        let host_bin_str = host_bin.to_string_lossy().to_string();

        let mut installed_paths = Vec::new();
        let home = std::env::var("HOME")
            .or_else(|_| std::env::var("USERPROFILE"))
            .unwrap_or_else(|_| "/tmp".to_string());

        let candidate_dirs = [
            // Linux Chromium-family
            format!("{home}/.config/google-chrome/NativeMessagingHosts"),
            format!("{home}/.config/chromium/NativeMessagingHosts"),
            format!("{home}/.config/BraveSoftware/Brave-Browser/NativeMessagingHosts"),
            format!("{home}/.config/microsoft-edge/NativeMessagingHosts"),
            // Linux Firefox
            format!("{home}/.mozilla/native-messaging-hosts"),
            // macOS Chromium-family & Firefox
            format!("{home}/Library/Application Support/Google/Chrome/NativeMessagingHosts"),
            format!("{home}/Library/Application Support/Chromium/NativeMessagingHosts"),
            format!("{home}/Library/Application Support/BraveSoftware/Brave-Browser/NativeMessagingHosts"),
            format!("{home}/Library/Application Support/Microsoft Edge/NativeMessagingHosts"),
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
                if parent.exists() && fs::create_dir_all(p).is_ok() {
                    let manifest_file = p.join(manifest_name);
                    let content = if dir_str.contains("Mozilla") || dir_str.contains("mozilla") {
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

        // Luôn ghi thêm vào thư mục dữ liệu cục bộ làm dự phòng
        let fallback_base = if data_dir.trim().is_empty() {
            if self.data_dir.trim().is_empty() {
                svm_storage::resolve_default_data_dir()
            } else {
                self.data_dir.clone()
            }
        } else {
            data_dir.to_string()
        };
        let fallback_dir = Path::new(&fallback_base).join("native-messaging-hosts");
        let _ = fs::create_dir_all(&fallback_dir);
        let fallback_file = fallback_dir.join(manifest_name);
        let _ = fs::write(&fallback_file, chrome_json.as_bytes());
        installed_paths.push(fallback_file.to_string_lossy().to_string());

        #[cfg(windows)]
        {
            let firefox_file = fallback_dir.join("io.hephaestus.secure_vault_manager.firefox.json");
            let _ = fs::write(&firefox_file, firefox_json.as_bytes());
            register_windows_native_host(
                &fallback_file.to_string_lossy(),
                &firefox_file.to_string_lossy(),
            );
            installed_paths.push(firefox_file.to_string_lossy().to_string());
        }

        Ok(installed_paths)
    }
}

impl Default for BridgeServer {
    fn default() -> Self {
        Self::with_default_dir()
    }
}

impl Drop for BridgeServer {
    fn drop(&mut self) {
        self.stop();
    }
}

#[cfg(windows)]
fn register_windows_native_host(chrome_manifest_path: &str, firefox_manifest_path: &str) {
    use windows_sys::Win32::Foundation::ERROR_SUCCESS;
    use windows_sys::Win32::System::Registry::{
        RegCloseKey, RegCreateKeyExW, RegSetValueExW, HKEY_CURRENT_USER, KEY_WRITE, REG_SZ,
    };

    let entries = [
        (
            r"Software\Google\Chrome\NativeMessagingHosts\io.hephaestus.secure_vault_manager",
            chrome_manifest_path,
        ),
        (
            r"Software\Chromium\NativeMessagingHosts\io.hephaestus.secure_vault_manager",
            chrome_manifest_path,
        ),
        (
            r"Software\BraveSoftware\Brave-Browser\NativeMessagingHosts\io.hephaestus.secure_vault_manager",
            chrome_manifest_path,
        ),
        (
            r"Software\Microsoft\Edge\NativeMessagingHosts\io.hephaestus.secure_vault_manager",
            chrome_manifest_path,
        ),
        (
            r"Software\Mozilla\NativeMessagingHosts\io.hephaestus.secure_vault_manager",
            firefox_manifest_path,
        ),
    ];

    for (subkey, path_str) in entries {
        let subkey_w: Vec<u16> = subkey.encode_utf16().chain(std::iter::once(0)).collect();
        let val_w: Vec<u16> = path_str.encode_utf16().chain(std::iter::once(0)).collect();
        let mut hkey = 0 as _;
        let rc = unsafe {
            RegCreateKeyExW(
                HKEY_CURRENT_USER,
                subkey_w.as_ptr(),
                0,
                core::ptr::null(),
                0,
                KEY_WRITE,
                core::ptr::null(),
                &mut hkey,
                core::ptr::null_mut(),
            )
        };
        if rc == ERROR_SUCCESS && !hkey.is_null() {
            let byte_len = (val_w.len() * 2) as u32;
            unsafe {
                RegSetValueExW(
                    hkey,
                    core::ptr::null(),
                    0,
                    REG_SZ,
                    val_w.as_ptr() as *const u8,
                    byte_len,
                );
                RegCloseKey(hkey);
            }
        }
    }
}

#[cfg(any(unix, windows))]
fn handle_client_stream<S: Read + Write>(
    stream: &mut S,
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
