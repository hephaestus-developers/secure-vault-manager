/**
 * @file ui/app/language_manager.h
 * @brief Quản lý đa ngôn ngữ (i18n) cho ứng dụng thông qua LanguageTranslator
 * (QTranslator) và tệp JSON.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTranslator>

namespace svm::app {

enum class AppLanguage { English, Vietnamese };

/**
 * @brief QTranslator tùy biến tải bản dịch từ tệp JSON chuẩn Notera Editor.
 */
class LanguageTranslator : public QTranslator {
  Q_OBJECT

public:
  explicit LanguageTranslator(QObject *parent = nullptr);
  ~LanguageTranslator() override = default;

  bool loadLanguage(const QString &filePathOrResource);

  [[nodiscard]] QString translate(const char *context, const char *sourceText,
                                  const char *disambiguation = nullptr,
                                  int n = -1) const override;

  [[nodiscard]] bool isEmpty() const override;

private:
  QHash<QString, QString> m_translations;
};

/**
 * @brief Quản lý ngôn ngữ toàn cục, hỗ trợ chuyển đổi ngôn ngữ động và lưu cấu
 * hình.
 */
class LanguageManager : public QObject {
  Q_OBJECT

public:
  static LanguageManager &instance();

  ~LanguageManager() override = default;
  LanguageManager(const LanguageManager &) = delete;
  LanguageManager &operator=(const LanguageManager &) = delete;

  /**
   * @brief Khởi tạo hệ thống dịch, tải ngôn ngữ từ cấu hình (mặc định English).
   */
  void initialize();

  /**
   * @brief Đặt ngôn ngữ hiện tại của ứng dụng.
   */
  void setLanguage(AppLanguage lang);

  /**
   * @brief Lấy ngôn ngữ hiện tại.
   */
  [[nodiscard]] AppLanguage currentLanguage() const;

  /**
   * @brief Mã ngôn ngữ dạng chuỗi (vd: "en", "vi").
   */
  [[nodiscard]] QString languageCode() const;

  /**
   * @brief Chuyển đổi mã chuỗi sang AppLanguage.
   */
  [[nodiscard]] static AppLanguage languageFromCode(const QString &code);

  /**
   * @brief Tên hiển thị thân thiện của ngôn ngữ.
   */
  [[nodiscard]] static QString languageName(AppLanguage lang);

signals:
  void languageChanged(AppLanguage lang);

private:
  explicit LanguageManager(QObject *parent = nullptr);

  AppLanguage m_currentLanguage{AppLanguage::English};
  LanguageTranslator *m_translator{nullptr};
};

} // namespace svm::app
