#pragma once

#include "uri/character_checks.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace uri::checks::elements
{
    inline bool is_decimal_octet(std::string_view const& element)
    {
        // a decimal octet as a string representing a value between 0 and 255.
        int value = 0;
        switch (element.size())
        {
            case 0:
                return false;

            case 1:
                // valid: 0-9
                if (characters::is_digit(element[0]))
                {
                    value = element[0] - '0';
                }
                return value > 0;

            case 2:
                // valid: 10-99
                if (characters::is_digit(element[0]) && characters::is_digit(element[1]))
                {
                    value = (element[0] - '0') * 10 + (element[1] - '0');
                }
                return value >= 10 && value < 100;

            case 3:
                // valid: 100-255
                if (characters::is_digit(element[0]) && characters::is_digit(element[1])
                    && characters::is_digit(element[2]))
                {
                    value = (element[0] - '0') * 100 + (element[1] - '0') * 10 + (element[2] - '0');
                }
                return value >= 100 && value < 256;
        }

        return false;
    }

    inline bool is_IPv4(std::string_view const& element)
    {
        std::array<std::string_view, 4> octet;
        std::size_t idx = 0;
        std::string::size_type last = 0;
        std::string::size_type i = 0;
        for (; (i < element.size()) && (idx < 4); ++i)
        {
            if (element[i] == '.')
            {
                octet[idx++] = element.substr(last, i - last);
                last = i + 1;
                continue;
            }

            if (!characters::is_digit(element[i]))
            {
                return false;
            }
        }

        // capturing final dec-octet
        if ((i == element.size()) && (idx != 4))
        {
            octet[idx++] = entity.substr(last, element.size() - last);
        }

        return (idx == 4) && std::all_of(octet.cbegin(), octet.cend(), [](auto const& sv) {
            return is_decimal_octet(sv);
        });
    }

    inline bool is_IPv6(std::string_view const& element)
    {
        bool colonPresence = false, reconIPv4 = false;

        for (auto const& c : element)
        {
            if (characters::is_hex_digit(c))
            {
                continue;
            }
            if ((c == ':') && !reconIPv4)
            {
                colonPresence = true;
                continue;
            }
            if ((c == '.') && colonPresence)
            {
                reconIPv4 = true;
                continue;
            }

            return false;
        }

        return true;
    }

    inline bool is_IPLiteral(std::string_view const& element)
    {
        bool res = !element.empty();
        if (res)
        {
            res = element[0] == '[' && element[element.size() - 1] == ']';
            if (res)
            {
                auto address = element.substr(1, element.size() - 2);
                res = !address.empty() && is_IPv6(address);
            }
        }

        return res;
    }

    inline bool is_regular_name(std::string_view const& element)
    {
        if (element.empty())
        {
            return true;
        }

        for (std::string_view::size_type i = 0; i < element.size(); ++i)
        {
            if (characters::is_unreserved(element[i]) || characters::is_subdelimiter(element[i]))
            {
                continue;
            }

            // pct-encoded
            if (element[i] == '%' && i < (element.size() - 2)
                && characters::is_hex_digit(element[i + 1])
                && characters::is_hex_digit((element[i + 2])))
            {
                i += 2;
                continue;
            }

            return false;
        }

        return true;
    }
} // namespace uri::checks::elements
