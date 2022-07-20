#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    auto len = data.length();
    auto rem_size = _capacity - _size;
    if (len > rem_size)
        len = rem_size;
    for (size_t i = 0; i < len; i++)
        _buffer.push_back(data[i]);
    _size += len;
    _write_count += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t l = len;
    if (l > _size)
        l = _size;
    return string().assign(_buffer.begin(), _buffer.begin() + l);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t l = len;
    if (l > _size)
        l = _size;
    _read_count += l;
    while (l--) {
        _buffer.pop_front();
        _size--;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string data;

    size_t l = len;
    if (l > _size)
        l = _size;

    while (l--) {
        data += _buffer.front();
        _buffer.pop_front();
        _size--;
    }
    _read_count += l;
    return data;
}

void ByteStream::end_input() { _input_ended_flag = true; }

bool ByteStream::input_ended() const { return _input_ended_flag; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return _size == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - _size; }
