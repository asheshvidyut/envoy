#include "source/common/common/radix_tree.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAre;

namespace Envoy {

TEST(RadixTree, AddItems) {
  RadixTree<const char*> trie;
  const char* cstr_a = "a";
  const char* cstr_b = "b";
  const char* cstr_c = "c";

  EXPECT_TRUE(trie.add("foo", cstr_a));
  EXPECT_TRUE(trie.add("bar", cstr_b));
  EXPECT_EQ(cstr_a, trie.find("foo"));
  EXPECT_EQ(cstr_b, trie.find("bar"));

  // overwrite_existing = false
  EXPECT_FALSE(trie.add("foo", cstr_c, false));
  EXPECT_EQ(cstr_a, trie.find("foo"));

  // overwrite_existing = true
  EXPECT_TRUE(trie.add("foo", cstr_c));
  EXPECT_EQ(cstr_c, trie.find("foo"));
}

TEST(RadixTree, LongestPrefix) {
  RadixTree<const char*> trie;
  const char* cstr_a = "a";
  const char* cstr_b = "b";
  const char* cstr_c = "c";
  const char* cstr_d = "d";
  const char* cstr_e = "e";
  const char* cstr_f = "f";

  EXPECT_TRUE(trie.add("foo", cstr_a));
  EXPECT_TRUE(trie.add("bar", cstr_b));
  EXPECT_TRUE(trie.add("baro", cstr_c));
  EXPECT_TRUE(trie.add("foo/bar", cstr_d));
  // Verify that prepending and appending branches to a node both work.
  EXPECT_TRUE(trie.add("barn", cstr_e));
  EXPECT_TRUE(trie.add("barp", cstr_f));

  EXPECT_EQ(cstr_a, trie.find("foo"));
  EXPECT_EQ(cstr_a, trie.findLongestPrefix("foo"));
  EXPECT_THAT(trie.findMatchingPrefixes("foo"), ElementsAre(cstr_a));
  EXPECT_EQ(cstr_a, trie.findLongestPrefix("foosball"));
  EXPECT_THAT(trie.findMatchingPrefixes("foosball"), ElementsAre(cstr_a));
  EXPECT_EQ(cstr_a, trie.findLongestPrefix("foo/"));
  EXPECT_THAT(trie.findMatchingPrefixes("foo/"), ElementsAre(cstr_a));
  EXPECT_EQ(cstr_d, trie.findLongestPrefix("foo/bar"));
  EXPECT_THAT(trie.findMatchingPrefixes("foo/bar"), ElementsAre(cstr_a, cstr_d));
  EXPECT_EQ(cstr_d, trie.findLongestPrefix("foo/bar/zzz"));
  EXPECT_THAT(trie.findMatchingPrefixes("foo/bar/zzz"), ElementsAre(cstr_a, cstr_d));

  EXPECT_EQ(cstr_b, trie.find("bar"));
  EXPECT_EQ(cstr_b, trie.findLongestPrefix("bar"));
  EXPECT_THAT(trie.findMatchingPrefixes("bar"), ElementsAre(cstr_b));
  EXPECT_EQ(cstr_b, trie.findLongestPrefix("baritone"));
  EXPECT_THAT(trie.findMatchingPrefixes("baritone"), ElementsAre(cstr_b));
  EXPECT_EQ(cstr_c, trie.findLongestPrefix("barometer"));
  EXPECT_THAT(trie.findMatchingPrefixes("barometer"), ElementsAre(cstr_b, cstr_c));

  EXPECT_EQ(cstr_e, trie.find("barn"));
  EXPECT_EQ(cstr_e, trie.findLongestPrefix("barnacle"));
  EXPECT_THAT(trie.findMatchingPrefixes("barnacle"), ElementsAre(cstr_b, cstr_e));

  EXPECT_EQ(cstr_f, trie.find("barp"));
  EXPECT_EQ(cstr_f, trie.findLongestPrefix("barpomus"));
  EXPECT_THAT(trie.findMatchingPrefixes("barpomus"), ElementsAre(cstr_b, cstr_f));

  EXPECT_EQ(nullptr, trie.find("toto"));
  EXPECT_EQ(nullptr, trie.findLongestPrefix("toto"));
  EXPECT_THAT(trie.findMatchingPrefixes("toto"), ElementsAre());
  EXPECT_EQ(nullptr, trie.find(" "));
  EXPECT_EQ(nullptr, trie.findLongestPrefix(" "));
  EXPECT_THAT(trie.findMatchingPrefixes(" "), ElementsAre());
}

TEST(RadixTree, VeryDeepTrieDoesNotStackOverflowOnDestructor) {
  RadixTree<const char*> trie;
  const char* cstr_a = "a";

  std::string key_a(20960, 'a');
  EXPECT_TRUE(trie.add(key_a, cstr_a));
  EXPECT_EQ(cstr_a, trie.find(key_a));
}

TEST(RadixTree, RadixTreeSpecificTests) {
  RadixTree<const char*> trie;
  const char* cstr_a = "a";
  const char* cstr_b = "b";
  const char* cstr_c = "c";
  const char* cstr_d = "d";

  // Test radix tree compression
  EXPECT_TRUE(trie.add("test", cstr_a));
  EXPECT_TRUE(trie.add("testing", cstr_b));
  EXPECT_TRUE(trie.add("tester", cstr_c));
  EXPECT_TRUE(trie.add("tested", cstr_d));

  EXPECT_EQ(cstr_a, trie.find("test"));
  EXPECT_EQ(cstr_b, trie.find("testing"));
  EXPECT_EQ(cstr_c, trie.find("tester"));
  EXPECT_EQ(cstr_d, trie.find("tested"));

  // Test prefix matching
  EXPECT_THAT(trie.findMatchingPrefixes("test"), ElementsAre(cstr_a));
  EXPECT_THAT(trie.findMatchingPrefixes("testing"), ElementsAre(cstr_a, cstr_b));
  EXPECT_THAT(trie.findMatchingPrefixes("tester"), ElementsAre(cstr_a, cstr_c));
  EXPECT_THAT(trie.findMatchingPrefixes("tested"), ElementsAre(cstr_a, cstr_d));

  // Test longest prefix
  EXPECT_EQ(cstr_a, trie.findLongestPrefix("test"));
  EXPECT_EQ(cstr_b, trie.findLongestPrefix("testing"));
  EXPECT_EQ(cstr_c, trie.findLongestPrefix("tester"));
  EXPECT_EQ(cstr_d, trie.findLongestPrefix("tested"));
  EXPECT_EQ(cstr_a, trie.findLongestPrefix("testx"));
  EXPECT_EQ(nullptr, trie.findLongestPrefix("tex"));
}

TEST(RadixTree, EmptyAndSingleNode) {
  RadixTree<const char*> trie;
  const char* cstr_a = "a";

  // Test empty trie
  EXPECT_EQ(nullptr, trie.find("anything"));
  EXPECT_EQ(nullptr, trie.findLongestPrefix("anything"));
  EXPECT_THAT(trie.findMatchingPrefixes("anything"), ElementsAre());

  // Test single node
  EXPECT_TRUE(trie.add("a", cstr_a));
  EXPECT_EQ(cstr_a, trie.find("a"));
  EXPECT_EQ(cstr_a, trie.findLongestPrefix("a"));
  EXPECT_THAT(trie.findMatchingPrefixes("a"), ElementsAre(cstr_a));
  EXPECT_EQ(nullptr, trie.find("b"));
  EXPECT_EQ(nullptr, trie.findLongestPrefix("b"));
  EXPECT_THAT(trie.findMatchingPrefixes("b"), ElementsAre());
}

} // namespace Envoy 