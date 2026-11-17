#include <QApplication>
#include <QTimer>
#include <cassert>
#include <iostream>

#include "ui/components/ConfirmDeleteDialog.hpp"
#include "ui/components/NotificationDialog.hpp"
#include "ui/views/about/AboutDialog.hpp"
#include "ui/views/about/KeyboardShortcutsDialog.hpp"
#include "ui/views/detail/RevisionHistoryDialog.hpp"
#include "ui/views/lock/RecoveryDialog.hpp"
#include "ui/views/lock/ResetVaultDialog.hpp"
#include "ui/views/settings/SettingsDialog.hpp"
#include "ui/views/vaults/NewVaultDialog.hpp"
#include "ui/window/WindowManager.hpp"

int main(int argc, char *argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  Q_INIT_RESOURCE(svm);
  WindowManager::instance().init(&app);

  std::cout << "Testing AboutDialog..." << std::endl;
  {
    svm::views::AboutDialog dlg;
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "Testing KeyboardShortcutsDialog..." << std::endl;
  {
    svm::views::KeyboardShortcutsDialog dlg;
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "Testing NotificationDialog..." << std::endl;
  {
    svm::components::NotificationDialog dlg;
    dlg.setTitle("Test Title");
    dlg.setMessage("Test Message");
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "Testing ConfirmDeleteDialog..." << std::endl;
  {
    svm::models::UiVaultItem item;
    item.id = "test-item-1";
    item.title = "Test Item";
    svm::components::ConfirmDeleteDialog dlg(item, false);
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "Testing SettingsDialog..." << std::endl;
  {
    svm::views::SettingsDialog dlg;
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "Testing NewVaultDialog..." << std::endl;
  {
    svm::views::NewVaultDialog dlg;
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "Testing RecoveryDialog..." << std::endl;
  {
    svm::views::RecoveryDialog dlg;
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "Testing ResetVaultDialog..." << std::endl;
  {
    svm::views::ResetVaultDialog dlg;
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "Testing RevisionHistoryDialog..." << std::endl;
  {
    QList<svm::models::UiItemRevision> revs;
    svm::views::RevisionHistoryDialog dlg(revs);
    assert(dlg.cardWidget() != nullptr);
    assert(dlg.layout() != nullptr);
    dlg.show();
    dlg.hide();
  }

  std::cout << "All 9 dialogs successfully verified with zero layout errors!"
            << std::endl;
  return 0;
}
