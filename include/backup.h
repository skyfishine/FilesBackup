////
// @file backup.h
// @brief
// 文件备份（主要的api）
//
// @author yuhaoze
// @email 743544510@qq.com
//

#pragma once
#include <string>
namespace backup {

class BackUp {
 public:
  BackUp() = default;
  virtual ~BackUp() = default;
  // 将src中的所有文件保存到dest中
  virtual void Copy(const std::string& src, const std::string& dest) = 0;
  // 将目录树中的文件保存到指定位置
  // TODO: 压缩、打包、加密
  virtual void Save(const std::string& src, const std::string& dest) = 0;
  // 将目录树中的文件恢复到指定位置
  // TODO: 解压、解包、解密
  virtual void Recover(const std::string& src, const std::string& dest) = 0;

 private:
  /* data */
};

}  // namespace backup
