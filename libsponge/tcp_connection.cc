#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _clock; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active || (!_sender.syn_sent() && !seg.header().syn))
        return;
    _clock = 0;

    if (seg.header().rst) {
        _active = false;
        _linger_after_streams_finish = false;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        // }
    } else {
        _receiver.segment_received(seg);
        if (seg.header().fin && !_sender.stream_in().eof()) {
            _linger_after_streams_finish = false;
        }
        if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            if (_receiver.has_fin() && !_linger_after_streams_finish && _sender.fin_acked())
                _active = false;
        }
        if (!_active)
            return;
        if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
            seg.header().seqno == _receiver.ackno().value() - 1) {
            _sender.send_empty_segment();
        }
        bool empty = _sender.stream_in().buffer_empty();
        bool syn_recv = _sender.syn_sent() && seg.header().syn;
        bool fin_recv = seg.header().fin && (!_sender.stream_in().eof() || _sender.fin_sent());
        bool has_data = seg.payload().size();
        if (empty && (syn_recv || fin_recv || has_data))
            _sender.send_empty_segment();
        if (seg.header().fin)
            _sender.has_recv_fin();
        send_segment(false);
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    send_segment(false);
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _clock += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        if (_sender.segments_out().empty())
            _sender.send_empty_segment();
        send_segment(true);
        unclean_shutdown();
    }

    if (_sender.fin_acked() && _receiver.has_fin() && _clock >= 10 * _cfg.rt_timeout) {
        _linger_after_streams_finish = false;
        _active = false;
    }

    if (!_active)
        return;

    if (_sender.syn_sent())
        send_segment(false);
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    send_segment(false);
}

void TCPConnection::connect() { send_segment(false); }

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            if (_sender.segments_out().empty())
                _sender.send_empty_segment();
            send_segment(true);
            unclean_shutdown();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_segment(bool rst_set) {
    _sender.fill_window();
    auto &que = _sender.segments_out();
    while (!que.empty()) {
        auto seg = que.front();
        que.pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = _receiver.window_size();
        seg.header().rst = rst_set;
        _segments_out.push(std::move(seg));
    }
}

void TCPConnection::unclean_shutdown() {
    _active = false;
    _linger_after_streams_finish = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}