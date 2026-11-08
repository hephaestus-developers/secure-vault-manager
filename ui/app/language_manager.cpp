/**
 * @file ui/app/language_manager.cpp
 * @brief Hiện thực LanguageTranslator và LanguageManager nạp i18n từ JSON.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "language_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace svm::app {

// =============================================================================
// LanguageTranslator
// =============================================================================

LanguageTranslator::LanguageTranslator(QObject *parent) : QTranslator(parent) {}

bool LanguageTranslator::loadLanguage(const QString &filePathOrResource) {
  QFile file(filePathOrResource);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "[i18n] Không thể mở tệp ngôn ngữ:" << filePathOrResource;
    return false;
  }

  QJsonParseError error{};
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    qWarning() << "[i18n] Lỗi cú pháp JSON trong tệp:" << filePathOrResource
               << error.errorString();
    return false;
  }

  m_translations.clear();
  const QJsonObject root = doc.object();
  for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
    m_translations.insert(it.key(), it.value().toString());
  }

  return true;
}

QString LanguageTranslator::translate(const char *context,
                                      const char *sourceText,
                                      const char *disambiguation, int n) const {
  Q_UNUSED(context);
  Q_UNUSED(disambiguation);
  Q_UNUSED(n);

  if (!sourceText) {
    return QString();
  }

  const QString key = QString::fromUtf8(sourceText);
  auto it = m_translations.constFind(key);
  if (it != m_translations.constEnd()) {
    return it.value();
  }

  return QString();
}

bool LanguageTranslator::isEmpty() const { return m_translations.isEmpty(); }

// =============================================================================
// LanguageManager
// =============================================================================

LanguageManager &LanguageManager::instance() {
  static LanguageManager s_instance;
  return s_instance;
}

LanguageManager::LanguageManager(QObject *parent) : QObject(parent) {
  m_translator = new LanguageTranslator(this);
}

void LanguageManager::initialize() {
  QSettings settings;
  QString savedCode =
      settings.value(QStringLiteral("General/Language")).toString();
  if (savedCode.isEmpty()) {
    QSettings iniSettings(QSettings::IniFormat, QSettings::UserScope,
                          QStringLiteral("Hephaestus"),
                          QStringLiteral("SecureVaultManager"));
    const int val =
        iniSettings.value(QStringLiteral("general/language"), -1).toInt();
    if (val == static_cast<int>(AppLanguage::Vietnamese)) {
      savedCode = QStringLiteral("vi");
    } else if (val == static_cast<int>(AppLanguage::English)) {
      savedCode = QStringLiteral("en");
    } else {
      savedCode = QStringLiteral("vi");
    }
  }
  setLanguage(languageFromCode(savedCode));
}

void LanguageManager::setLanguage(AppLanguage lang) {
  m_currentLanguage = lang;

  if (m_translator) {
    QCoreApplication::removeTranslator(m_translator);
  }

  const QString qrcPath = (lang == AppLanguage::Vietnamese)
                              ? QStringLiteral(":/i18n/vi.json")
                              : QStringLiteral(":/i18n/en.json");

  if (m_translator->loadLanguage(qrcPath)) {
    QCoreApplication::installTranslator(m_translator);
  }

  QSettings settings;
  settings.setValue(QStringLiteral("General/Language"), languageCode());

  QSettings iniSettings(QSettings::IniFormat, QSettings::UserScope,
                        QStringLiteral("Hephaestus"),
                        QStringLiteral("SecureVaultManager"));
  iniSettings.setValue(QStringLiteral("general/language"),
                       static_cast<int>(lang));

  emit languageChanged(lang);
}

AppLanguage LanguageManager::currentLanguage() const {
  return m_currentLanguage;
}

QString LanguageManager::languageCode() const {
  switch (m_currentLanguage) {
  case AppLanguage::Vietnamese:
    return QStringLiteral("vi");
  case AppLanguage::English:
  default:
    return QStringLiteral("en");
  }
}

AppLanguage LanguageManager::languageFromCode(const QString &code) {
  if (code.compare(QStringLiteral("vi"), Qt::CaseInsensitive) == 0) {
    return AppLanguage::Vietnamese;
  }
  return AppLanguage::English;
}

QString LanguageManager::languageName(AppLanguage lang) {
  switch (lang) {
  case AppLanguage::Vietnamese:
    return QStringLiteral("Tiếng Việt");
  case AppLanguage::English:
  default:
    return QStringLiteral("English");
  }
}

} // namespace svm::app
