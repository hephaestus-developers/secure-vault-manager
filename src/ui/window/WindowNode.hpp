/**
 * @file WindowNode.hpp
 * @brief Hierarchical tree node representing a window and its modal descendant
 * relationships.
 */

#pragma once

#include <QObject>
#include <QPointer>
#include <QWidget>
#include <QWindow>
#include <memory>
#include <vector>

/**
 * @class WindowNode
 * @brief Node in the hierarchical window tree managing parent-child relations
 * and leaf discovery.
 *
 * Tracks individual windows, their native window surfaces (QWindow), modal
 * flags, and maintains parent-child links for event propagation down to the
 * topmost active leaf window.
 */
class WindowNode {
public:
  /**
   * @brief Constructs a window node.
   * @param widget Target QWidget window or dialog.
   * @param parent Optional parent node in the window tree.
   * @param isModal True if this window is modal.
   */
  explicit WindowNode(QWidget *widget, WindowNode *parent = nullptr,
                      bool isModal = false);

  /**
   * @brief Destructor. Cleans up child nodes.
   */
  ~WindowNode();

  /**
   * @brief Returns the managed QWidget pointer (may be null if destroyed).
   */
  QWidget *widget() const;

  /**
   * @brief Returns the underlying native QWindow handle.
   */
  QWindow *windowHandle() const;

  /**
   * @brief Refreshes the cached native QWindow handle if created
   * post-construction.
   */
  void refreshWindowHandle();

  /**
   * @brief Returns the parent node.
   */
  WindowNode *parent() const;

  /**
   * @brief Sets the parent node.
   * @param parent Pointer to parent WindowNode.
   */
  void setParent(WindowNode *parent);

  /**
   * @brief Returns the collection of child window nodes.
   */
  const std::vector<std::unique_ptr<WindowNode>> &children() const;

  /**
   * @brief Indicates whether this window is modal.
   */
  bool isModal() const;

  /**
   * @brief Sets modal state of this window node.
   * @param modal True for modal windows.
   */
  void setModal(bool modal);

  /**
   * @brief Checks if the managed widget is visible.
   */
  bool isVisible() const;

  /**
   * @brief Checks if this node is a leaf (has no visible child nodes).
   */
  bool isLeaf() const;

  /**
   * @brief Finds the active/deepest visible leaf node in this branch.
   * @return Deepest visible modal or active leaf node, or this if leaf.
   */
  WindowNode *findActiveLeaf();

  /**
   * @brief Checks if an observed QObject belongs to this window or any of its
   * internal children.
   * @param object Target QObject to check.
   * @return True if object is this widget, this window handle, or a descendant
   * widget.
   */
  bool containsObject(QObject *object) const;

  /**
   * @brief Checks if any descendant in the subtree rooted at this node is
   * visible and modal.
   */
  bool hasModalDescendant() const;

  /**
   * @brief Recursively searches the subtree for a node matching the target
   * object.
   * @param object Target QObject.
   * @return Matching WindowNode or nullptr.
   */
  WindowNode *findNodeForObject(QObject *object);

  /**
   * @brief Adds a child window node.
   * @param child Unique pointer to child WindowNode.
   */
  void addChild(std::unique_ptr<WindowNode> child);

  /**
   * @brief Removes a child node matching the widget pointer.
   * @param widget Target widget to detach.
   * @return Removed unique pointer (transfers ownership), or nullptr.
   */
  std::unique_ptr<WindowNode> removeChild(QWidget *widget);

  /**
   * @brief Propagates an attention request to this window (executes shake
   * animation).
   */
  void requestAttention();

private:
  QPointer<QWidget> m_widget;
  QPointer<QWindow> m_windowHandle;
  WindowNode *m_parent{nullptr};
  std::vector<std::unique_ptr<WindowNode>> m_children;
  bool m_isModal{false};
};
