#pragma once

#include <vector>
#include <algorithm>

#include "source/common/common/assert.h"

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace Envoy {
 template <class Value> class RadixTree {
  static constexpr int32_t NoNode = -1;
  struct RadixTreeNode {
    absl::string_view prefix;
    Value value_{};
    // Hash map for O(1) child lookup by first character
    absl::flat_hash_map<uint8_t, RadixTreeNode> children_;
  };

  /**
   * Get the child node for the given character key.
   * @param node the node to get the child from.
   * @param char_key the one-byte key of the child to get.
   * @return pointer to the child node, or nullptr if not found.
   */
  const RadixTreeNode* getChild(const RadixTreeNode& node, uint8_t char_key) const {
    auto it = node.children_.find(char_key);
    if (it != node.children_.end()) {
      return &it->second;
    }
    return nullptr;
  }

  RadixTreeNode* getChild(RadixTreeNode& node, uint8_t char_key) {
    auto it = node.children_.find(char_key);
    if (it != node.children_.end()) {
      return &it->second;
    }
    return nullptr;
  }

  /**
   * Check if a node has a value (is a leaf node)
   */
  bool hasValue(const RadixTreeNode& node) const {
    // For pointer types, check if the pointer is not null
    if constexpr (std::is_pointer_v<Value>) {
      return node.value_ != nullptr;
    } else {
      return static_cast<bool>(node.value_);
    }
  }

  /**
   * Find the longest common prefix between two strings
   */
  size_t longestPrefix(absl::string_view a, absl::string_view b) const {
    size_t len = std::min(a.size(), b.size());
    for (size_t i = 0; i < len; i++) {
      if (a[i] != b[i]) {
        return i;
      }
    }
    return len;
  }

  /**
   * Insert a key-value pair into the radix tree
   * @param node the current node to insert into
   * @param key the full key being inserted
   * @param search the remaining search key
   * @param value the value to insert
   * @return tuple of (new_node, old_value, did_update)
   */
  std::tuple<RadixTreeNode*, Value, bool> insert(RadixTreeNode* node, 
                                                 absl::string_view key,
                                                 absl::string_view search, 
                                                 Value value) {
    // Handle key exhaustion
    if (search.empty()) {
      Value oldVal{};
      bool didUpdate = false;
      if (hasValue(*node)) {
        oldVal = node->value_;
        didUpdate = true;
      }
      
      node->value_ = std::move(value);
      return {node, oldVal, didUpdate};
    }

    // Look for the edge
    uint8_t firstChar = static_cast<uint8_t>(search[0]);
    auto childIt = node->children_.find(firstChar);

    // No edge, create one
    if (childIt == node->children_.end()) {
      // Create a new child node
      RadixTreeNode newChild;
      newChild.prefix = search;
      newChild.value_ = std::move(value);
      
      // Add the child to the current node
      node->children_[firstChar] = std::move(newChild);
      
      return {node, Value{}, false};
    }

    // Get the child node
    RadixTreeNode& child = childIt->second;

    // Determine longest prefix of the search key on match
    size_t commonPrefix = longestPrefix(search, child.prefix);
    if (commonPrefix == child.prefix.size()) {
      // The search key is longer than the child prefix, continue down
      absl::string_view remainingSearch = search.substr(commonPrefix);
      auto [newChild, oldVal, didUpdate] = insert(&child, key, remainingSearch, std::move(value));
      if (newChild != nullptr) {
        return {node, oldVal, didUpdate};
      }
      return {nullptr, oldVal, didUpdate};
    }

    // Split the node - create a new intermediate node
    RadixTreeNode splitNode;
    splitNode.prefix = search.substr(0, commonPrefix);
    
    // Update the child's prefix
    child.prefix = child.prefix.substr(commonPrefix);
    
    // Create a new leaf for the current key
    RadixTreeNode newLeaf;
    newLeaf.prefix = search.substr(commonPrefix);
    newLeaf.value_ = std::move(value);
    
    // Add both children to the split node
    splitNode.children_[static_cast<uint8_t>(child.prefix[0])] = std::move(child);
    splitNode.children_[static_cast<uint8_t>(newLeaf.prefix[0])] = std::move(newLeaf);
    
    // Replace the original child with the split node
    node->children_[firstChar] = std::move(splitNode);
    
    return {node, Value{}, false};
  }

public:
  /**
   * Adds an entry to the RadixTree at the given Key.
   * @param key the key used to add the entry.
   * @param value the value to be associated with the key.
   * @param overwrite_existing will overwrite the value when the value for a given key already
   * exists.
   * @return false when a value already exists for the given key.
   */
  bool add(absl::string_view key, Value value, bool overwrite_existing = true) {
    // Check if the key already exists
    Value existing = find(key);
    
    // If a value exists and we shouldn't overwrite, return false
    if (static_cast<bool>(existing) && !overwrite_existing) {
      return false;
    }
    
    insert(&root_, key, key, std::move(value));
    return true;
  }

  /**
   * Finds the entry associated with the key.
   * @param key the key used to find.
   * @param result the value associated with the key (only set if found).
   * @return true if the key was found, false otherwise.
   */
  bool find(absl::string_view key, Value& result) const {
    return findRecursive(&root_, key, result);
  }

  /**
   * Finds the entry associated with the key.
   * @param key the key used to find.
   * @return the Value associated with the key, or an empty-initialized Value
   *         if there is no matching key.
   */
  Value find(absl::string_view key) const {
    Value result;
    if (find(key, result)) {
      return result;
    }
    return Value{};
  }

  /**
   * Returns the set of entries that are prefixes of the specified key, longest last.
   * Complexity is O(min(longest key prefix, key length)).
   * @param key the key used to find.
   * @return a vector of values whose keys are a prefix of the specified key, longest last.
   */
  absl::InlinedVector<Value, 4> findMatchingPrefixes(absl::string_view key) const {
    absl::InlinedVector<Value, 4> result;
    absl::string_view search = key;
    const RadixTreeNode* node = &root_;
    bool consumed_prefix = false;

    while (true) {
      // Check if current node has a value (is a leaf) and we've consumed some prefix
      if (hasValue(*node) && consumed_prefix) {
        result.push_back(node->value_);
      }

      // Check for key exhaustion
      if (search.empty()) {
        break;
      }

      // Look for an edge
      uint8_t firstChar = static_cast<uint8_t>(search[0]);
      auto childIt = node->children_.find(firstChar);
      if (childIt == node->children_.end()) {
        break;
      }

      const RadixTreeNode& child = childIt->second;
      node = &child;

      // Consume the search prefix
      if (search.size() >= child.prefix.size() && 
          search.substr(0, child.prefix.size()) == child.prefix) {
        search = search.substr(child.prefix.size());
        consumed_prefix = true;
      } else {
        break;
      }
    }

    return result;
  }

  /**
   * Finds the entry with the longest key that is a prefix of the specified key.
   * Complexity is O(min(longest key prefix, key length)).
   * @param key the key used to find.
   * @return a value whose key is a prefix of the specified key. If there are
   *         multiple such values, the one with the longest key. If there are
   *         no keys that are a prefix of the input key, an empty-initialized Value.
   */
  Value findLongestPrefix(absl::string_view key) const {
    absl::string_view search = key;
    const RadixTreeNode* node = &root_;
    const RadixTreeNode* last_node_with_value = nullptr;
    bool consumed_prefix = false;

    while (true) {
      // Check if current node has a value (is a leaf) and we've consumed some prefix
      if (hasValue(*node) && consumed_prefix) {
        last_node_with_value = node;
      }

      // Check for key exhaustion
      if (search.empty()) {
        break;
      }

      // Look for an edge
      uint8_t firstChar = static_cast<uint8_t>(search[0]);
      auto childIt = node->children_.find(firstChar);
      if (childIt == node->children_.end()) {
        break;
      }

      const RadixTreeNode& child = childIt->second;
      node = &child;

      // Consume the search prefix
      if (search.size() >= child.prefix.size() && 
          search.substr(0, child.prefix.size()) == child.prefix) {
        search = search.substr(child.prefix.size());
        consumed_prefix = true;
      } else {
        break;
      }
    }

    // Return the value from the last node that had a value, or empty value if none found
    if (last_node_with_value != nullptr) {
      return last_node_with_value->value_;
    }
    return Value{};
  }

private:
  // Initialized with a single empty node as the root node.
  RadixTreeNode root_ = RadixTreeNode();

  /**
   * Recursive helper for find operation.
   * @param node the current node to search from.
   * @param search the remaining search key.
   * @param result the value to return if found.
   * @return true if the key was found, false otherwise.
   */
  bool findRecursive(const RadixTreeNode* node, absl::string_view search, Value& result) const {
    if (search.empty()) {
      if (hasValue(*node)) {
        result = node->value_;
        return true;
      }
      return false;
    }

    uint8_t firstChar = static_cast<uint8_t>(search[0]);
    auto childIt = node->children_.find(firstChar);
    if (childIt == node->children_.end()) {
      return false;
    }

    const RadixTreeNode& child = childIt->second;
    
    // Check if the child's prefix matches the search
    if (search.size() >= child.prefix.size() && 
        search.substr(0, child.prefix.size()) == child.prefix) {
      absl::string_view newSearch(search.begin() + child.prefix.size(), search.end());
      return findRecursive(&child, newSearch, result);
    }

    return false;
  }
};
}