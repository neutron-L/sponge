#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
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
    , _receive_window_size(1)
    , RTO(_initial_retransmission_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {  // cout<<_bytes_in_flight<<endl;
    return _bytes_in_flight;
}

/*
 find the min value of three values below
 _stream.buffer_size()
 TCPConfig::MAX_PAYLOAD_SIZE
 cur_window_size

 the size of header do nothing to the payload,but syn and fin will change the value of sequence
 number--segment.length_in_sequence_space()
  */
void TCPSender::fill_window() {
    uint64_t right = checkpoint + (_receive_window_size == 0 ? 1 : _receive_window_size);

    for (uint16_t size = right - _next_seqno; _next_seqno < right; size = right - _next_seqno) {
        TCPSegment segment;
        TCPHeader header;
        Buffer payload;
        size_t payload_size = TCPConfig::MAX_PAYLOAD_SIZE;
        if (!_syn_sent)  // syn has no payload
        {
            _syn_sent = true;
            header.syn = true;
        } else {  // cout<<_stream.buffer_size()<<" "<<size<<endl;
            payload_size = payload_size < _stream.buffer_size() ? payload_size : _stream.buffer_size();
            payload_size = payload_size < size ? payload_size : size;
            Buffer p(_stream.read(payload_size));
            payload = p;
            size -= payload_size;
        }
        if (!_fin_sent && size > 0 && _stream.eof()) {
            // fin can own payload
            _fin_sent = true;
            header.fin = true;
            _input_end_index = _next_seqno + segment.length_in_sequence_space();
        }
        // if no payload no fin no syn just break
        if (payload_size == 0 && !header.fin && !header.syn)
            break;
        // if (header.fin)
        // cerr << "send fin " << header.seqno.raw_value() << ' ' << header.ackno.raw_value() << endl;
        header.seqno = next_seqno();
        segment.header() = header;
        segment.payload() = payload;
        _segments_out.push(segment);
        _buffer.push(segment);

        // size-=payload_size;
        _bytes_in_flight += segment.length_in_sequence_space();
        _next_seqno += segment.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // if acking something has not been sent return false
    if (unwrap(ackno, _isn, checkpoint) > _next_seqno)
        return;
    bool flag = false;  // if new data acked,flag=true
                        //  if receiver says window_size is zero,set cur_window_size to 0 because
    // we can not just stuck there,and send nothing
    _receive_window_size = window_size;
    if (_syn_sent && ackno.raw_value() == next_seqno().raw_value())
        _syn_acked = true;
    if (_fin_sent && ackno.raw_value() == next_seqno().raw_value())
        _fin_acked = true;

    while (!_buffer.empty() &&
           unwrap(_buffer.front().header().seqno + _buffer.front().length_in_sequence_space(), _isn, checkpoint) <=
               unwrap(ackno, _isn, checkpoint)) {
        RTO = _initial_retransmission_timeout;
        _number_of_consecutive_transmissions = 0;
        checkpoint += _buffer.front().length_in_sequence_space();
        _bytes_in_flight -= _buffer.front().length_in_sequence_space();
        // if ack>sequence number of last byte in one segment,pop it
        _buffer.pop();
        flag = true;
    }

    // new data acked and there are outstanding data to be acked,restart timer
    if (flag) {
        _clock = 0;
    }

    // if new space has opened up,fill the window again
    fill_window();

    // if all segments has been acked,close timer
    if (_buffer.empty() && _stream.input_ended()) {
        _clock = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_buffer.empty())
        return;
    _clock += ms_since_last_tick;
    if (_clock >= RTO && !_buffer.empty()) {
        _segments_out.push(_buffer.front());
        if (_receive_window_size != 0) {
            _number_of_consecutive_transmissions++;
            RTO <<= 1;
        }
        _clock = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _number_of_consecutive_transmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    TCPHeader header;
    header.seqno = next_seqno();
    segment.header() = header;
    _segments_out.push(segment);
}
