//! Bộ thực thi các lệnh nghiệp vụ của `svm-cli` (`execute_cli`).

use std::env;

use svm_crypto::{evaluate_password_strength, generate_password, generate_recovery_key};
use svm_domain::{Category, JsonValue, VaultItem};
use svm_ssh_agent::generate_ssh_keypair;
use svm_storage::VaultStorage;
use svm_totp::{generate_totp, parse_otpauth_uri};
use svm_transfer::{export_items_to_file, parse_import_file};
use svm_vault::VaultService;

use crate::args::{extract_positionals, find_flag_value, has_flag, parse_format_str, print_help};

fn open_vault_service(args: &[String]) -> Result<VaultService, String> {
    let data_dir = find_flag_value(args, "--data-dir")
        .or_else(|| env::var("SVM_DATA_DIR").ok())
        .unwrap_or_default();
    let password = find_flag_value(args, "--password")
        .or_else(|| env::var("SVM_MASTER_PASSWORD").ok())
        .unwrap_or_default();

    let mut svc = VaultService::with_storage(VaultStorage::new(data_dir));
    if svc.is_configured() {
        if !password.is_empty() {
            svc.unlock_with_credentials("personal", &password)
                .map_err(|e| e.message().to_string())?;
        } else {
            let _ = svc.unlock_biometric();
        }
    } else {
        let _ = svc.unlock_biometric();
    }
    Ok(svc)
}

/// Thực thi lệnh CLI từ danh sách đối số và trả về chuỗi kết quả JSON.
pub fn execute_cli(args: &[String]) -> Result<String, String> {
    let positionals = extract_positionals(args);

    if has_flag(args, "--version") || has_flag(args, "-v") {
        return Ok("svm-cli 0.1.0".to_string());
    }
    if has_flag(args, "--help") || has_flag(args, "-h") || positionals.is_empty() {
        print_help();
        return Ok(String::new());
    }

    let cmd = positionals[0].as_str();
    match cmd {
        "version" => Ok("svm-cli 0.1.0".to_string()),
        "help" => {
            print_help();
            Ok(String::new())
        }
        "status" => {
            let svc = open_vault_service(args)?;
            let mut obj = JsonValue::new_object();
            obj.insert("configured", JsonValue::Bool(svc.is_configured()));
            obj.insert("unlocked", JsonValue::Bool(svc.is_unlocked()));
            obj.insert(
                "dataDir",
                JsonValue::String(svc.storage().base_path.clone()),
            );
            obj.insert("itemCount", JsonValue::Number(svc.items().len() as f64));
            Ok(obj.to_json_string())
        }
        "init" => {
            let password = find_flag_value(args, "--password")
                .or_else(|| env::var("SVM_MASTER_PASSWORD").ok())
                .unwrap_or_else(|| "MasterPassword!2026".to_string());
            let vault_name =
                find_flag_value(args, "--name").unwrap_or_else(|| "Personal Vault".to_string());
            let data_dir = find_flag_value(args, "--data-dir")
                .or_else(|| env::var("SVM_DATA_DIR").ok())
                .unwrap_or_default();

            let mut svc = VaultService::with_storage(VaultStorage::new(data_dir));
            let recovery_key = generate_recovery_key();
            let mut meta = JsonValue::new_object();
            meta.insert("initialSetup", JsonValue::Bool(true));
            meta.insert("name", JsonValue::String(vault_name.clone()));
            meta.insert("recoveryKey", JsonValue::String(recovery_key.clone()));

            let vault_id = svc
                .create_vault(&meta.to_json_string(), &password)
                .map_err(|e| e.message().to_string())?;

            let mut out = JsonValue::new_object();
            out.insert("initialized", JsonValue::Bool(true));
            out.insert("vaultId", JsonValue::String(vault_id));
            out.insert("name", JsonValue::String(vault_name));
            out.insert("recoveryKey", JsonValue::String(recovery_key));
            Ok(out.to_json_string())
        }
        "vault" => {
            let sub = positionals.get(1).map(|s| s.as_str()).unwrap_or("list");
            let mut svc = open_vault_service(args)?;
            match sub {
                "list" => Ok(svc.list_vaults_json()),
                "create" => {
                    let name = positionals
                        .get(2)
                        .cloned()
                        .or_else(|| find_flag_value(args, "--name"))
                        .ok_or_else(|| "Missing vault name".to_string())?;
                    let id = svc
                        .create_vault(&name, "")
                        .map_err(|e| e.message().to_string())?;
                    let mut out = JsonValue::new_object();
                    out.insert("id", JsonValue::String(id));
                    out.insert("name", JsonValue::String(name));
                    Ok(out.to_json_string())
                }
                "delete" => {
                    let id = positionals
                        .get(2)
                        .ok_or_else(|| "Missing vault ID".to_string())?;
                    svc.delete_vault(id, "")
                        .map_err(|e| e.message().to_string())?;
                    Ok(format!(r#"{{"deleted":true,"vaultId":"{id}"}}"#))
                }
                "default" => {
                    let id = positionals
                        .get(2)
                        .ok_or_else(|| "Missing vault ID".to_string())?;
                    svc.set_default_vault(id)
                        .map_err(|e| e.message().to_string())?;
                    Ok(format!(r#"{{"defaultVaultId":"{id}"}}"#))
                }
                other => Err(format!("Unknown vault subcommand: {other}")),
            }
        }
        "item" => {
            let sub = positionals.get(1).map(|s| s.as_str()).unwrap_or("list");
            let mut svc = open_vault_service(args)?;
            match sub {
                "list" => {
                    let mut filter = JsonValue::new_object();
                    if let Some(cat) = find_flag_value(args, "--category") {
                        filter.insert("category", JsonValue::String(cat));
                    }
                    if let Some(q) = find_flag_value(args, "--search") {
                        filter.insert("search", JsonValue::String(q));
                    }
                    filter.insert(
                        "includeTrashed",
                        JsonValue::Bool(has_flag(args, "--trashed")),
                    );
                    svc.list_items_json(Some(&filter.to_json_string()))
                        .map_err(|e| e.message().to_string())
                }
                "get" => {
                    let id = positionals
                        .get(2)
                        .ok_or_else(|| "Missing item ID".to_string())?;
                    svc.get_item_json(id).map_err(|e| e.message().to_string())
                }
                "add" => {
                    let title = find_flag_value(args, "--title")
                        .or_else(|| positionals.get(2).cloned())
                        .unwrap_or_else(|| "CLI Entry".to_string());
                    let username = find_flag_value(args, "--username")
                        .or_else(|| find_flag_value(args, "--user"))
                        .unwrap_or_default();
                    let password = find_flag_value(args, "--pass")
                        .or_else(|| find_flag_value(args, "--password"))
                        .unwrap_or_default();
                    let url = find_flag_value(args, "--url").unwrap_or_default();
                    let totp = find_flag_value(args, "--totp").unwrap_or_default();
                    let cat_str =
                        find_flag_value(args, "--category").unwrap_or_else(|| "login".to_string());

                    let item = VaultItem {
                        vault_id: "personal".to_string(),
                        title,
                        category: Category::parse_str(&cat_str),
                        username,
                        password,
                        website_url: url,
                        has_totp: !totp.is_empty(),
                        totp_secret: totp,
                        ..Default::default()
                    };
                    let id = svc.upsert_item(item).map_err(|e| e.message().to_string())?;
                    Ok(format!(r#"{{"saved":true,"id":"{id}"}}"#))
                }
                "trash" => {
                    let id = positionals
                        .get(2)
                        .ok_or_else(|| "Missing item ID".to_string())?;
                    svc.trash_item(id).map_err(|e| e.message().to_string())?;
                    Ok(format!(r#"{{"trashed":true,"id":"{id}"}}"#))
                }
                "restore" => {
                    let id = positionals
                        .get(2)
                        .ok_or_else(|| "Missing item ID".to_string())?;
                    svc.restore_item(id).map_err(|e| e.message().to_string())?;
                    Ok(format!(r#"{{"restored":true,"id":"{id}"}}"#))
                }
                "purge" => {
                    let id = positionals
                        .get(2)
                        .ok_or_else(|| "Missing item ID".to_string())?;
                    svc.purge_item(id).map_err(|e| e.message().to_string())?;
                    Ok(format!(r#"{{"purged":true,"id":"{id}"}}"#))
                }
                other => Err(format!("Unknown item subcommand: {other}")),
            }
        }
        "totp" => {
            let secret_or_uri = positionals
                .get(1)
                .ok_or_else(|| "Missing TOTP secret or otpauth:// URI".to_string())?;
            let ts = find_flag_value(args, "--time")
                .and_then(|v| v.parse::<u64>().ok())
                .unwrap_or(0);
            let cfg = parse_otpauth_uri(secret_or_uri).map_err(|e| e.message().to_string())?;
            let (code, remaining) = generate_totp(secret_or_uri, ts);
            let mut out = JsonValue::new_object();
            out.insert("code", JsonValue::String(code));
            out.insert("remaining", JsonValue::Number(remaining as f64));
            if !cfg.issuer.is_empty() {
                out.insert("issuer", JsonValue::String(cfg.issuer));
            }
            if !cfg.account.is_empty() {
                out.insert("account", JsonValue::String(cfg.account));
            }
            Ok(out.to_json_string())
        }
        "password" => {
            let sub = positionals.get(1).map(|s| s.as_str()).unwrap_or("generate");
            match sub {
                "generate" => {
                    let length = find_flag_value(args, "--length")
                        .and_then(|v| v.parse::<i64>().ok())
                        .unwrap_or(20);
                    let mut opts = JsonValue::new_object();
                    opts.insert("length", JsonValue::Number(length as f64));
                    opts.insert("uppercase", JsonValue::Bool(!has_flag(args, "--no-upper")));
                    opts.insert("lowercase", JsonValue::Bool(!has_flag(args, "--no-lower")));
                    opts.insert("digits", JsonValue::Bool(!has_flag(args, "--no-digits")));
                    opts.insert("symbols", JsonValue::Bool(!has_flag(args, "--no-symbols")));
                    let pwd = generate_password(&opts.to_json_string());
                    let eval = evaluate_password_strength(&pwd);
                    let mut out = JsonValue::new_object();
                    out.insert("password", JsonValue::String(pwd));
                    out.insert("score", JsonValue::Number(eval.score as f64));
                    out.insert("percent", JsonValue::Number(eval.percent as f64));
                    Ok(out.to_json_string())
                }
                "strength" => {
                    let pwd = positionals
                        .get(2)
                        .ok_or_else(|| "Missing password to evaluate".to_string())?;
                    Ok(evaluate_password_strength(pwd).to_json_string())
                }
                other => Err(format!("Unknown password subcommand: {other}")),
            }
        }
        "ssh" => {
            let sub = positionals.get(1).map(|s| s.as_str()).unwrap_or("keygen");
            if sub != "keygen" {
                return Err(format!("Unknown ssh subcommand: {sub}"));
            }
            let comment = find_flag_value(args, "--comment")
                .unwrap_or_else(|| "user@secure-vault".to_string());
            let kp = generate_ssh_keypair(&comment);
            let mut out = JsonValue::new_object();
            out.insert("publicKey", JsonValue::String(kp.public_key_openssh));
            out.insert("fingerprint", JsonValue::String(kp.fingerprint_sha256));
            out.insert("privateKeyPem", JsonValue::String(kp.private_key_pem));
            Ok(out.to_json_string())
        }
        "export" => {
            let fmt_str = find_flag_value(args, "--format").unwrap_or_else(|| "json".to_string());
            let output_path = find_flag_value(args, "--output")
                .ok_or_else(|| "Missing --output <FILE>".to_string())?;
            let key = find_flag_value(args, "--key").unwrap_or_default();
            let svc = open_vault_service(args)?;
            let fmt = parse_format_str(&fmt_str);
            export_items_to_file(fmt, svc.items(), &output_path, &key)
                .map_err(|e| e.message().to_string())?;
            let mut out = JsonValue::new_object();
            out.insert("exported", JsonValue::Bool(true));
            out.insert("format", JsonValue::String(fmt_str));
            out.insert("output", JsonValue::String(output_path));
            out.insert("count", JsonValue::Number(svc.items().len() as f64));
            Ok(out.to_json_string())
        }
        "import" => {
            let fmt_str = find_flag_value(args, "--format").unwrap_or_else(|| "json".to_string());
            let input_path = find_flag_value(args, "--input")
                .ok_or_else(|| "Missing --input <FILE>".to_string())?;
            let key = find_flag_value(args, "--key").unwrap_or_default();
            let mut svc = open_vault_service(args)?;
            let fmt = parse_format_str(&fmt_str);
            let items =
                parse_import_file(fmt, &input_path, &key).map_err(|e| e.message().to_string())?;
            let count = items.len();
            for item in items {
                let _ = svc.upsert_item(item);
            }
            let mut out = JsonValue::new_object();
            out.insert("imported", JsonValue::Bool(true));
            out.insert("format", JsonValue::String(fmt_str));
            out.insert("count", JsonValue::Number(count as f64));
            Ok(out.to_json_string())
        }
        other => Err(format!(
            "Unknown command '{other}'. Run 'svm-cli --help' for usage."
        )),
    }
}
