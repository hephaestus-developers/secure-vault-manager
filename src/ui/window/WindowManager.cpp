/**
 * @file WindowManager.cpp
 * @brief Implementation of WindowManager.
 */

#include "WindowManager.hpp"

#include "core/LogCategories.hpp"

#include <QDateTime>
#include <QEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVariant>
#include <QWindow>

WindowManager &WindowManager::instance() {
  static WindowManager s_instance;
  return s_instance;
}

WindowManager::WindowManager() = default;

void WindowManager::init(QApplication *app) {
  if (m_app != app && app) {
    if (m_app) {
      m_app->removeEventFilter(this);
    }
    m_app = app;
    m_app->installEventFilter(this);
  }
}

void WindowManager::registerRootWindow(QWidget *mainWindow) {
  if (!mainWindow) {
    return;
  }
  m_rootNode = std::make_unique<WindowNode>(mainWindow, nullptr, false);
  emit windowRegistered(mainWindow);
}

WindowNode *WindowManager::registerWindow(QWidget *window, QWidget *parent,
                                          bool isModal) {
  if (!window) {
    return nullptr;
  }

  WindowNode *parentNode = nullptr;
  if (parent) {
    parentNode = findNode(parent);
  }
  if (!parentNode) {
    parentNode = m_rootNode.get();
  }

  auto childNode = std::make_unique<WindowNode>(window, parentNode, isModal);
  WindowNode *rawPtr = childNode.get();

  if (parentNode) {
    parentNode->addChild(std::move(childNode));
  } else {
    m_rootNode = std::move(childNode);
  }

  if (isModal) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_leafActivationTime = now;
    m_lastAlertTime = now;
  }

  // Connect auto-unregister on widget deletion
  connect(window, &QObject::destroyed, this,
          [this, window]() { unregisterWindow(window); });

  qCDebug(logWindow) << "Registered window node:" << window
                     << "modal:" << isModal;
  emit windowRegistered(window);
  return rawPtr;
}

void WindowManager::unregisterWindow(QWidget *window) {
  if (!window || !m_rootNode) {
    return;
  }
  qCDebug(logWindow) << "Unregistering window node:" << window;
  if (m_rootNode->widget() == window) {
    m_rootNode.reset();
    emit windowUnregistered(window);
    return;
  }

  auto removed = m_rootNode->removeChild(window);
  if (removed) {
    emit windowUnregistered(window);
  }
}

WindowNode *WindowManager::rootNode() const { return m_rootNode.get(); }

QWidget *WindowManager::rootWindow() const {
  return m_rootNode ? m_rootNode->widget() : nullptr;
}

WindowNode *WindowManager::activeLeafNode() const {
  if (!m_rootNode) {
    return nullptr;
  }
  return m_rootNode->findActiveLeaf();
}

WindowNode *WindowManager::findNode(QObject *object) const {
  if (!m_rootNode || !object) {
    return nullptr;
  }
  return m_rootNode->findNodeForObject(object);
}

bool WindowManager::isBlockedByModal(QWidget *window) const {
  WindowNode *node = findNode(window);
  return node ? node->hasModalDescendant() : false;
}

void WindowManager::activateLeafOrRoot() {
  WindowNode *leaf = activeLeafNode();
  if (leaf && leaf != m_rootNode.get() && leaf->isVisible()) {
    leaf->requestAttention();
    return;
  }

  if (QWidget *root = rootWindow()) {
    if (root->isMinimized()) {
      root->showNormal();
    }
    root->raise();
    root->activateWindow();
  }
}

void WindowManager::shakeWindow(QWidget *window) {
  if (!window || !window->isVisible()) {
    return;
  }

  qCDebug(logWindow) << "Executing visual shake attention on window:" << window;
  window->raise();

  // Prevent stacking duplicate shake animations
  if (window->property("isShaking").toBool()) {
    return;
  }
  window->setProperty("isShaking", true);

  // Prefer dedicated shakeOffset property (smooth layout margin animation
  // without repainting artifacts)
  if (window->metaObject()->indexOfProperty("shakeOffset") >= 0) {
    auto *anim = new QPropertyAnimation(window, "shakeOffset", window);
    anim->setDuration(360);
    anim->setKeyValueAt(0.0, 0);
    anim->setKeyValueAt(0.12, -8);
    anim->setKeyValueAt(0.28, 8);
    anim->setKeyValueAt(0.44, -5);
    anim->setKeyValueAt(0.60, 5);
    anim->setKeyValueAt(0.76, -2);
    anim->setKeyValueAt(0.88, 2);
    anim->setKeyValueAt(1.0, 0);

    QPointer<QWidget> targetWindow(window);
    connect(anim, &QPropertyAnimation::finished, [targetWindow]() {
      if (targetWindow) {
        targetWindow->setProperty("shakeOffset", 0);
        targetWindow->setProperty("isShaking", false);
      }
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
    return;
  }

  // Fallback for widgets without shakeOffset
  QWidget *content = nullptr;
  if (auto *mainWin = qobject_cast<QMainWindow *>(window)) {
    content = mainWin->centralWidget();
  } else {
    for (QObject *child : window->children()) {
      if (auto *w = qobject_cast<QWidget *>(child)) {
        if (!w->isWindow() && w->isVisible()) {
          content = w;
          break;
        }
      }
    }
  }

  if (!content) {
    content = window;
  }

  const QPoint origPos = content->pos();
  auto *anim = new QPropertyAnimation(content, "pos", content);
  anim->setDuration(340);
  anim->setKeyValueAt(0.0, origPos);
  anim->setKeyValueAt(0.12, QPoint(origPos.x() - 8, origPos.y()));
  anim->setKeyValueAt(0.28, QPoint(origPos.x() + 8, origPos.y()));
  anim->setKeyValueAt(0.44, QPoint(origPos.x() - 5, origPos.y()));
  anim->setKeyValueAt(0.60, QPoint(origPos.x() + 5, origPos.y()));
  anim->setKeyValueAt(0.76, QPoint(origPos.x() - 2, origPos.y()));
  anim->setKeyValueAt(0.88, QPoint(origPos.x() + 2, origPos.y()));
  anim->setKeyValueAt(1.0, origPos);

  QPointer<QWidget> targetContent(content);
  QPointer<QWidget> targetWindow(window);
  connect(anim, &QPropertyAnimation::finished,
          [targetContent, targetWindow, origPos]() {
            if (targetContent) {
              targetContent->move(origPos);
            }
            if (targetWindow) {
              targetWindow->setProperty("isShaking", false);
            }
          });

  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void WindowManager::notifyWindowActivated(QWidget *window) {
  Q_UNUSED(window);
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  m_leafActivationTime = now;
  m_lastAlertTime = now;
}

bool WindowManager::eventFilter(QObject *watched, QEvent *event) {
  const QEvent::Type type = event->type();

  // Check events that signify interaction attempts or window focus shifts
  const bool isInteraction =
      (type == QEvent::MouseButtonPress ||
       type == QEvent::MouseButtonDblClick ||
       type == QEvent::NonClientAreaMouseButtonPress || type == QEvent::Wheel);

  const bool isActivation = (type == QEvent::WindowActivate);

  if (isInteraction || isActivation) {
    WindowNode *targetNode = findNode(watched);

    // If target node is found and has an active modal descendant
    if (targetNode && targetNode->hasModalDescendant()) {
      WindowNode *leaf = targetNode->findActiveLeaf();
      if (leaf && leaf != targetNode && leaf->isVisible()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        // Grace period: ignore all interactions and activations within 500ms of
        // dialog appearance to prevent false alert shakes caused by the opening
        // click or window focus handoff
        if (now - m_leafActivationTime < 500) {
          return isInteraction; // Silently consume lingering mouse clicks from
                                // opening
        }

        // Only alert if enough time has passed since last alert (debounce
        // 500ms)
        if (now - m_lastAlertTime > 500) {
          m_lastAlertTime = now;
          leaf->requestAttention();
          emit modalBlockedInteraction(targetNode->widget(), leaf->widget());
        }

        if (isActivation) {
          if (QWidget *leafWidget = leaf->widget()) {
            leafWidget->raise();
            leafWidget->activateWindow();
          }
        }

        return true; // Consume event to prevent ancestor interaction
      }
    }
  }

  return QObject::eventFilter(watched, event);
}
