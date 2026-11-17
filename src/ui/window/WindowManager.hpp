/**
 * @file WindowManager.hpp
 * @brief Singleton lifecycle and event propagation manager for the application
 * window tree.
 */

#pragma once

#include "WindowNode.hpp"

#include <QApplication>
#include <QDateTime>
#include <QObject>
#include <QWidget>
#include <memory>

/**
 * @class WindowManager
 * @brief Central manager tracking window lifecycle, hierarchical tree
 * structure, and event propagation.
 *
 * Enforces strict single root window lifecycle, maintains parent-child window
 * relationships, and intercepts window events to propagate warnings down to the
 * active modal leaf window.
 */
class WindowManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Accesses the global singleton instance.
   */
  static WindowManager &instance();

  /**
   * @brief Initializes the window manager and installs the global application
   * event filter.
   * @param app Application instance.
   */
  void init(QApplication *app);

  /**
   * @brief Registers the primary top-level window as the root of the window
   * tree.
   * @param mainWindow Pointer to MainWindow.
   */
  void registerRootWindow(QWidget *mainWindow);

  /**
   * @brief Registers a child window or dialog into the window tree.
   * @param window Target window to register.
   * @param parent Optional parent window (defaults to root if nullptr).
   * @param isModal True if the dialog should block ancestor windows.
   * @return Pointer to the newly created WindowNode.
   */
  WindowNode *registerWindow(QWidget *window, QWidget *parent = nullptr,
                             bool isModal = false);

  /**
   * @brief Unregisters a window from the tree upon destruction or closing.
   * @param window Target window.
   */
  void unregisterWindow(QWidget *window);

  /**
   * @brief Returns the root window node.
   */
  WindowNode *rootNode() const;

  /**
   * @brief Returns the managed root QWidget window.
   */
  QWidget *rootWindow() const;

  /**
   * @brief Returns the current active leaf window node in the tree.
   */
  WindowNode *activeLeafNode() const;

  /**
   * @brief Finds the WindowNode owning or containing the specified QObject.
   * @param object Target widget or window object.
   * @return Matching WindowNode or nullptr.
   */
  WindowNode *findNode(QObject *object) const;

  /**
   * @brief Checks if the given window has active modal children blocking it.
   * @param window Window to test.
   */
  bool isBlockedByModal(QWidget *window) const;

  /**
   * @brief Activates the current active leaf window, or restores and raises
   * root window if none.
   */
  void activateLeafOrRoot();

  /**
   * @brief Applies dynamic horizontal shake animation to the target window.
   * @param window Target window to shake.
   */
  void shakeWindow(QWidget *window);

  /**
   * @brief Notifies WindowManager that a window has been shown or activated,
   * refreshing settling timestamps to suppress false modal alert shakes.
   * @param window The window that was shown or activated.
   */
  void notifyWindowActivated(QWidget *window = nullptr);

signals:
  /**
   * @brief Emitted when a new window node is registered.
   */
  void windowRegistered(QWidget *window);

  /**
   * @brief Emitted when a window node is detached and unregistered.
   */
  void windowUnregistered(QWidget *window);

  /**
   * @brief Emitted when an interaction on a blocked parent is caught and
   * redirected to the leaf.
   */
  void modalBlockedInteraction(QWidget *blockedWindow,
                               QWidget *activeLeafWindow);

protected:
  /**
   * @brief Global event filter intercepting user interactions and activations
   * on blocked ancestors.
   * @param watched Target QObject receiving the event.
   * @param event Incoming event.
   * @return True to swallow the event, false to pass through.
   */
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  WindowManager();
  ~WindowManager() override = default;

  WindowManager(const WindowManager &) = delete;
  WindowManager &operator=(const WindowManager &) = delete;

  std::unique_ptr<WindowNode> m_rootNode;
  QApplication *m_app{nullptr};
  qint64 m_lastAlertTime{0};
  qint64 m_leafActivationTime{0};
};
