/**
 * @file WindowNode.cpp
 * @brief Implementation of WindowNode.
 */

#include "WindowNode.hpp"

#include "WindowManager.hpp"

#include <QApplication>
#include <QPropertyAnimation>

WindowNode::WindowNode(QWidget *widget, WindowNode *parent, bool isModal)
    : m_widget(widget), m_parent(parent), m_isModal(isModal) {
  if (m_widget) {
    m_windowHandle = m_widget->windowHandle();
  }
}

WindowNode::~WindowNode() { m_children.clear(); }

QWidget *WindowNode::widget() const { return m_widget.data(); }

QWindow *WindowNode::windowHandle() const {
  if (!m_windowHandle && m_widget) {
    const_cast<WindowNode *>(this)->m_windowHandle = m_widget->windowHandle();
  }
  return m_windowHandle.data();
}

void WindowNode::refreshWindowHandle() {
  if (m_widget) {
    m_windowHandle = m_widget->windowHandle();
  }
}

WindowNode *WindowNode::parent() const { return m_parent; }

void WindowNode::setParent(WindowNode *parent) { m_parent = parent; }

const std::vector<std::unique_ptr<WindowNode>> &WindowNode::children() const {
  return m_children;
}

bool WindowNode::isModal() const { return m_isModal; }

void WindowNode::setModal(bool modal) { m_isModal = modal; }

bool WindowNode::isVisible() const { return m_widget && m_widget->isVisible(); }

bool WindowNode::isLeaf() const {
  for (const auto &child : m_children) {
    if (child->isVisible()) {
      return false;
    }
  }
  return true;
}

WindowNode *WindowNode::findActiveLeaf() {
  // Traverse down children to find the deepest visible, active modal leaf
  for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
    if ((*it)->isVisible()) {
      return (*it)->findActiveLeaf();
    }
  }
  return this;
}

bool WindowNode::containsObject(QObject *object) const {
  if (!object) {
    return false;
  }

  // 1. Direct widget match
  if (m_widget && m_widget.data() == object) {
    return true;
  }

  // 2. Direct QWindow handle match
  QWindow *handle = windowHandle();
  if (handle && handle == object) {
    return true;
  }

  // 3. Child widget strictly belonging to this top-level window (never across
  // window boundaries)
  if (m_widget) {
    if (auto *w = qobject_cast<QWidget *>(object)) {
      if (w->window() == m_widget.data()) {
        return true;
      }
    }
  }

  // 4. Native subwindow handle
  if (handle) {
    if (auto *win = qobject_cast<QWindow *>(object)) {
      if (win == handle) {
        return true;
      }
    }
  }

  return false;
}

bool WindowNode::hasModalDescendant() const {
  for (const auto &child : m_children) {
    if (child->isVisible() &&
        (child->isModal() || child->hasModalDescendant())) {
      return true;
    }
  }
  return false;
}

WindowNode *WindowNode::findNodeForObject(QObject *object) {
  // Traverse child nodes first (deepest/leaf window takes precedence)
  for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
    WindowNode *found = (*it)->findNodeForObject(object);
    if (found) {
      return found;
    }
  }

  if (containsObject(object)) {
    return this;
  }

  return nullptr;
}

void WindowNode::addChild(std::unique_ptr<WindowNode> child) {
  if (child) {
    child->setParent(this);
    m_children.push_back(std::move(child));
  }
}

std::unique_ptr<WindowNode> WindowNode::removeChild(QWidget *widget) {
  if (!widget) {
    return nullptr;
  }
  for (auto it = m_children.begin(); it != m_children.end(); ++it) {
    if ((*it)->widget() == widget) {
      std::unique_ptr<WindowNode> removed = std::move(*it);
      m_children.erase(it);
      removed->setParent(nullptr);
      return removed;
    }
  }
  for (const auto &child : m_children) {
    auto removed = child->removeChild(widget);
    if (removed) {
      return removed;
    }
  }
  return nullptr;
}

void WindowNode::requestAttention() {
  if (m_widget) {
    WindowManager::instance().shakeWindow(m_widget.data());
  }
}
