////
// @file haffman_test.cpp
// @brief
// 测试哈夫曼树
//
// @author yuhaoze
// @email 743544510@qq.com
//
#include <gtest/gtest.h>
#define private public
#include "haffman.h"
using namespace backup;

TEST(HaffmanTest, CreateHaffmanCode) {
  std::unordered_map<char, uint64_t> mp;
  mp['a'] = 3;
  Haffman haff(mp);
  auto tree = haff.createHaffmanTree();
  auto codes = haff.createHaffmanCode();
  EXPECT_EQ(codes['a'].second, std::bitset<256>(std::string("0")));

  mp['b'] = 1;
  mp['c'] = 2;
  mp['d'] = 1;
  Haffman haff2(mp);
  tree = haff2.createHaffmanTree();
  codes = haff2.createHaffmanCode();
  EXPECT_EQ(codes['a'].first, 1);
  EXPECT_EQ(codes['b'].first, 3);
  EXPECT_EQ(codes['c'].first, 2);
  EXPECT_EQ(codes['d'].first, 3);
}

TEST(HaffmanTest, CompressFileTest) {
  ::system("echo \"abcdabcd\" > haff");
  Haffman haff;
  auto& codes = haff.codes;
  codes['a'] = std::make_pair(3, std::bitset<256>("110"));
  codes['b'] = std::make_pair(1, std::bitset<256>("0"));
  codes['c'] = std::make_pair(3, std::bitset<256>("101"));
  codes['d'] = std::make_pair(3, std::bitset<256>("100"));
  codes[0x0a] = std::make_pair(3, std::bitset<256>("111"));  // \n

  Node* n0 = new Node(0xa, 0, nullptr, nullptr);
  Node* na = new Node('a', 0, nullptr, nullptr);
  Node* nb = new Node('b', 0, nullptr, nullptr);
  Node* nc = new Node('c', 0, nullptr, nullptr);
  Node* nd = new Node('d', 0, nullptr, nullptr);
  Node* n1 = new Node('\0', 0, na, n0);
  Node* n2 = new Node('\0', 0, nd, nc);
  Node* n3 = new Node('\0', 0, n2, n1);
  Node* root = new Node('\0', 0, nb, n3);
  haff.nodeQueue.push(root);
  std::ifstream ifs("haff", std::ios::binary);
  std::ofstream ofs("haff_t", std::ios::binary);
  ofs.write(reinterpret_cast<const char*>(&haff.file_len), sizeof(haff.file_len));
  ofs.flush();
  size_t size = haff.CompressFile(ifs, ofs);
  EXPECT_EQ(size, 23);
  // 编码后为：
  // 110 0 101 100 110 0 101 100 111
  // 1100 1011 0011 0010 1100 1110 最后补0
  // 0xcb32ce
  ofs.close();
  std::ifstream ifss("haff_t", std::ios::binary);
  std::bitset<256> set, answer(0xcb32ce);
  answer <<= (256 / 8 - 3) * 8;
  ifss.seekg(sizeof(haff.file_len));
  size = haff.LoadBitSet(set, size, ifss);
  EXPECT_EQ(set, answer);
  EXPECT_EQ(size, 3);

  ifss.seekg(sizeof(haff.file_len));
  std::ofstream ofss("haff_tt", std::ios::binary);
  size = haff.UnCompressFile(ifss, ofss);
  EXPECT_EQ(size, 9);
  ofss.close();
  EXPECT_EQ(::system("cmp haff haff_tt"), 0);
  ::system("rm -f haff haff_t haff_tt");
}

TEST(HaffmanTest, DumpAndRecoverTest) {
  ::system("echo \"hello world\nnvvvvvvdddeeww\" > haff_d");
  std::ifstream ifs("haff_d", std::ios::binary);
  std::ofstream ofs("haff_dd", std::ios::binary);
  Haffman haff;
  haff.CountFreq(ifs);
  haff.createHaffmanTree();
  haff.createHaffmanCode();
  haff.DumpFreq(ofs);
  EXPECT_EQ(haff.file_len_pos, 0);
  ofs.flush();
  ifs.clear();
  ifs.seekg(0, std::ios::beg);  // 注意：重新返回到开始的位置！！
  ifs.close();
  std::ifstream input("haff_d", std::ios::binary);
  haff.CompressFile(input, ofs);
  ofs.close();
  std::ifstream ifss("haff_dd", std::ios::binary);
  std::ofstream ofss("haff_ddd", std::ios::binary);
  Haffman haff2;
  haff2.RecoverFreq(ifss);
  haff2.createHaffmanTree();
  haff2.createHaffmanCode();
  for(auto& [ch, freq] : haff.charFreq) {
    EXPECT_EQ(haff2.charFreq[ch], freq);
  }
  EXPECT_EQ(haff.file_len, haff2.file_len);
  for (auto& [ch, pair] : haff.codes) {
    auto& len = pair.first;
    auto& code = pair.second;
    EXPECT_EQ(haff2.codes[ch].first, len);
    EXPECT_EQ(haff2.codes[ch].second, code);
  }
  haff2.UnCompressFile(ifss, ofss);
  ofss.close();
  EXPECT_EQ(::system("cmp haff_d haff_ddd"), 0);
  ::system("rm -f haff_d haff_dd haff_ddd");
}