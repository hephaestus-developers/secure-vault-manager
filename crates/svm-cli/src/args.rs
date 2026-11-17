//! Tiện ích phân tích đối số dòng lệnh (`args`) và hiển thị hướng dẫn sử dụng (`print_help`) cho `svm-cli`.

use svm_transfer::TransferFormat;

/// In thông điệp hướng dẫn sử dụng chi tiết ra luồng `stdout`.
pub fn print_help() {
    println!("svm-cli 0.1.0 - Secure Vault Manager Command Line Interface");
    println!();
    println!("USAGE:");
    println!("  svm-cli [--data-dir <DIR>] [--password <PW>] <COMMAND> [ARGS...]");
    println!();
    println!("COMMANDS:");
    println!("  status                                  Show vault configuration & lock status");
    println!("  init --password <PW> [--name <NAME>]    Initialize a new encrypted vault");
    println!("  vault list                              List all vault profiles");
    println!("  vault create <NAME>                     Create a new vault profile");
    println!("  vault delete <VAULT_ID>                 Delete a non-default vault profile");
    println!("  vault default <VAULT_ID>                Set the active default vault");
    println!("  item list [--category <CAT>] [--search <Q>]   List vault items");
    println!("  item get <ITEM_ID> [--reveal]           Inspect a vault item by ID");
    println!("  item add --title <T> [--username <U>] [--password <P>] [--url <URL>] [--totp <S>]");
    println!("  item trash <ITEM_ID>                    Move an item to trash");
    println!("  item restore <ITEM_ID>                  Restore an item from trash");
    println!("  item purge <ITEM_ID|__all_trash__>      Permanently delete item(s)");
    println!("  totp <SECRET_OR_OTPAUTH_URI> [--time <UNIX_SECS>]   Generate RFC 6238 TOTP code");
    println!("  password generate [--length <N>] [--no-symbols]     Generate a strong password");
    println!("  password strength <PASSWORD>            Evaluate password entropy & score");
    println!("  ssh keygen [--comment <COMMENT>]        Generate an OpenSSH Ed25519 keypair");
    println!("  export --format <svm|json|csv> --output <FILE> [--key <BACKUP_PW>]");
    println!("  import --format <svm|json|1pux|xml|csv> --input <FILE> [--key <BACKUP_PW>]");
    println!("  version                                 Print CLI version");
    println!("  help                                    Print this help message");
}

/// Tìm giá trị đi kèm của một cờ dòng lệnh (hỗ trợ cả `--flag val` và `--flag=val`).
pub fn find_flag_value(args: &[String], flag: &str) -> Option<String> {
    for i in 0..args.len() {
        if args[i] == flag {
            return args.get(i + 1).cloned();
        }
        if let Some(rest) = args[i].strip_prefix(&format!("{flag}=")) {
            return Some(rest.to_string());
        }
    }
    None
}

/// Kiểm tra xem một cờ boolean có xuất hiện trong danh sách đối số hay không.
pub fn has_flag(args: &[String], flag: &str) -> bool {
    args.iter().any(|a| a == flag)
}

/// Ánh xạ tên định dạng trên CLI sang `TransferFormat`.
pub fn parse_format_str(fmt: &str) -> TransferFormat {
    match fmt.trim().to_ascii_lowercase().as_str() {
        "svm" | "1" => TransferFormat::Svm,
        "json" | "bitwarden" | "2" => TransferFormat::BitwardenJson,
        "1pux" | "1password" | "3" => TransferFormat::OnePux,
        "xml" | "keepass" | "4" => TransferFormat::KeePassXml,
        _ => TransferFormat::Csv,
    }
}

/// Tách danh sách các đối số vị trí (positional arguments) bằng cách bỏ qua các cờ toàn cục.
pub fn extract_positionals(args: &[String]) -> Vec<String> {
    let mut positionals = Vec::new();
    let mut skip_next = false;
    for (idx, arg) in args.iter().enumerate() {
        if idx == 0 {
            continue;
        }
        if skip_next {
            skip_next = false;
            continue;
        }
        if matches!(
            arg.as_str(),
            "--data-dir"
                | "--password"
                | "--name"
                | "--category"
                | "--search"
                | "--title"
                | "--username"
                | "--user"
                | "--pass"
                | "--url"
                | "--totp"
                | "--time"
                | "--length"
                | "--comment"
                | "--format"
                | "--output"
                | "--input"
                | "--key"
        ) {
            skip_next = true;
            continue;
        }
        if arg.starts_with("--") {
            continue;
        }
        positionals.push(arg.clone());
    }
    positionals
}
