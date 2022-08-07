#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    route_table.emplace_back(Route(route_prefix, prefix_length, next_hop, interface_num));
    // Your code here.
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // If the TTL was zero already,
    if (dgram.header().ttl == 0)
        return;
    dgram.header().ttl--;
    // hits zero after the decrement
    if (dgram.header().ttl == 0)
        return;
    // searches the routing table
    int idx = -1;
    uint8_t len;
    auto size = route_table.size();
    uint8_t longest_prefix_match{0};

    for (unsigned int i = 0; i < size; i++) {
        len = match(i, dgram.header().dst);
        if (idx == -1 || len > longest_prefix_match) {
            idx = i;
            longest_prefix_match = len;
        }
    }

    if (idx != -1) {
        Route route = route_table[idx];
        if (route.next_hop.has_value())
            interface(route.interface_num).send_datagram(dgram, route.next_hop->ip());
        else
            interface(route.interface_num).send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

uint8_t Router::match(int i, uint32_t ip_addr) {
    Route route = route_table[i];
    if (((route.route_prefix >> (32 - route.prefix_length)) ^ (ip_addr >> (32 - route.prefix_length))) == 0)
        return route.prefix_length;
    return 0;
}
