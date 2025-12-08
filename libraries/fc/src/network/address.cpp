#include <fc/network/ip.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

namespace fc {
    namespace ip {

        address::address()
        {
            _is_v6 = false;
            _ip4 = 0;
            std::fill(std::begin(_ip6), std::end(_ip6), 0u);
        }

        address::address(uint32_t v4)
        {
            _is_v6 = false;
            _ip4 = v4;
            std::fill(std::begin(_ip6), std::end(_ip6), 0u);
        }

        address::address(const std::string& s)
        {
            *this = s;
        }

        address& address::operator=(const std::string& s)
        {
            if (s.find(':') != std::string::npos)
            {
                // IPv6
                _is_v6 = true;
                std::fill(std::begin(_ip6), std::end(_ip6), 0u);

                // Parse IPv6 using std::stringstream
                // Expecting 8 groups or compressed format ::
                std::vector<std::string> parts;
                std::string token;
                std::stringstream ss(s);

                if (s == "::")
                {
                    // all-zero IPv6 address
                    return *this;
                }

                bool has_compress = (s.find("::") != std::string::npos);

                while (std::getline(ss, token, ':'))
                    parts.push_back(token);

                std::vector<uint16_t> groups;

                if (has_compress)
                {
                    // compression
                    int empty_idx = -1;
                    for (size_t i = 0; i < parts.size(); i++)
                    {
                        if (parts[i].empty())
                        {
                            empty_idx = i;
                            break;
                        }
                    }

                    for (size_t i = 0; i < empty_idx; i++)
                        groups.push_back(uint16_t(std::stoi(parts[i], nullptr, 16)));

                    int remaining = parts.size() - empty_idx - 1;
                    int zeros = 8 - remaining - groups.size();

                    for (int i = 0; i < zeros; i++)
                        groups.push_back(0);

                    for (size_t i = empty_idx + 1; i < parts.size(); i++)
                        groups.push_back(uint16_t(std::stoi(parts[i], nullptr, 16)));
                }
                else
                {
                    for (auto& p : parts)
                        groups.push_back(uint16_t(std::stoi(p, nullptr, 16)));
                }

                if (groups.size() != 8)
                    throw std::runtime_error("Invalid IPv6 address");

                // Convert groups[0..7] -> 4 × uint32_t
                for (int i = 0; i < 4; i++)
                {
                    _ip6[i] =
                    (uint32_t(groups[i*2]) << 16) |
                    uint32_t(groups[i*2 + 1]);
                }

                return *this;
            }
            else
            {
                // IPv4
                _is_v6 = false;
                _ip4 = 0;
                std::fill(std::begin(_ip6), std::end(_ip6), 0u);

                uint32_t a, b, c, d;
                if (std::sscanf(s.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
                    throw std::runtime_error("Invalid IPv4");

                _ip4 = (a << 24) | (b << 16) | (c << 8) | d;
                return *this;
            }
        }

        address::operator std::string() const
        {
            if (!_is_v6)
            {
                uint32_t a = (_ip4 >> 24) & 0xff;
                uint32_t b = (_ip4 >> 16) & 0xff;
                uint32_t c = (_ip4 >> 8) & 0xff;
                uint32_t d = (_ip4) & 0xff;

                return std::to_string(a) + "." +
                std::to_string(b) + "." +
                std::to_string(c) + "." +
                std::to_string(d);
            }

            // IPv6
            std::stringstream ss;

            uint16_t groups[8];
            for (int i = 0; i < 4; i++)
            {
                groups[i*2]     = (_ip6[i] >> 16) & 0xffff;
                groups[i*2 + 1] = _ip6[i] & 0xffff;
            }

            for (int i = 0; i < 8; i++)
            {
                ss << std::hex << groups[i];
                if (i != 7)
                    ss << ":";
            }

            return ss.str();
        }

        bool operator==(const address& a, const address& b)
        {
            if (a._is_v6 != b._is_v6)
                return false;

            if (!a._is_v6)
                return a._ip4 == b._ip4;

            return std::equal(std::begin(a._ip6), std::end(a._ip6), std::begin(b._ip6));
        }

        bool operator!=(const address& a, const address& b)
        {
            return !(a == b);
        }

        bool address::is_private_address() const
        {
            if (_is_v6)
                return false; // TODO: RFC4193 support (fc never used this feature)

                uint32_t ip = _ip4;
            if ((ip >> 24) == 10) return true;
            if ((ip >> 20) == ((172 << 12) | (16 << 4))) return true;
            if ((ip >> 16) == ((192 << 8) | 168)) return true;
            if ((ip >> 16) == ((169 << 8) | 254)) return true;
            return false;
        }

    bool address::is_multicast_address() const
    {
      if (_is_v6)
        return false; // TODO: IPv6 multicast detection
      return (_ip4 >> 24) >= 224 && (_ip4 >> 24) < 240;
    }

    bool address::is_public_address() const
    {
      return !is_private_address() && !is_multicast_address();
    }
  }
} // namespace fc::ip
