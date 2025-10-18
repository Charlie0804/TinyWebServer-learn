#include "buffer.h"

// 初始化
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 可写的数量 ： buffer大小 - 写下标
size_t Buffer::WriteableBytes() const { return buffer_.size() - writePos_; }

// 可读的数量 ： 写下标 - 读下标
size_t Buffer::ReadableBytes() const { return writePos_ - readPos_; }

// 可预留空间的数量 ： 读下标
size_t Buffer::PrependableBytes() const { return readPos_; }

const char* Buffer::Peek() const { return &buffer_[readPos_]; }

// 确保可写的长度
void Buffer::EnsureWriteable(size_t len)
{
    if (len > WriteableBytes())
    {
        MakeSpace_(len);
    }
    assert(len <= WriteableBytes());  // 确保可写的长度
}

// 移动写下标，在Append中使用
void Buffer::HasWritten(size_t len) { writePos_ += len; }

// 读取len长度，移动读下标
void Buffer::Retrieve(size_t len) { readPos_ += len; }

// 读取到end位置
void Buffer::RetrieveUntil(const char* end)
{
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

// 取出所有数据，buffer归零，读写下标归零
void Buffer::RetrieveAll()
{
    bzero(&buffer_[0], buffer_.size());
    readPos_ = writePos_ = 0;
}

// 取出剩余可读的str
std::string Buffer::RetrieveAllToStr()
{
    str::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 写指针的位置
const char* Buffer::BeginWriteConst() const { return &buffer_[writePos_]; }

char* Buffer::BeginWrite() { return &buffer_[writePos_]; }

// 添加str到缓冲区
void Buffer::Append(const char* str, size_t len)
{
    assert(str);
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Append(const void* data, size_t len) { Append(str.c_str(), str.size()); }

void Append(const void* data, size_t len) { Append(static_cast<const char*>(data), len); }

// 将buffer中的读下标的地方放到该buffer中的写下标位置
void Append(const Buffer& buff) { Append(buff.Peek(), buff.ReadableBytes()); }

// 将fd的内容读到缓冲区，即writable的位置
ssize_t Buffer::ReadFd(int fd, int* Errno)
{
    char buff[65535];
    struct iovec vec[2];
    size_t writeable = WriteableBytes();
    // 分散读
    iov[0].iov_base = BeginWrite();
    iov[0].iov_len = writeable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    ssize_t len = readv(fd, iov, 2);
    if (len < 0)
    {
        *Errno = errno;
    }
    else if (static_cast<size_t>(len) <= writeable)
    {
        writePos_ += len;
    }
    else
    {
        writePos_ = buffer_.size();
        Append(buff, static_cast<size_t>(len - writeable));
    }
    return len;
}

// 将buffer中可读的区域写入fd中
ssize_t Buffer::WriteFd(int fd, int* Errno)
{
    ssize_t len = write(fd, Peek(), ReadableBytes());
    if (len < 0)
    {
        *Errno = errno;
        return len;
    }
    Retrieve(len);
    return len;
}

char* Buffer::BeginPtr_() { return &buffer_[0]; }

const char* Buffer::BeginPtr_() const { return &buffer_[0]; }

void Beffer::MakeSpace_(size_t len)
{
    if (WriteableBytes() + PrependableBytes() < len)
    {
        buffer_.resize(writePos_ + len + 1);
    }
    else
    {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readable;
        assert(readable == ReadableBytes())
    }
}
