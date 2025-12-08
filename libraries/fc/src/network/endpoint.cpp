#include <fc/network/ip.hpp>
#include <sstream>

namespace fc {
    namespace ip {

        endpoint::endpoint()
        {
            _port = 0;
        }

        endpoint::endpoint(const address& a, uint16_t p)
        : _ip(a)
        {
            _port = p;
        }

        uint16_t endpoint::port() const
        {
            return uint16_t(_port & 0xffff);
        }

        const address& endpoint::get_address() const
        {
            return _ip;
        }

        endpoint endpoint::from_string(const std::string& s)
        {
            auto pos = s.rfind(':');
            if (pos == std::string::npos)
                throw std::runtime_error("Missing port");

            std::string ip = s.substr(0, pos);
            std::string pr = s.substr(pos + 1);

            uint16_t p = uint16_t(std::stoi(pr));

            return endpoint(address(ip), p);
        }

        endpoint::operator std::string() const
        {
            return std::string(_ip) + ":" + std::to_string(port());
        }

        bool operator==(const endpoint& a, const endpoint& b)
        {
            return a.port() == b.port() && a.get_address() == b.get_address();
        }

        bool operator!=(const endpoint& a, const endpoint& b)
        {
            return !(a == b);
        }

        bool operator<(const endpoint& a, const endpoint& b)
        {
            if (a.get_address() == b.get_address())
                return a.port() < b.port();

            // IPv4 < IPv6
            if (!a.get_address()._is_v6 && b.get_address()._is_v6)
                return true;
            if (a.get_address()._is_v6 && !b.get_address()._is_v6)
                return false;

            // Compare IPv4 directly
            if (!a.get_address()._is_v6)
                return a.get_address()._ip4 < b.get_address()._ip4;

            // Compare IPv6 arrays
            for (int i = 0; i < 4; i++)
            {
                if (a.get_address()._ip6[i] < b.get_address()._ip6[i]) return true;
                if (a.get_address()._ip6[i] > b.get_address()._ip6[i]) return false;
            }
            return false;
        }

    } } // namespace fc::ip
