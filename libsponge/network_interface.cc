#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    broadcast_addr.fill(-1);
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // The MAC address corresponding to the IP address is not buffered
    if (!_arp_cache.count(next_hop_ip) || _arp_cache[next_hop_ip] == broadcast_addr) {
        // set the timer and cache (broadcast)
        // If the network interface already sent an ARP request about the same IP address in the last
        // five seconds
        if (!_arp_cache.count(next_hop_ip) || _arp_timers[next_hop_ip] + 5000 < _clock) {
            _arp_cache[next_hop_ip] = broadcast_addr;
            _arp_timers[next_hop_ip] = _clock;
            make_arp(ARPMessage::OPCODE_REQUEST, next_hop_ip, broadcast_addr);
        }
        _buffer.push(make_pair(dgram, next_hop_ip));
    } else {
        _send_datagram(dgram, next_hop_ip);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const EthernetHeader &header = frame.header();
    if (header.dst == broadcast_addr || header.dst == _ethernet_address) {
        if (header.type == EthernetHeader::TYPE_ARP) {
            ARPMessage arp_msg;
            arp_msg.parse(frame.payload());

            // save the sender ip addr -- MAC addr
            _arp_timers[arp_msg.sender_ip_address] = _clock;
            _arp_cache[arp_msg.sender_ip_address] = arp_msg.sender_ethernet_address;
            if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == _ip_address.ipv4_numeric())
                make_arp(ARPMessage::OPCODE_REPLY, arp_msg.sender_ip_address, arp_msg.sender_ethernet_address);
            // send the datagram if the ethernet address is known
            auto size = _buffer.size();
            while (size--) {
                auto item = _buffer.front();
                _buffer.pop();
                if (item.second == arp_msg.sender_ip_address)
                    _send_datagram(item.first, item.second);
                else
                    _buffer.push(item);
            }
            return {};
        } else {
            IPv4Datagram dgram;
            dgram.parse(frame.payload());
            return make_optional<InternetDatagram>(dgram);
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _clock += ms_since_last_tick;
    std::queue<uint32_t> overdue_items;
    // Expire any IP-to-Ethernet mappings that have expired.
    for (auto &timer : _arp_timers) {
        if (timer.second + 30000 <= _clock && _arp_cache[timer.first] != broadcast_addr)
            overdue_items.push(timer.first);
        if (timer.second + 5000 <= _clock && _arp_cache[timer.first] == broadcast_addr) {
            _arp_timers[timer.first] = _clock;
            make_arp(ARPMessage::OPCODE_REQUEST, timer.first, broadcast_addr);
        }
    }
    // delete the overdue items
    while (!overdue_items.empty()) {
        auto ip = overdue_items.front();
        overdue_items.pop();
        _arp_cache.erase(ip);
        _arp_timers.erase(ip);
    }
}

void NetworkInterface::make_arp(uint16_t opcode, uint32_t ip_addr, EthernetAddress ether_addr) {
    EthernetHeader header;
    header.dst = ether_addr;
    header.src = _ethernet_address;
    header.type = EthernetHeader::TYPE_ARP;

    EthernetFrame frame;
    frame.header() = header;

    ARPMessage arp_msg;
    arp_msg.opcode = opcode;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    if (opcode == ARPMessage::OPCODE_REQUEST)
        ether_addr.fill(0);

    arp_msg.target_ethernet_address = ether_addr;
    arp_msg.target_ip_address = ip_addr;

    frame.payload() = arp_msg.serialize();

    _frames_out.push(frame);
}

void NetworkInterface::_send_datagram(const InternetDatagram &dgram, const uint32_t &next_hop_ip) {
    EthernetHeader header;
    header.dst = _arp_cache[next_hop_ip];
    header.src = _ethernet_address;
    header.type = EthernetHeader::TYPE_IPv4;

    EthernetFrame frame;
    frame.header() = header;
    frame.payload() = dgram.serialize();

    _frames_out.push(frame);
}
