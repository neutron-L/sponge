#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _send_window_size(0)
    , _receive_window_size(1)
    , RTO(_initial_retransmission_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // the state is CLOSE, send syn
    if (!_syn_sent && _next_seqno == 0) {
        _send_segment(0, true, false);
    } else if (!_fin_sent && _stream.eof() && _send_window_size < _receive_window_size)
        _send_segment(0, false, true);
    else {
        // send_empty_segment();
        while (!_fin_sent && !_stream.buffer_empty() && _send_window_size < _receive_window_size) {
            uint16_t rem_size = _receive_window_size - _send_window_size;
            if (rem_size > TCPConfig::MAX_PAYLOAD_SIZE)
                rem_size = TCPConfig::MAX_PAYLOAD_SIZE;

            // send segment
            _send_segment(rem_size, false, false);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (ackno - next_seqno() > 0)
        return;

    _receive_window_size = window_size;
    if (_receive_window_size)
        _zero_size = false;
    else {
        _zero_size = true;
        _receive_window_size = 1;
    }
    if (_syn_sent && !_syn_acked && ackno == next_seqno())
        _syn_acked = true;
    else if (_fin_sent && !_fin_acked && ackno == next_seqno())
        _fin_acked = true;

    if (!_buffer.empty() &&
        wrap(_buffer.front().length_in_sequence_space(), _buffer.front().header().seqno) - ackno <= 0) {
        _clock = 0;
        // reset the RTO
        RTO = _initial_retransmission_timeout;
        // reset the count of "consecutive retransmissions" back to zero
        _number_of_consecutive_transmissions = 0;

        while (!_buffer.empty() &&
               wrap(_buffer.front().length_in_sequence_space(), _buffer.front().header().seqno) - ackno <= 0) {
            auto len = _buffer.front().length_in_sequence_space();
            _send_window_size -= len;
            _bytes_in_flight -= len;
            _buffer.pop();
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _clock += ms_since_last_tick;
    if (_clock >= RTO) {
        _clock = 0;
        _number_of_consecutive_transmissions++;
        if (!_zero_size)
            RTO <<= 1;
        if (!_buffer.empty()) {
            _segments_out.push(_buffer.front());
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _number_of_consecutive_transmissions; }

void TCPSender::send_empty_segment() { _send_segment(0, false, false); }

void TCPSender::_send_segment(uint16_t size, bool syn, bool fin) {
    TCPSegment segment;
    segment.payload() = Buffer(_stream.read(size));

    TCPHeader header;
    header.seqno = next_seqno();
    header.syn = syn;
    header.fin = fin;

    if (segment.payload().size() < static_cast<size_t>(_receive_window_size - _send_window_size) && _stream.eof())
        header.fin = true;

    segment.header() = header;

    uint64_t len = segment.length_in_sequence_space();

    _bytes_in_flight += len;
    _next_seqno += len;
    _send_window_size += len;
    if (header.syn)
        _syn_sent = true;
    if (header.fin)
        _fin_sent = true;

    // if segment is not empty
    if (size || header.syn || header.fin)
        _buffer.push(segment);
    _segments_out.push(segment);
}
