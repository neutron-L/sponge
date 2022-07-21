#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _inbuffer(capacity), _arrived(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    auto len = data.length();

    // Adjust the index of read data
    auto i = _first_unassembled_idx - _output.buffer_size();
    while (_first_unread_idx < i) {
        _arrived[_first_unread_idx % _capacity] = false;
        _first_unread_idx++;
    }
    size_t first_unacceptable_idx = _first_unread_idx + _capacity;

    // if eof is true, and the last byte of data is within the window, then the _eof is true
    if (eof && index + len <= first_unacceptable_idx)
        _eof = true;

    if (index + len > _first_unassembled_idx) {
        size_t idx = index;
        auto iter = data.begin();

        // ignore the bytes that have assembled
        if (idx < _first_unassembled_idx) {
            iter += _first_unassembled_idx - idx;
            len -= _first_unassembled_idx - idx;
            idx = _first_unassembled_idx;
        }

        len = min(first_unacceptable_idx - idx, len);

        if (len) {
            while (len--) {
                if (!_arrived[idx % _capacity])
                    _unassembled_size++;
                _arrived[idx % _capacity] = true;
                _inbuffer[idx % _capacity] = *iter++;
                idx++;
            }

            // ressemble bytes
            string s;
            while (_first_unassembled_idx < first_unacceptable_idx && _arrived[_first_unassembled_idx % _capacity]) {
                s += _inbuffer[_first_unassembled_idx % _capacity];
                _first_unassembled_idx++;
            }
            if (s.length())
                _output.write(s);
            _unassembled_size -= s.length();
        }
    }

    if (empty())
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_size; }

bool StreamReassembler::empty() const { return _unassembled_size == 0 && _eof; }
