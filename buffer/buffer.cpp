#include "buffer.h"

// 初始化
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 可写的数量 ： buffer大小 - 写下标
size_t Buffer::WriteableBytes() const {
    return buffer_.size() - writePos_;
}

// 可读的数量 ： 写下标 - 读下标
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

// 可预留空间的数量 ： 读下标
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

const char* Buffer::Peek() const {
    return &buffer_[readPos_];
}

// 确保可写的长度
void Buffer::EnsureWriteable(size_t len) {
    if (len > WriteableBytes()) {
        MakeSpace_(len);
    }
    assert(len <= WriteableBytes()); // 确保可写的长度
}

//