#pragma once

#include <algorithm>
#include <array>

namespace uri::checks::characters
{
    /*!
     * \brief Defines if the character c is a valid ASCII character.
     * \param c[in] a character
     * \return true if c is a valid ASCII character, false otherwise.
     */
    inline bool is_alpha(char c)
    {
        return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
    }

    /*!
     * \brief Defines if the character c is a valid DIGIT character.
     * \param c[in] a character
     * \return true if c is a valid DIGIT character, false otherwise.
     */
    inline bool is_digit(char c)
    {
        return (c >= '0') && (c <= '9');
    }

    /*!
     * \brief Defines if the character c is a valid hexadecimal DIGIT character.
     * \param c[in] a charater
     * \return true if c is a valid hexadecimal DIGIT character, false otherwise.
     */
    inline bool is_hex_digit(char c)
    {
        return is_digit(c) || ((c >= 'A') && (c <= 'F')) || ((c >= 'a') && (c <= 'f'));
    }

    /*!
     * \brief Defines if the character c is an unreserved character (in most RFC definitions).
     * \param c[in] a charater
     * \return true if c is an unreserved character, false otherwise.
     */
    inline bool is_unreserved(char c)
    {
        constexpr std::array unreserved{'-', '.', '_', '~'};
        return is_alpha(c) || is_digit(c)
               || std::any_of(
                   unreserved.begin(), unreserved.end(), [&c](auto const& h) { return c == h; });
    }

    /*!
     * \brief Defines if the character c is a subdelimiter character (in most RFC definitions).
     * \param c[in] a charater
     * \return true if c is a subdelimiter character, false otherwise.
     */
    inline bool is_subdelimiter(char c)
    {
        constexpr std::array subdelimiters{'!', '$', '&', '\'', '(', ')', '*', '+', ',', ';', '='};
        return std::any_of(
            subdelimiters.begin(), subdelimiters.end(), [&c](auto const& h) { return c == h; });
    }

} // namespace uri::checks::characters
