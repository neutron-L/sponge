#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto header = seg.header();
    if (header.syn) {
        _has_syn = true;
        _syn_no = header.seqno;
    }
    if (_has_syn && header.fin) {
        _has_fin = true;
    }

    auto data = seg.payload();
    auto seqno = header.seqno;
    //
    if ((header.syn && data.size()) || (header.syn && header.fin))
        seqno = seqno + 1;
    if (_has_syn) {
        // assert(ackno().has_value());
        uint64_t checkpoint = _reassembler.head_index();
        uint64_t idx = unwrap(seqno, _syn_no, checkpoint);  // get the absolute index
        if (data.size() || header.fin) {
            idx--;
            _reassembler.push_substring(data.copy(), idx, header.fin);
        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_has_syn)
        return {};
    else {
        size_t head_index = _reassembler.head_index();
        head_index++;                          // get the absolute seqno of ack number
        if (_has_fin && _reassembler.empty())  // the segment contains fin and syn, and stream is empty
            head_index++;
        return make_optional<WrappingInt32>(wrap(head_index, _syn_no));
    }
    if (_has_fin) {
        _has_syn = false;
        _has_fin = false;
    }
}

size_t TCPReceiver::window_size() const { return static_cast<StreamReassembler>(_reassembler).window_size(); }
