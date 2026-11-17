//! CXX FFI Bridge implementation exposing SVM Core capabilities to C++20.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

use svm_bridge::BridgeServer;
use svm_crypto::{
    evaluate_password_strength, generate_password as crypto_generate_password, generate_uuid_v4,
};
use svm_domain::{Category, JsonValue, VaultItem};
use svm_ssh_agent::{generate_ssh_keypair, SshAgentServer};
use svm_storage::VaultStorage;
use svm_totp::generate_totp as totp_generate;
use svm_transfer::{export_items_to_file, parse_import_file, TransferFormat};
use svm_vault::VaultService;

use crate::context::{
    request_ssh_approval, CoreRuntime, SvmContext, SvmEventCallback, EVENT_BRIDGE_STATUS,
    EVENT_ITEMS_CHANGED, EVENT_SETTINGS_CHANGED, EVENT_SSH_STATUS, EVENT_VAULT_LOCKED,
    EVENT_VAULT_UNLOCKED,
};

#[cxx::bridge(namespace = "svm::ffi")]
pub mod ffi {
    /// String result payload returned across FFI boundary.
    #[derive(Debug, Clone)]
    pub struct FfiStringResult {
        pub success: bool,
        pub value: String,
        pub error_message: String,
    }

    /// Boolean result payload returned across FFI boundary.
    #[derive(Debug, Clone)]
    pub struct FfiBoolResult {
        pub success: bool,
        pub value: bool,
        pub error_message: String,
    }

    /// TOTP generation result payload.
    #[derive(Debug, Clone)]
    pub struct FfiTotpResult {
        pub success: bool,
        pub code: String,
        pub remaining_seconds: i32,
        pub error_message: String,
    }

    /// Favicon raw bytes payload.
    #[derive(Debug, Clone)]
    pub struct FfiFaviconResult {
        pub success: bool,
        pub data: Vec<u8>,
        pub error_message: String,
    }

    extern "Rust" {
        type FfiContext;

        fn create_context(data_dir: &str) -> Box<FfiContext>;
        fn core_version() -> String;
        fn abi_version() -> u32;
        fn harden_process() -> bool;
        fn is_biometric_available() -> bool;

        fn parse_otpauth(uri: &str) -> FfiStringResult;
        fn decode_qr_rgba(width: u32, height: u32, rgba_bytes: &[u8]) -> FfiStringResult;
        fn generate_password(options_json: &str) -> String;
        fn password_strength(password: &str) -> FfiStringResult;

        fn set_event_callback(self: &mut FfiContext, callback_ptr: usize, user_data_ptr: usize);

        fn is_unlocked(self: &FfiContext) -> bool;

        // Vault management
        fn list_vaults(self: &FfiContext) -> FfiStringResult;
        fn create_vault(self: &mut FfiContext, name: &str, password: &str) -> FfiStringResult;
        fn unlock_vault(self: &mut FfiContext, vault_id: &str, password: &str) -> FfiBoolResult;
        fn unlock_biometric(self: &mut FfiContext) -> FfiBoolResult;
        fn lock_vault(self: &mut FfiContext) -> FfiBoolResult;
        fn get_recovery_key(self: &FfiContext) -> FfiStringResult;
        fn change_password(self: &mut FfiContext, old_pass: &str, new_pass: &str) -> FfiBoolResult;
        fn rotate_recovery_key(self: &mut FfiContext, password: &str) -> FfiStringResult;
        fn delete_vault(self: &mut FfiContext, vault_id: &str, password: &str) -> FfiBoolResult;
        fn rename_vault(self: &mut FfiContext, vault_id: &str, new_name: &str) -> FfiBoolResult;
        fn set_default_vault(self: &mut FfiContext, vault_id: &str) -> FfiBoolResult;
        fn enable_biometric(self: &mut FfiContext, password: &str) -> FfiBoolResult;
        fn disable_biometric(self: &mut FfiContext) -> FfiBoolResult;
        fn report_idle(self: &mut FfiContext, idle_ms: u64);
        fn reset_vault(self: &mut FfiContext) -> FfiBoolResult;

        // Item management
        fn list_items(self: &FfiContext, filter_json: &str) -> FfiStringResult;
        fn get_item(self: &FfiContext, item_id: &str) -> FfiStringResult;
        fn upsert_item(self: &mut FfiContext, item_json: &str) -> FfiStringResult;
        fn trash_item(self: &mut FfiContext, item_id: &str) -> FfiBoolResult;
        fn restore_item(self: &mut FfiContext, item_id: &str) -> FfiBoolResult;
        fn purge_item(self: &mut FfiContext, item_id: &str) -> FfiBoolResult;
        fn reveal_field(self: &FfiContext, item_id: &str, field_id: &str) -> FfiStringResult;

        // Revision history
        fn list_history(self: &FfiContext, item_id: &str) -> FfiStringResult;
        fn restore_history(self: &mut FfiContext, item_id: &str, revision: i32) -> FfiBoolResult;

        // Attachments
        fn list_attachments(self: &FfiContext, item_id: &str) -> FfiStringResult;
        fn add_attachment(self: &mut FfiContext, item_id: &str, src_path: &str) -> FfiStringResult;
        fn export_attachment(
            self: &FfiContext,
            attachment_id: &str,
            dst_path: &str,
        ) -> FfiBoolResult;
        fn delete_attachment(self: &mut FfiContext, attachment_id: &str) -> FfiBoolResult;

        // TOTP
        fn generate_totp(self: &FfiContext, item_id_or_secret: &str, unix_ts: u64)
            -> FfiTotpResult;

        // Import & Export
        fn import_preview(
            self: &FfiContext,
            format: i32,
            file_path: &str,
            password: &str,
        ) -> FfiStringResult;
        fn import_execute(self: &mut FfiContext, preview_id: &str) -> FfiStringResult;
        fn export_vault(
            self: &FfiContext,
            format: i32,
            file_path: &str,
            password: &str,
        ) -> FfiBoolResult;

        // SSH Agent
        fn start_ssh_agent(self: &mut FfiContext) -> FfiBoolResult;
        fn stop_ssh_agent(self: &mut FfiContext) -> FfiBoolResult;
        fn ssh_agent_status(self: &FfiContext) -> FfiStringResult;
        fn generate_ssh_key(
            self: &mut FfiContext,
            vault_id: &str,
            title: &str,
            comment: &str,
        ) -> FfiStringResult;

        // Browser Extension Bridge
        fn start_bridge(self: &mut FfiContext) -> FfiBoolResult;
        fn stop_bridge(self: &mut FfiContext) -> FfiBoolResult;
        fn bridge_clients_list(self: &FfiContext) -> FfiStringResult;
        fn native_host_install(self: &FfiContext) -> FfiBoolResult;

        // Settings & Prompts
        fn get_settings(self: &FfiContext) -> FfiStringResult;
        fn save_settings(self: &mut FfiContext, settings_json: &str) -> FfiBoolResult;
        fn fetch_favicon(self: &FfiContext, domain: &str) -> FfiFaviconResult;
        fn prompt_respond(self: &mut FfiContext, request_id: &str, approved: bool)
            -> FfiBoolResult;
    }
}

pub struct FfiContext {
    pub(crate) inner: SvmContext,
}

pub fn create_context(data_dir: &str) -> Box<FfiContext> {
    let dir = if data_dir.trim().is_empty() {
        None
    } else {
        Some(data_dir.to_string())
    };

    let effective_dir = dir
        .clone()
        .filter(|s| !s.trim().is_empty())
        .unwrap_or_else(svm_storage::resolve_default_data_dir);
    let storage = VaultStorage::new(effective_dir.clone());

    let runtime = CoreRuntime {
        vault_service: VaultService::with_storage(storage),
        ssh_agent: SshAgentServer::new(),
        bridge: BridgeServer::new(effective_dir),
        import_previews: Vec::new(),
        pending_prompts: Vec::new(),
    };

    Box::new(FfiContext {
        inner: SvmContext {
            data_dir: dir,
            unlocked: AtomicBool::new(false),
            event_callback: None,
            event_user_data: std::ptr::null_mut(),
            runtime: Mutex::new(runtime),
        },
    })
}

pub fn core_version() -> String {
    "0.1.0".to_string()
}

pub fn abi_version() -> u32 {
    1
}

pub fn harden_process() -> bool {
    svm_system::harden_current_process();
    true
}

pub fn is_biometric_available() -> bool {
    svm_system::is_biometric_available()
}

pub fn parse_otpauth(uri: &str) -> ffi::FfiStringResult {
    match svm_totp::parse_otpauth_uri(uri) {
        Ok(cfg) => ffi_str_ok(cfg.to_json().to_json_string()),
        Err(_) => ffi_str_ok("{}".to_string()),
    }
}

pub fn decode_qr_rgba(width: u32, height: u32, rgba_bytes: &[u8]) -> ffi::FfiStringResult {
    match svm_totp::decode_qr_rgba(rgba_bytes, width, height) {
        Ok(content) => ffi_str_ok(content),
        Err(e) => ffi_str_err(e.message()),
    }
}

pub fn generate_password(options_json: &str) -> String {
    crypto_generate_password(options_json)
}

pub fn password_strength(password: &str) -> ffi::FfiStringResult {
    let res = evaluate_password_strength(password);
    ffi_str_ok(res.to_json_string())
}

impl FfiContext {
    pub fn set_event_callback(&mut self, callback_ptr: usize, user_data_ptr: usize) {
        if callback_ptr == 0 {
            self.inner.event_callback = None;
            self.inner.event_user_data = std::ptr::null_mut();
        } else {
            let cb: SvmEventCallback = unsafe { std::mem::transmute(callback_ptr) };
            self.inner.event_callback = cb;
            self.inner.event_user_data = user_data_ptr as *mut std::ffi::c_void;
        }
    }

    pub fn is_unlocked(&self) -> bool {
        self.inner.unlocked.load(Ordering::Acquire)
    }

    pub fn list_vaults(&self) -> ffi::FfiStringResult {
        if let Ok(rt) = self.inner.runtime.lock() {
            ffi_str_ok(rt.vault_service.list_vaults_json())
        } else {
            ffi_str_err("Runtime mutex poisoned")
        }
    }

    pub fn create_vault(&mut self, name: &str, password: &str) -> ffi::FfiStringResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        match rt.vault_service.create_vault(name, password) {
            Ok(vault_id) => {
                self.inner.unlocked.store(true, Ordering::Release);
                self.inner.emit_event(EVENT_VAULT_UNLOCKED, "{}");
                ffi_str_ok(vault_id)
            }
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn unlock_vault(&mut self, vault_id: &str, password: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.unlock_with_credentials(vault_id, password) {
            Ok(()) => {
                self.inner.unlocked.store(true, Ordering::Release);
                let is_unlocked = rt.vault_service.is_unlocked();
                let items = rt.vault_service.items();
                let ssh_prompt = rt.vault_service.ssh_prompt_approval();
                rt.ssh_agent.sync_keys(is_unlocked, items, ssh_prompt);
                drop(rt);
                self.inner.emit_event(EVENT_VAULT_UNLOCKED, "{}");
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn unlock_biometric(&mut self) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.unlock_biometric() {
            Ok(()) => {
                self.inner.unlocked.store(true, Ordering::Release);
                let is_unlocked = rt.vault_service.is_unlocked();
                let items = rt.vault_service.items();
                let ssh_prompt = rt.vault_service.ssh_prompt_approval();
                rt.ssh_agent.sync_keys(is_unlocked, items, ssh_prompt);
                drop(rt);
                self.inner.emit_event(EVENT_VAULT_UNLOCKED, "{}");
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn lock_vault(&mut self) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        rt.vault_service.lock();
        self.inner.unlocked.store(false, Ordering::Release);
        rt.ssh_agent.sync_keys(false, &[], false);
        drop(rt);
        self.inner.emit_event(EVENT_VAULT_LOCKED, "{}");
        ffi_bool_ok(true)
    }

    pub fn get_recovery_key(&self) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };
        if !rt.vault_service.is_unlocked() {
            return ffi_str_err("Vault is locked");
        }
        ffi_str_ok(rt.vault_service.recovery_key().to_string())
    }

    pub fn change_password(&mut self, old_pass: &str, new_pass: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.change_password(old_pass, new_pass) {
            Ok(()) => ffi_bool_ok(true),
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn rotate_recovery_key(&mut self, password: &str) -> ffi::FfiStringResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        match rt.vault_service.rotate_recovery_key(password) {
            Ok(new_key) => ffi_str_ok(new_key),
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn delete_vault(&mut self, vault_id: &str, password: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.delete_vault(vault_id, password) {
            Ok(()) => {
                if !rt.vault_service.is_unlocked() {
                    self.inner.unlocked.store(false, Ordering::Release);
                }
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn rename_vault(&mut self, vault_id: &str, new_name: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.rename_vault(vault_id, new_name) {
            Ok(()) => ffi_bool_ok(true),
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn set_default_vault(&mut self, vault_id: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.set_default_vault(vault_id) {
            Ok(()) => ffi_bool_ok(true),
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn enable_biometric(&mut self, password: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.enable_biometric(password) {
            Ok(()) => ffi_bool_ok(true),
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn disable_biometric(&mut self) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.disable_biometric() {
            Ok(()) => ffi_bool_ok(true),
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn report_idle(&mut self, idle_ms: u64) {
        if let Ok(mut rt) = self.inner.runtime.lock() {
            if rt.vault_service.report_idle(idle_ms) {
                self.inner.unlocked.store(false, Ordering::Release);
                rt.ssh_agent.sync_keys(false, &[], false);
                drop(rt);
                self.inner.emit_event(EVENT_VAULT_LOCKED, "{}");
            }
        }
    }

    pub fn reset_vault(&mut self) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.reset_vault() {
            Ok(_) => {
                self.inner.unlocked.store(false, Ordering::Release);
                rt.ssh_agent.sync_keys(false, &[], false);
                drop(rt);
                self.inner.emit_event(EVENT_VAULT_LOCKED, "{}");
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    // Items
    pub fn list_items(&self, filter_json: &str) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        let filter = if filter_json.trim().is_empty() {
            None
        } else {
            Some(filter_json)
        };

        match rt.vault_service.list_items_json(filter) {
            Ok(json) => ffi_str_ok(json),
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn get_item(&self, item_id: &str) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        match rt.vault_service.get_item_json(item_id) {
            Ok(json) => ffi_str_ok(json),
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn upsert_item(&mut self, item_json: &str) -> ffi::FfiStringResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        match rt.vault_service.upsert_item_json(item_json) {
            Ok(item_id) => {
                let is_unlocked = rt.vault_service.is_unlocked();
                let items = rt.vault_service.items();
                let ssh_prompt = rt.vault_service.ssh_prompt_approval();
                rt.ssh_agent.sync_keys(is_unlocked, items, ssh_prompt);
                drop(rt);
                self.inner.emit_event(EVENT_ITEMS_CHANGED, "{}");
                ffi_str_ok(item_id)
            }
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn trash_item(&mut self, item_id: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.trash_item(item_id) {
            Ok(()) => {
                drop(rt);
                self.inner.emit_event(EVENT_ITEMS_CHANGED, "{}");
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn restore_item(&mut self, item_id: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.restore_item(item_id) {
            Ok(()) => {
                drop(rt);
                self.inner.emit_event(EVENT_ITEMS_CHANGED, "{}");
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn purge_item(&mut self, item_id: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.purge_item(item_id) {
            Ok(()) => {
                drop(rt);
                self.inner.emit_event(EVENT_ITEMS_CHANGED, "{}");
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn reveal_field(&self, item_id: &str, field_id: &str) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        match rt.vault_service.reveal_field(item_id, field_id) {
            Ok(value) => ffi_str_ok(value),
            Err(e) => ffi_str_err(e.message()),
        }
    }

    // History
    pub fn list_history(&self, item_id: &str) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        match rt.vault_service.list_history_json(item_id) {
            Ok(json) => ffi_str_ok(json),
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn restore_history(&mut self, item_id: &str, revision: i32) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.restore_history(item_id, revision as u32) {
            Ok(()) => {
                drop(rt);
                self.inner.emit_event(EVENT_ITEMS_CHANGED, "{}");
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    // Attachments
    pub fn list_attachments(&self, item_id: &str) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        match rt.vault_service.list_attachments_json(item_id) {
            Ok(json) => ffi_str_ok(json),
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn add_attachment(&mut self, item_id: &str, src_path: &str) -> ffi::FfiStringResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        match rt.vault_service.add_attachment(item_id, src_path) {
            Ok(meta_json) => {
                drop(rt);
                self.inner.emit_event(EVENT_ITEMS_CHANGED, "{}");
                ffi_str_ok(meta_json)
            }
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn export_attachment(&self, attachment_id: &str, dst_path: &str) -> ffi::FfiBoolResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.export_attachment(attachment_id, dst_path) {
            Ok(()) => ffi_bool_ok(true),
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    pub fn delete_attachment(&mut self, attachment_id: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        match rt.vault_service.delete_attachment(attachment_id) {
            Ok(()) => {
                drop(rt);
                self.inner.emit_event(EVENT_ITEMS_CHANGED, "{}");
                ffi_bool_ok(true)
            }
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    // TOTP
    pub fn generate_totp(&self, item_id_or_secret: &str, unix_ts: u64) -> ffi::FfiTotpResult {
        let secret = if let Ok(rt) = self.inner.runtime.lock() {
            rt.vault_service
                .items()
                .iter()
                .find(|it| it.id == item_id_or_secret)
                .map(|it| it.totp_secret.clone())
                .filter(|s| !s.is_empty())
                .unwrap_or_else(|| item_id_or_secret.to_string())
        } else {
            item_id_or_secret.to_string()
        };

        let (code, rem) = totp_generate(&secret, unix_ts);
        ffi::FfiTotpResult {
            success: !code.is_empty(),
            code,
            remaining_seconds: rem as i32,
            error_message: String::new(),
        }
    }

    // Import & Export
    pub fn import_preview(
        &self,
        format: i32,
        file_path: &str,
        password: &str,
    ) -> ffi::FfiStringResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        let tf = TransferFormat::from_code(format);
        match parse_import_file(tf, file_path, password) {
            Ok(items) => {
                let preview_id = format!("prev-{}", generate_uuid_v4());
                let count = items.len();
                rt.import_previews.push((preview_id.clone(), items));

                let mut obj = JsonValue::new_object();
                obj.insert("preview_id", JsonValue::String(preview_id));
                obj.insert("items_count", JsonValue::Number(count as f64));
                ffi_str_ok(obj.to_json_string())
            }
            Err(e) => ffi_str_err(e.message()),
        }
    }

    pub fn import_execute(&mut self, preview_id: &str) -> ffi::FfiStringResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        let pos = rt
            .import_previews
            .iter()
            .position(|(id, _)| id == preview_id);
        let Some(idx) = pos else {
            return ffi_str_err("Import preview session not found or expired");
        };

        let (_, items) = rt.import_previews.remove(idx);
        let mut imported = 0usize;
        let mut failed = 0usize;

        for item in items {
            match rt.vault_service.upsert_item(item) {
                Ok(_) => imported += 1,
                Err(_) => failed += 1,
            }
        }

        if imported > 0 {
            let is_unlocked = rt.vault_service.is_unlocked();
            let items = rt.vault_service.items();
            let ssh_prompt = rt.vault_service.ssh_prompt_approval();
            rt.ssh_agent.sync_keys(is_unlocked, items, ssh_prompt);
            drop(rt);
            self.inner
                .emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"import\"}");
        }

        let mut obj = JsonValue::new_object();
        obj.insert("imported", JsonValue::Number(imported as f64));
        obj.insert("failed", JsonValue::Number(failed as f64));
        ffi_str_ok(obj.to_json_string())
    }

    pub fn export_vault(&self, format: i32, file_path: &str, password: &str) -> ffi::FfiBoolResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        let tf = TransferFormat::from_code(format);
        match export_items_to_file(tf, rt.vault_service.items(), file_path, password) {
            Ok(()) => ffi_bool_ok(true),
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    // SSH
    pub fn start_ssh_agent(&mut self) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        let ctx_raw = &self.inner as *const SvmContext as usize;
        rt.ssh_agent
            .set_approval_callback(move |comment, fp| request_ssh_approval(ctx_raw, comment, fp));
        rt.ssh_agent.sync_keys(
            rt.vault_service.is_unlocked(),
            rt.vault_service.items(),
            rt.vault_service.ssh_prompt_approval(),
        );
        rt.ssh_agent.start();
        self.inner
            .emit_event(EVENT_SSH_STATUS, "{\"running\":true}");
        ffi_bool_ok(true)
    }

    pub fn stop_ssh_agent(&mut self) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        rt.ssh_agent.stop();
        self.inner
            .emit_event(EVENT_SSH_STATUS, "{\"running\":false}");
        ffi_bool_ok(true)
    }

    pub fn ssh_agent_status(&self) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        ffi_str_ok(rt.ssh_agent.status_json(rt.vault_service.items()))
    }

    pub fn generate_ssh_key(
        &mut self,
        vault_id: &str,
        title: &str,
        comment: &str,
    ) -> ffi::FfiStringResult {
        let kp = generate_ssh_keypair(comment);
        let mut item = VaultItem {
            id: format!("ssh-{}", generate_uuid_v4()),
            vault_id: vault_id.to_string(),
            title: title.to_string(),
            category: Category::SshKey,
            username: comment.to_string(),
            ssh_public_key: kp.public_key_openssh,
            ssh_fingerprint: kp.fingerprint_sha256,
            notes: kp.private_key_pem,
            is_ssh_loaded: true,
            ..Default::default()
        };

        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        if item.vault_id.is_empty() {
            item.vault_id = rt.vault_service.default_vault_id();
        }
        match rt.vault_service.upsert_item(item.clone()) {
            Ok(saved_id) => {
                let is_unlocked = rt.vault_service.is_unlocked();
                let items = rt.vault_service.items();
                let ssh_prompt = rt.vault_service.ssh_prompt_approval();
                rt.ssh_agent.sync_keys(is_unlocked, items, ssh_prompt);
                drop(rt);
                self.inner
                    .emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"ssh_generate\"}");
                ffi_str_ok(saved_id)
            }
            Err(e) => ffi_str_err(e.message()),
        }
    }

    // Bridge
    pub fn start_bridge(&mut self) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        let ctx_raw = &self.inner as *const SvmContext as usize;
        rt.bridge
            .start(move |req_json| crate::service_ffi::dispatch_bridge_request(ctx_raw, req_json));
        self.inner
            .emit_event(EVENT_BRIDGE_STATUS, "{\"listening\":true}");
        ffi_bool_ok(true)
    }

    pub fn stop_bridge(&mut self) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        rt.bridge.stop();
        self.inner
            .emit_event(EVENT_BRIDGE_STATUS, "{\"listening\":false}");
        ffi_bool_ok(true)
    }

    pub fn bridge_clients_list(&self) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        ffi_str_ok(rt.bridge.list_clients_json())
    }

    pub fn native_host_install(&self) -> ffi::FfiBoolResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        let dir = &rt.vault_service.storage().base_path;
        match rt.bridge.install_manifests(dir) {
            Ok(_) => ffi_bool_ok(true),
            Err(e) => ffi_bool_err(e.message()),
        }
    }

    // Settings
    pub fn get_settings(&self) -> ffi::FfiStringResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_str_err("Runtime mutex poisoned");
        };

        ffi_str_ok(rt.vault_service.storage().load_settings())
    }

    pub fn save_settings(&mut self, settings_json: &str) -> ffi::FfiBoolResult {
        let Ok(mut rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        rt.vault_service.apply_settings_json(settings_json);
        let is_unlocked = rt.vault_service.is_unlocked();
        let items = rt.vault_service.items();
        let ssh_prompt = rt.vault_service.ssh_prompt_approval();
        rt.ssh_agent.sync_keys(is_unlocked, items, ssh_prompt);
        if let Err(e) = rt.vault_service.storage().save_settings(settings_json) {
            return ffi_bool_err(e.message());
        }
        drop(rt);
        self.inner.emit_event(EVENT_SETTINGS_CHANGED, settings_json);
        ffi_bool_ok(true)
    }

    pub fn fetch_favicon(&self, domain: &str) -> ffi::FfiFaviconResult {
        let (enabled, base_path) = {
            let Ok(rt) = self.inner.runtime.lock() else {
                return ffi_fav_err("Runtime mutex poisoned");
            };
            (
                rt.vault_service.favicon_fetch_enabled(),
                rt.vault_service.storage().base_path.clone(),
            )
        };

        if !enabled {
            return ffi_fav_err("Favicon fetching disabled");
        }

        let Some(host) = crate::service_ffi::normalize_favicon_domain(domain) else {
            return ffi_fav_err("Invalid domain");
        };

        const MAX_FAVICON_BYTES: usize = 256 * 1024;
        let cache_dir = std::path::Path::new(&base_path).join("favicons");
        let cache_path = cache_dir.join(format!("{host}.bin"));

        if let Ok(cached) = std::fs::read(&cache_path) {
            if !cached.is_empty() && cached.len() <= MAX_FAVICON_BYTES {
                return ffi::FfiFaviconResult {
                    success: true,
                    data: cached,
                    error_message: String::new(),
                };
            }
        }

        let agent: ureq::Agent = ureq::Agent::config_builder()
            .timeout_global(Some(std::time::Duration::from_secs(5)))
            .max_redirects(3)
            .build()
            .into();

        for path in ["favicon.ico", "favicon.png", "apple-touch-icon.png"] {
            let url = format!("https://{host}/{path}");
            if let Ok(mut resp) = agent.get(&url).call() {
                if resp.status().is_success() {
                    let mut buf = Vec::new();
                    use std::io::Read;
                    if resp
                        .body_mut()
                        .as_reader()
                        .take(MAX_FAVICON_BYTES as u64 + 1)
                        .read_to_end(&mut buf)
                        .is_ok()
                        && !buf.is_empty()
                        && buf.len() <= MAX_FAVICON_BYTES
                    {
                        let _ = std::fs::create_dir_all(&cache_dir);
                        let _ = std::fs::write(&cache_path, &buf);
                        return ffi::FfiFaviconResult {
                            success: true,
                            data: buf,
                            error_message: String::new(),
                        };
                    }
                }
            }
        }

        ffi_fav_err("Favicon not found")
    }

    pub fn prompt_respond(&mut self, request_id: &str, approved: bool) -> ffi::FfiBoolResult {
        let Ok(rt) = self.inner.runtime.lock() else {
            return ffi_bool_err("Runtime mutex poisoned");
        };

        let found = rt
            .pending_prompts
            .iter()
            .find(|(id, _)| id == request_id)
            .map(|(_, e)| std::sync::Arc::clone(&e.waiter));

        if let Some(waiter) = found {
            let (lock, cvar) = &*waiter;
            let mut guard = lock.lock().unwrap_or_else(|e| e.into_inner());
            *guard = Some(approved);
            cvar.notify_all();
            ffi_bool_ok(true)
        } else {
            ffi_bool_err("Prompt request not found or timed out")
        }
    }
}

fn ffi_str_ok(val: String) -> ffi::FfiStringResult {
    ffi::FfiStringResult {
        success: true,
        value: val,
        error_message: String::new(),
    }
}

fn ffi_str_err(msg: impl ToString) -> ffi::FfiStringResult {
    ffi::FfiStringResult {
        success: false,
        value: String::new(),
        error_message: msg.to_string(),
    }
}

fn ffi_bool_ok(val: bool) -> ffi::FfiBoolResult {
    ffi::FfiBoolResult {
        success: true,
        value: val,
        error_message: String::new(),
    }
}

fn ffi_bool_err(msg: impl ToString) -> ffi::FfiBoolResult {
    ffi::FfiBoolResult {
        success: false,
        value: false,
        error_message: msg.to_string(),
    }
}

fn ffi_fav_err(msg: impl ToString) -> ffi::FfiFaviconResult {
    ffi::FfiFaviconResult {
        success: false,
        data: Vec::new(),
        error_message: msg.to_string(),
    }
}
