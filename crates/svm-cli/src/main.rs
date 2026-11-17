//! # svm-cli
//!
//! Công cụ dòng lệnh không giao diện (Headless CLI) dành cho quản trị viên hệ thống,
//! kịch bản tự động hóa DevOps và tích hợp CI/CD của Secure Vault Manager.

pub mod args;
pub mod commands;

use std::env;
use std::process::ExitCode;

pub use commands::execute_cli;

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    match execute_cli(&args) {
        Ok(output) => {
            if !output.is_empty() {
                println!("{output}");
            }
            ExitCode::SUCCESS
        }
        Err(err) => {
            eprintln!("Error: {err}");
            ExitCode::FAILURE
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn vec_args(slice: &[&str]) -> Vec<String> {
        slice.iter().map(|s| s.to_string()).collect()
    }

    #[test]
    fn test_cli_totp_password_and_ssh() {
        let totp_out = execute_cli(&vec_args(&[
            "svm-cli",
            "totp",
            "JBSWY3DPEHPK3PXP",
            "--time",
            "1700000000",
        ]))
        .expect("totp");
        assert!(totp_out.contains(r#""code":"#));
        assert!(totp_out.contains(r#""remaining":10"#));

        let gen_out = execute_cli(&vec_args(&[
            "svm-cli", "password", "generate", "--length", "24",
        ]))
        .expect("password generate");
        assert!(gen_out.contains(r#""password":"#));

        let ssh_out = execute_cli(&vec_args(&[
            "svm-cli",
            "ssh",
            "keygen",
            "--comment",
            "ci@hephaestus.io",
        ]))
        .expect("ssh keygen");
        assert!(ssh_out.contains("ssh-ed25519"));
        assert!(ssh_out.contains("SHA256:"));
    }

    #[test]
    fn test_cli_vault_init_and_item_crud() {
        let dir = "/tmp/svm-cli-unit-test";
        let _ = std::fs::remove_dir_all(dir);

        let init_out = execute_cli(&vec_args(&[
            "svm-cli",
            "--data-dir",
            dir,
            "--password",
            "CliTestPass!2026",
            "init",
            "--name",
            "DevOps Vault",
        ]))
        .expect("init");
        assert!(init_out.contains(r#""initialized":true"#));

        let add_out = execute_cli(&vec_args(&[
            "svm-cli",
            "--data-dir",
            dir,
            "--password",
            "CliTestPass!2026",
            "item",
            "add",
            "--title",
            "GitLab Production",
            "--username",
            "root",
            "--pass",
            "SuperSecret!99",
            "--url",
            "https://gitlab.example.com",
        ]))
        .expect("item add");
        assert!(add_out.contains(r#""saved":true"#));

        let list_out = execute_cli(&vec_args(&[
            "svm-cli",
            "--data-dir",
            dir,
            "--password",
            "CliTestPass!2026",
            "item",
            "list",
            "--search",
            "gitlab",
        ]))
        .expect("item list");
        assert!(list_out.contains("GitLab Production"));

        let _ = std::fs::remove_dir_all(dir);
    }
}
