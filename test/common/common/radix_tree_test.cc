#include "source/common/common/radix_tree.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAre;

namespace Envoy {

TEST(RadixTree, AddItems) {
  RadixTree<const char*> radixtree;
  const char* cstr_a = "a";
  const char* cstr_b = "b";
  const char* cstr_c = "c";

  EXPECT_TRUE(radixtree.add("foo", cstr_a));
  EXPECT_TRUE(radixtree.add("bar", cstr_b));
  EXPECT_EQ(cstr_a, radixtree.find("foo"));
  EXPECT_EQ(cstr_b, radixtree.find("bar"));

  // overwrite_existing = false
  EXPECT_FALSE(radixtree.add("foo", cstr_c, false));
  EXPECT_EQ(cstr_a, radixtree.find("foo"));

  // overwrite_existing = true
  EXPECT_TRUE(radixtree.add("foo", cstr_c));
  EXPECT_EQ(cstr_c, radixtree.find("foo"));
}

TEST(RadixTree, LongestPrefix) {
  RadixTree<const char*> radixtree;
  const char* cstr_a = "a";
  const char* cstr_b = "b";
  const char* cstr_c = "c";
  const char* cstr_d = "d";
  const char* cstr_e = "e";
  const char* cstr_f = "f";

  EXPECT_TRUE(radixtree.add("foo", cstr_a));
  EXPECT_TRUE(radixtree.add("bar", cstr_b));
  EXPECT_TRUE(radixtree.add("baro", cstr_c));
  EXPECT_TRUE(radixtree.add("foo/bar", cstr_d));
  // Verify that prepending and appending branches to a node both work.
  EXPECT_TRUE(radixtree.add("barn", cstr_e));
  EXPECT_TRUE(radixtree.add("barp", cstr_f));

  EXPECT_EQ(cstr_a, radixtree.find("foo"));
  EXPECT_EQ(cstr_a, radixtree.findLongestPrefix("foo"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("foo"), ElementsAre(cstr_a));
  EXPECT_EQ(cstr_a, radixtree.findLongestPrefix("foosball"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("foosball"), ElementsAre(cstr_a));
  EXPECT_EQ(cstr_a, radixtree.findLongestPrefix("foo/"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("foo/"), ElementsAre(cstr_a));
  EXPECT_EQ(cstr_d, radixtree.findLongestPrefix("foo/bar"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("foo/bar"), ElementsAre(cstr_a, cstr_d));
  EXPECT_EQ(cstr_d, radixtree.findLongestPrefix("foo/bar/zzz"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("foo/bar/zzz"), ElementsAre(cstr_a, cstr_d));

  EXPECT_EQ(cstr_b, radixtree.find("bar"));
  EXPECT_EQ(cstr_b, radixtree.findLongestPrefix("bar"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("bar"), ElementsAre(cstr_b));
  EXPECT_EQ(cstr_b, radixtree.findLongestPrefix("baritone"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("baritone"), ElementsAre(cstr_b));
  EXPECT_EQ(cstr_c, radixtree.findLongestPrefix("barometer"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("barometer"), ElementsAre(cstr_b, cstr_c));

  EXPECT_EQ(cstr_e, radixtree.find("barn"));
  EXPECT_EQ(cstr_e, radixtree.findLongestPrefix("barnacle"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("barnacle"), ElementsAre(cstr_b, cstr_e));

  EXPECT_EQ(cstr_f, radixtree.find("barp"));
  EXPECT_EQ(cstr_f, radixtree.findLongestPrefix("barpomus"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("barpomus"), ElementsAre(cstr_b, cstr_f));

  EXPECT_EQ(nullptr, radixtree.find("toto"));
  EXPECT_EQ(nullptr, radixtree.findLongestPrefix("toto"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("toto"), ElementsAre());
  EXPECT_EQ(nullptr, radixtree.find(" "));
  EXPECT_EQ(nullptr, radixtree.findLongestPrefix(" "));
  EXPECT_THAT(radixtree.findMatchingPrefixes(" "), ElementsAre());
}

TEST(RadixTree, VeryDeepRadixTreeDoesNotStackOverflowOnDestructor) {
  RadixTree<const char*> radixtree;
  const char* cstr_a = "a";

  std::string key_a(20960, 'a');
  EXPECT_TRUE(radixtree.add(key_a, cstr_a));
  EXPECT_EQ(cstr_a, radixtree.find(key_a));
}

TEST(RadixTree, RadixTreeSpecificTests) {
  RadixTree<const char*> radixtree;
  const char* cstr_a = "a";
  const char* cstr_b = "b";
  const char* cstr_c = "c";
  const char* cstr_d = "d";

  // Test radix tree compression
  EXPECT_TRUE(radixtree.add("test", cstr_a));
  EXPECT_TRUE(radixtree.add("testing", cstr_b));
  EXPECT_TRUE(radixtree.add("tester", cstr_c));
  EXPECT_TRUE(radixtree.add("tested", cstr_d));

  EXPECT_EQ(cstr_a, radixtree.find("test"));
  EXPECT_EQ(cstr_b, radixtree.find("testing"));
  EXPECT_EQ(cstr_c, radixtree.find("tester"));
  EXPECT_EQ(cstr_d, radixtree.find("tested"));

  // Test prefix matching
  EXPECT_THAT(radixtree.findMatchingPrefixes("test"), ElementsAre(cstr_a));
  EXPECT_THAT(radixtree.findMatchingPrefixes("testing"), ElementsAre(cstr_a, cstr_b));
  EXPECT_THAT(radixtree.findMatchingPrefixes("tester"), ElementsAre(cstr_a, cstr_c));
  EXPECT_THAT(radixtree.findMatchingPrefixes("tested"), ElementsAre(cstr_a, cstr_d));

  // Test longest prefix
  EXPECT_EQ(cstr_a, radixtree.findLongestPrefix("test"));
  EXPECT_EQ(cstr_b, radixtree.findLongestPrefix("testing"));
  EXPECT_EQ(cstr_c, radixtree.findLongestPrefix("tester"));
  EXPECT_EQ(cstr_d, radixtree.findLongestPrefix("tested"));
  EXPECT_EQ(cstr_a, radixtree.findLongestPrefix("testx"));
  EXPECT_EQ(nullptr, radixtree.findLongestPrefix("tex"));
}

TEST(RadixTree, EmptyAndSingleNode) {
  RadixTree<const char*> radixtree;
  const char* cstr_a = "a";

  // Test empty radixtree
  EXPECT_EQ(nullptr, radixtree.find("anything"));
  EXPECT_EQ(nullptr, radixtree.findLongestPrefix("anything"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("anything"), ElementsAre());

  // Test single node
  EXPECT_TRUE(radixtree.add("a", cstr_a));
  EXPECT_EQ(cstr_a, radixtree.find("a"));
  EXPECT_EQ(cstr_a, radixtree.findLongestPrefix("a"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("a"), ElementsAre(cstr_a));
  EXPECT_EQ(nullptr, radixtree.find("b"));
  EXPECT_EQ(nullptr, radixtree.findLongestPrefix("b"));
  EXPECT_THAT(radixtree.findMatchingPrefixes("b"), ElementsAre());
}

} // namespace Envoy 