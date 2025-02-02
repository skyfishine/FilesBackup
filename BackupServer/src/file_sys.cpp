#include <dirent.h>
#include <fcntl.h>    // For file descriptor
#include <limits.h>   // 包含 PATH_MAX
#include <sys/stat.h> // For stat, chmod
#include <unistd.h>   // For read, write, link
#include <utime.h>    // For utime
#include <pwd.h>      // getpwuid
#include <sstream>
#include <grp.h>
#include <cassert>
#include <cstring>
#include <iostream>

#include "file_meta.h"
#include "file_sys.h"
namespace backup {

Path::Path(std::string path)
{
    // /结尾路径：/foo/
    if (path.back() == '/') path.pop_back();
    // TODO: 相对路径处理？(软链接指向的路径)
    path_ = std::move(path);
}

Path::Path(Path &&p) noexcept
    : path_(p.path_)
{}

Path &Path::operator=(const Path &path)
{
    path_ = path.path_;
    return *this;
}

Path &Path::operator=(Path &&path) noexcept
{
    if (this == &path) return *this;
    path_ = std::move(path.path_);
    return *this;
}

bool Path::IsExist() const { return fs::exists(path_); }

bool Path::IsDirectory() const { return fs::is_directory(path_); }

bool Path::IsRelative() const
{
    std::string p = path_.string();
    if (!p.empty() && *p.begin() == '/') return false;
    return true;
}

bool Path::IsRegularFile() const
{
    FileType type = GetFileType();
    return type == FileType::REG;
}

FileType Path::GetFileType() const
{
    struct stat st;
    if (lstat(path_.string().c_str(), &st) != 0) { return FileType::NOTEXIST; }
    switch (st.st_mode & S_IFMT) {
    case S_IFREG: // 普通文件
        return FileType::REG;
        break;
    case S_IFDIR: // 目录文件
        return FileType::DIR;
        break;
    case S_IFSOCK: // 套接字文件
        return FileType::SOCK;
        break;
    case S_IFCHR: // 字符设备
        return FileType::CHR;
        break;
    case S_IFIFO: // 管道
        return FileType::FIFO;
        break;
    case S_IFBLK: // 块文件
        return FileType::BLK;
        break;
    case S_IFLNK:
        return FileType::FLNK;
        break;
    default:
        return FileType::UNKNOWN;
        break;
    }
}

std::string Path::ToString() const { return path_.string(); }

std::string Path::FileName() const { return path_.filename(); }

Path Path::ParentPath() const { return Path(path_.parent_path()); }

Path &Path::ReplaceFileName(const std::string &file_name)
{
    path_.replace_filename(file_name);
    return *this;
}

Path Path::ToFullPath() const
{
    assert(IsRelative());
    return GetCurPath() / *this;
}

Path Path::operator/(const Path &other) const { return path_ / other.path_; }

std::vector<std::string> Path::SplitPath() const
{
    std::string p = ToString();
    if (p.empty()) return {};
    std::size_t start = 0, end;
    if (*p.begin() == '/') start++;
    std::vector<std::string> str_list;
    while ((end = p.find('/', start)) != std::string::npos) {
        str_list.emplace_back(p.substr(start, end - start));
        start = end + 1;
    }
    str_list.emplace_back(p.substr(start, end));
    return str_list;
}

Path::Path(const fs::path &p) { path_ = p; }
Path::Path(fs::path &&p) noexcept
    : path_(p)
{}

std::vector<Path> GetFilesFromDir(const Path &path)
{
    std::vector<Path> result;
    DIR *dirp = nullptr;
    struct dirent *dir_entry = nullptr;
    dirp = opendir(path.ToString().c_str());
    if (dirp == nullptr) {
        throw std::runtime_error("Can not open dir " + path.ToString());
    }
    while ((dir_entry = readdir(dirp)) != nullptr) {
        if (strcmp(dir_entry->d_name, ".") == 0 ||
            strcmp(dir_entry->d_name, "..") == 0)
            continue;
        result.emplace_back(dir_entry->d_name);
    }
    closedir(dirp);
    return result;
}

Path GetFileLinkTo(const std::string &path)
{
    // 用来存放软链接指向的文件路径
    char targetPath[PATH_MAX];
    ssize_t len = readlink(path.c_str(), targetPath, sizeof(targetPath) - 1);
    if (len == -1) { throw std::runtime_error("Error reading symbolic link"); }
    // 读取成功，添加字符串结束符
    targetPath[len] = '\0';
    return {std::string(targetPath)};
}

Path GetCurPath()
{
    char buffer[PATH_MAX]; // 定义一个缓冲区来存放路径

    // 获取当前工作目录
    if (getcwd(buffer, sizeof(buffer)) == nullptr) {
        std::perror("getcwd() error");
    }
    return {std::string(buffer)};
}

bool SaveFileMetaData(const FileMetadata &meta, const std::string &target)
{
    assert(meta.name == Path(target).FileName());
    bool ret = true;
    struct timespec times[2];
    times[0].tv_sec = meta.access_time; // atime: 时间戳（秒）
    times[0].tv_nsec = 0;               // 纳秒
    times[1].tv_sec = meta.mod_time;    // mtime: 时间戳（秒）
    times[1].tv_nsec = 0;               // 纳秒

    if (utimensat(AT_FDCWD, target.c_str(), times, AT_SYMLINK_NOFOLLOW) != 0) {
        std::cerr << "Can not modify time of " << target << std::endl;
        return false;
    }

    // 修改拥有者/组
    // root用户才能修改uid
    if (getuid() == 0) {
        if (lchown(target.c_str(), meta.uid, meta.gid) != 0) {
            // throw Status{errno, "无法修改文件" + target + "的用户组信息; " +
            // strerror(errno)};
            std::cerr << "Can not modify user id or group id of " << target
                      << std::endl;
            ret = false;
        }
    }

    // 修改权限
    // 权限要最后修改！
    // 软链接文件没有权限
    if (meta.type != static_cast<uint8_t>(FileType::FLNK) &&
        chmod(target.c_str(), meta.permissions) != 0) {
        std::cerr << "Can not modify permission of " << target << std::endl;
        ret = false;
    }

    return ret;
}

void MakeDir(const std::string &dirname, mode_t mode)
{
    bool ret = mkdir(dirname.c_str(), mode) == 0;
    if (!ret) {
        if (errno == EEXIST) {
            ret = true;
        } else {
            throw Status{
                errno, "无法创建文件夹" + dirname + "; " + strerror(errno)};
        }
    }
}

void Link(const std::string &to, const std::string &from)
{
    bool ret = link(to.c_str(), from.c_str()) == 0;
    if (!ret) {
        throw Status{
            errno,
            "无法将文件'" + from + "' 硬链接到文件 '" + to + "'; " +
                backup_make_error_code(errno).message()};
    }
}

void SymLink(const std::string &to, const std::string &from)
{
    bool ret = symlink(to.c_str(), from.c_str()) == 0;
    if (!ret) {
        throw Status{
            errno,
            "无法将文件'" + from + "'软链接到文件'" + to + "'; " +
                backup_make_error_code(errno).message()};
    }
}

void MakeFifo(const std::string &filename, mode_t mode)
{
    bool ret = mkfifo(filename.c_str(), mode) == 0;
    if (!ret) {
        throw Status{
            errno,
            "无法创建管道文件'" + filename + "'; " +
                backup_make_error_code(errno).message()};
    }
}

bool RemoveFile(const Path &path) noexcept { return fs::remove(path.path_); }

bool RemoveAll(const Path &path) { return fs::remove_all(path.path_); }

std::string UidToString(uid_t uid)
{
    struct passwd *pw = getpwuid(uid); // 根据 uid 获取用户信息
    if (pw) {
        return std::string(pw->pw_name); // 获取用户名并返回
    } else {
        return "Unknown User"; // 如果找不到对应的用户，返回默认值
    }
}

std::string GidToString(gid_t uid)
{
    struct group *gr = getgrgid(uid);
    if (gr) {
        return std::string(gr->gr_name);
    } else {
        return "Unknown User";
    }
}
std::string FormatTime(time_t tm, std::string format)
{
    std::tm *localTime = std::localtime(&tm);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), format.c_str(), localTime);
    return std::string(buffer);
}

std::string FormatDouble(double d, int width = 2)
{
    // 使用 ostringstream 格式化浮点数
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(width) << d;
    // 将格式化后的结果保存到字符串
    return stream.str();
}

std::string FormatSize(uint64_t size)
{
    if (size < 1024) {
        return std::to_string(size) + 'B';
    } else if (size < 1024 * 1024) {
        return FormatDouble(size / 1024.0) + "KB";
    } else if (size < 1024 * 1024 * 1024) {
        return FormatDouble(size / (1024.0 * 1024)) + "MB";
    } else {
        return FormatDouble(size / (1024.0 * 1024 * 1024)) + "GB";
    }
}

std::string FormatPermission(mode_t permission, FileType type)
{
    std::string per_s;
    switch (type) {
    case FileType::REG:
        per_s += '-';
        break;
    case FileType::DIR:
        per_s += 'd';
        break;
    case FileType::FIFO:
        per_s += 'p';
        break;
    case FileType::FLNK:
        per_s += 'l';
        break;
    default:
        per_s += '?';
        break;
    }
    // 用户权限
    per_s += (permission & S_IRUSR) ? 'r' : '-';
    per_s += (permission & S_IWUSR) ? 'w' : '-';
    per_s += (permission & S_IXUSR) ? 'x' : '-';

    // 组权限
    per_s += (permission & S_IRGRP) ? 'r' : '-';
    per_s += (permission & S_IWGRP) ? 'w' : '-';
    per_s += (permission & S_IXGRP) ? 'x' : '-';

    // 其他权限
    per_s += (permission & S_IROTH) ? 'r' : '-';
    per_s += (permission & S_IWOTH) ? 'w' : '-';
    per_s += (permission & S_IXOTH) ? 'x' : '-';
    return per_s;
}

ErrorCode Access(const std::string &path, int permission)
{
    auto ret = access(path.c_str(), permission);
    if (ret == 0) {
        return OK;
    } else {
        switch (errno) {
        case ENOENT:
            return NOT_EXIST;
        case EACCES:
            return NO_PERMISSION;
        default:
            return ERROR;
        }
    }
}

bool RenameFile(const std::string &s1, const std::string &s2)
{
    return rename(s1.c_str(), s2.c_str()) == 0;
}

std::string GetFileTypeTag(FileType type)
{
    switch (type) {
    case FileType::REG:
        return "文件";
    case FileType::DIR:
        return "文件夹";
    case FileType::FLNK:
        return "软链接";
    case FileType::FIFO:
        return "管道";
    default:
        return "文件";
    }
}
} // namespace backup
