#pragma once

#include "uri/character_checks.h"
#include "uri/elements_checks.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace uri
{
    class URI
    {
        using strv = std::string_view;

      public:
        enum class ParsingResult
        {
            NoError,
            EmptyURI,
        };

        enum class ParsingSteps
        {
            Scheme,
            CheckAuthority,
            Authority,
            CheckSeparator,
            Path,
            Fragment,
            Query
        };

        URI() = default;
        explicit URI(std::string const& uri) : uri_(uri)
        {
            parse();
        }
        explicit URI(std::string&& uri) : uri_(std::move(uri))
        {
            parse();
        }

        // Implicit conersion to type std::string for convinience
        operator std::string() const
        {
            return uri_;
        }

        [[nodiscard]] std::optional<strv> scheme() const
        {
            if (scheme_.empty())
            {
                return std::nullopt;
            }
            else
            {
                return scheme_;
            }
        }

        [[nodiscard]] std::optional<strv> user() const
        {
            if (user_.empty())
            {
                return std::nullopt;
            }
            else
            {
                return user_;
            }
        }

        [[nodiscard]] std::optional<strv> host() const
        {
            if (host_.empty())
            {
                return std::nullopt;
            }
            else
            {
                return host_;
            }
        }

        template<typename ReturnType = strv>
        [[nodiscard]] ReturnType port() const
        {
            if constexpr (std::is_integral_v<ReturnType>)
            {
                if (port_.empty())
                {
                    return static_cast<ReturnType>(0);
                }
                else
                {
                    return std::atoi(port_.data());
                }
            }
            else
            {
                return port_;
            }
        }

        [[nodiscard]] std::optional<strv> path() const
        {
            if (path_.empty())
            {
                return std::nullopt;
            }
            else
            {
                return path_;
            }
        }

        [[nodiscard]] std::optional<strv> path(size_t i) const
        {
            if (path().has_value())
            {
                i = std::min(i, pathSegments_.size() - 1);
                return pathSegments_.at(i);
            }
            else
            {
                return std::nullopt;
            }
        }

        [[nodiscard]] std::optional<strv> pathUntil(size_t i) const
        {
            if (path().has_value())
            {
                i = std::min(i, pathSegments_.size() - 1);
                auto length = std::accumulate(pathSegments_.begin(),
                    std::next(pathSegments_.begin(), i + 1),
                    0,
                    [](size_t a, auto const& b) { return a + b.size(); });
                return path_.substr(0u, length);
            }
            else
            {
                return std::nullopt;
            }
        }

        [[nodiscard]] std::optional<size_t> pathSize() const
        {
            if (path().has_value())
            {
                return pathSegments_.size();
            }
            else
            {
                return std::nullopt;
            }
        }

        [[nodiscard]] std::optional<strv> queryLine() const
        {
            if (!queryLine_.empty())
            {
                return queryLine_;
            }
            else
            {
                return std::nullopt;
            }
        }

        [[nodiscard]] std::optional<std::map<strv, strv>> queries() const
        {
            if (queries_.empty())
                return std::nullopt;

            std::map<strv, strv> res;
            for (auto const& [key, value] : queries_)
            {
                res.emplace(key, value);
            }

            return res;
        }

        [[nodiscard]] std::optional<strv> fragment() const
        {
            if (fragment_.empty())
            {
                return std::nullopt;
            }
            else
            {
                return fragment_;
            }
        }

        bool hasAuthority() const
        {
            return host().has_value(); // minimal to define that authority has been found.
        }

        bool hasQueries() const
        {
            return queries().has_value();
        }

        bool hasPath() const
        {
            return path().has_value();
        }

        bool hasFragment() const
        {
            return fragment().has_value();
        }
        /*!
         * \brief Defines if the current URI complies to RFC3986
         * \return true if completely compliant, otherwise false.
         */
        bool isCompliant() const
        {
            return isSchemeCompliant(scheme_) && isUserCompliant(user_)
                   && (host().has_value() && isHostCompliant(host_)) && isPortCompliant(port_)
                   && std::all_of(pathSegments_.begin(),
                       pathSegments_.end(),
                       [&](auto const& seg) { return isPathSegmentCompliant(seg); })
                   && isQueryCompliant(queryLine_) && isFragmentCompliant(fragment_);
        }

        bool isAbsolutePath() const
        {
            return isAbsolutePath_;
        }

        void setScheme(std::string const& newScheme)
        {
            if (scheme().has_value())
            {
                uri_.replace(scheme_[0], scheme_.size(), newScheme);
            }
            else
            {
                std::string scheme = newScheme + std::string(SCHEME_SEP);
                uri_.append(scheme, scheme.size(), 0u);
            }

            scheme_ = strv{uri_.c_str(), newScheme.size()};
        }

        std::string string() const
        {
            return uri_;
        }

        void clear()
        {
            scheme_ = strv{};
            user_ = strv{};
            host_ = strv{};
            port_ = strv{};
            path_ = strv{};
            fragment_ = strv{};
            queryLine_ = strv{};

            pathSegments_.clear();
            queries_.clear();

            uri_.clear();
        }

      private:
        static constexpr strv SCHEME_SEP = ":";
        static constexpr strv AUTH_ANON = "//";
        static constexpr strv USER_SEP = "@";
        static constexpr strv PORT_SEP = ":";
        static constexpr strv PATH_SEP = "/";
        static constexpr strv QUERY_ANON = "?";
        static constexpr strv QUERY_SEP = "&";
        static constexpr strv FRAG_SEP = "#";
        static constexpr strv KV_SEP = "=";

        bool isSchemeCharacter(char c) const
        {
            // scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
            constexpr std::array ch{'+', '-', '.'};
            return checks::characters::is_alpha(c) || checks::characters::is_digit(c)
                   || std::any_of(ch.begin(), ch.end(), [&c](auto const& h) { return c == h; });
        }

        std::string_view tryParseScheme(std::string_view uri)
        {
            std::string sep;
            sep.append(SCHEME_SEP).append(AUTH_ANON);

            // search for "http://...", "https://..." or "ldap://..."
            auto schemeSeparatorPosition =
                std::search(uri.begin(), uri.end(), sep.begin(), sep.end());
            if (schemeSeparatorPosition != uri.end())
            {
                // scheme_ = strv{uri.begin(), std::prev(schemeSeparatorPosition)}; // TODO msc
                // C++20
                scheme_ = uri.substr(
                    0u, static_cast<size_t>(std::distance(uri.begin(), schemeSeparatorPosition)));

                uri.remove_prefix(scheme_.size() + SCHEME_SEP.size());
            }

            return uri;
        }

        bool isSchemeCompliant(std::string_view const& scheme) const
        {
            if (scheme.empty())
            {
                return false;
            }
            if (!checks::characters::is_alpha(scheme.front()))
            {
                return false;
            }
            if (std::any_of(std::next(scheme.begin()), scheme.end(), [&](auto const& c) {
                    return !isSchemeCharacter(c);
                }))
            {
                return false;
            }

            return true;
        }

        std::string_view tryParseUser(std::string_view uri)
        {
            // search for "user@github.com/path/to/ressource..."
            auto userSeparatorPosition =
                std::search(uri.begin(), uri.end(), USER_SEP.begin(), USER_SEP.end());
            if (userSeparatorPosition != uri.end())
            {
                if (userSeparatorPosition == uri.begin())
                {
                    // ex : "@local.fr/path/to/ressource"
                    throw std::runtime_error("'@' symbol can not follow an empty user name.");
                }
                // user_ = strv{uri.begin(), std::prev(userSeparatorPosition)}; // TODO msc C++20
                user_ = uri.substr(
                    0u, static_cast<size_t>(std::distance(uri.begin(), userSeparatorPosition)));

                uri.remove_prefix(user_.size() + USER_SEP.size());
            }

            return uri;
        }

        bool isUserCompliant(std::string_view const& user) const
        {
            if (user.empty())
            {
                return true;
            }

            constexpr auto ch = ':';
            for (std::string_view::size_type i = 0; i < user.size(); ++i)
            {
                if (checks::characters::is_unreserved(user[i])
                    || checks::characters::is_subdelimiter(user[i]) || (user[i] == ch))
                {
                    continue;
                }

                // pct-encoded
                if (user[i] == '%' && i < (user.size() - 2)
                    && checks::characters::is_hex_digit(user[i + 1])
                    && checks::characters::is_hex_digit(user[i + 2]))
                {
                    i += 2;
                    continue;
                }

                return false;
            }

            return true;
        }

        std::string_view tryParseHost(std::string_view uri)
        {
            // search for "my.space.com:8080"...
            // we need the last colon as host can be an IPv6 that can contain a similir caracter.
            auto portSeparatorPosition = uri.find_last_of(PORT_SEP);
            if (portSeparatorPosition != strv::npos)
            {
                host_ = uri.substr(0u, portSeparatorPosition);
                port_ = uri.substr(portSeparatorPosition + 1);

                uri.remove_prefix(host_.size() + PORT_SEP.size() + port_.size());
            }
            else
            {
                host_ = uri;
                uri.remove_prefix(host_.size());
            }

            return uri;
        }

        bool isHostCompliant(std::string_view const& host) const
        {
            if (host.find_first_of('[') != std::string_view::npos)
            {
                return checks::elements::is_IPLiteral(host);
            }
            return checks::entities::is_IPv4(host)
                   || checks::elements::is_regular_name(host);
        }

        bool isPortCompliant(std::string_view const& port) const
        {
            return std::all_of(port.cbegin(), port.cend(), [](char c) {
                return checks::characters::is_digit(c);
            });
        }

        std::string_view tryParseAuthority(std::string_view uri)
        {
            std::string seps;
            seps.append(PATH_SEP).append(FRAG_SEP).append(QUERY_ANON);

            auto authority = uri;
            if (auto pos = uri.find_first_of(seps); pos != strv::npos)
            {
                // pos begin at 0, so for a size, it's pos+1 and we want the previous one, so pos.
                authority = uri.substr(0u, pos);
            }

            auto initialAuthSize = authority.size();

            authority = tryParseUser(authority);

            if (!authority.empty())
            {
                authority = tryParseHost(authority);
            }

            uri.remove_prefix(initialAuthSize);

            return uri;
        }

        std::string_view tryParsePath(std::string_view uri)
        {
            if (hasAuthority())
            {
                uri.remove_prefix(PATH_SEP.size());
            }

            std::string seps;
            seps.append(QUERY_ANON).append(FRAG_SEP);

            path_ = uri;
            if (auto pos = uri.find_first_of(seps); pos != strv::npos)
            {
                // pos begin at 0, so for a size, it's pos+1 and we want the previous one, so pos.
                path_ = uri.substr(0u, pos);
            }

            if (uri.front() == PATH_SEP.front())
            {
                isAbsolutePath_ = true;
            }

            strv pathView = path_;

            while (!pathView.empty())
            {
                if (auto pos = pathView.find(PATH_SEP); pos != strv::npos)
                {
                    // pos begin at 0, so for a size, it's pos+1.
                    pathSegments_.emplace_back(pathView.data(), pos + 1);
                    pathView.remove_prefix(pos + 1);
                }
                else
                {
                    // We are at the end of the path with a file or else.
                    std::swap(pathView, pathSegments_.emplace_back());
                }
            }

            uri.remove_prefix(path_.size());

            return uri;
        }

        bool isPathSegmentCompliant(std::string_view const& segment) const
        {
            constexpr std::array ch{':', '@', '/' /* add / because it is kept with the segment*/};
            for (std::string_view::size_type i = 0; i < segment.size(); ++i)
            {
                if (checks::characters::is_unreserved(segment[i])
                    || checks::characters::is_subdelimiter(segment[i])
                    || std::any_of(
                        ch.begin(), ch.end(), [&h = segment[i]](auto const& c) { return c == h; }))
                {
                    continue;
                }

                // pct-encoded
                if (segment[i] == '%' && i < (segment.size() - 2)
                    && checks::characters::is_hex_digit(segment[i + 1])
                    && checks::characters::is_hex_digit(segment[i + 2]))
                {
                    i += 2;
                    continue;
                }

                return false;
            }

            return true;
        }

        std::string_view tryParseFragment(std::string_view uri)
        {
            // search for "/path/to/ressource#frag"...
            uri.remove_prefix(FRAG_SEP.size());

            // should be the last element in the URI to parse
            fragment_ = uri.substr(0u);

            uri.remove_prefix(fragment_.size());
            // uri should be totaly empty now

            return uri;
        }

        bool isFragmentCompliant(std::string_view const& fragment) const
        {
            if (fragment.empty())
            {
                return true;
            }

            constexpr std::array ch{':', '@', '/', '?'};
            for (std::string_view::size_type i = 0; i < fragment.size(); ++i)
            {
                if (checks::characters::is_unreserved(fragment[i])
                    || checks::characters::is_subdelimiter(fragment[i])
                    || std::any_of(
                        ch.begin(), ch.end(), [&h = fragment[i]](auto const& c) { return c == h; }))
                {
                    continue;
                }

                // pct-encoded
                if (fragment[i] == '%' && i < (fragment.size() - 2)
                    && checks::characters::is_hex_digit(fragment[i + 1])
                    && checks::characters::is_hex_digit(fragment[i + 2]))
                {
                    i += 2;
                    continue;
                }

                return false;
            }

            return true;
        }

        std::string_view tryParseQuery(std::string_view uri)
        {
            // search for "key=value&key2=value2"...
            queryLine_ = uri;
            if (auto pos = uri.find(FRAG_SEP); pos != strv::npos)
            {
                // pos begin at 0, so for a size, it's pos+1 and we want the previous one, so pos.
                queryLine_ = uri.substr(0u, pos);
            }

            strv queryView = queryLine_;

            while (!queryView.empty())
            {
                if (auto pos = queryView.find(QUERY_SEP); pos != strv::npos)
                {
                    auto queryPiece = queryView.substr(0u, pos);
                    if (auto equal = queryPiece.find(KV_SEP); equal == strv::npos)
                    {
                        throw std::runtime_error("error in a Key-Value pair in the query line.");
                    }
                    else
                    {
                        queries_.emplace(queryPiece.substr(0u, equal),
                            queryPiece.substr(equal + 1, queryPiece.size() - (equal + 1)));

                        queryView.remove_prefix(queryPiece.size());
                    }

                    queryView.remove_prefix(QUERY_SEP.size());
                }
                else
                {
                    // last piece of the query line, so this should let queryView in an empty state.
                    strv queryPiece;
                    std::swap(queryPiece, queryView);
                    if (auto equal = queryPiece.find(KV_SEP); equal == strv::npos)
                    {
                        throw std::runtime_error(
                            "No equal sign in a Key-Value pair on the query line.");
                    }
                    else
                    {
                        queries_.emplace(queryPiece.substr(0u, equal),
                            queryPiece.substr(equal + 1, queryPiece.size() - (equal + 1)));
                    }
                }
            }

            uri.remove_prefix(queryLine_.size());

            return uri;
        }

        bool isQueryCompliant(std::string_view const& query) const
        {
            // The rules to fragments apply also to queries.
            return isFragmentCompliant(query);
        }

        ParsingResult parse()
        {
            if (uri_.empty())
            {
                return ParsingResult::EmptyURI;
            }

            strv uriView{uri_};
            ParsingSteps step = ParsingSteps::Scheme;

            while (!uriView.empty())
            {
                switch (step)
                {
                    case utils::URI::ParsingSteps::Scheme: {
                        uriView = tryParseScheme(uriView);
                        step = ParsingSteps::CheckAuthority;
                        break;
                    }
                    case utils::URI::ParsingSteps::CheckAuthority: {
                        auto authAnon = (uriView.size() > AUTH_ANON.size()
                                                 && uriView.substr(0, AUTH_ANON.size()) == AUTH_ANON
                                             ? AUTH_ANON
                                             : strv{});
                        auto nxtPathSep = uriView.find(PATH_SEP, authAnon.size());

                        if (!authAnon.empty()
                            || nxtPathSep > 0 // means there is caracters until path's begin
                            || nxtPathSep == strv::npos // means there is no path at all
                            || uriView.substr(nxtPathSep).empty() // means there is an trailing separator
                        )
                        {
                            step = ParsingSteps::Authority;
                            if (!authAnon.empty())
                            {
                                uriView.remove_prefix(AUTH_ANON.size());
                            }
                        }
                        else
                        {
                            step = ParsingSteps::CheckSeparator;
                        }
                        break;
                    }
                    case utils::URI::ParsingSteps::Authority: {
                        uriView = tryParseAuthority(uriView);
                        step = ParsingSteps::CheckSeparator;
                        break;
                    }
                    case utils::URI::ParsingSteps::CheckSeparator: {
                        if (uriView.front() == PATH_SEP.front())
                        {
                            step = ParsingSteps::Path;
                        }
                        else if (uriView.front() == QUERY_ANON.front())
                        {
                            step = ParsingSteps::Query;
                            uriView.remove_prefix(QUERY_ANON.size());
                        }
                        else
                        {
                            step = ParsingSteps::Fragment;
                            uriView.remove_prefix(FRAG_SEP.size());
                        }
                        break;
                    }
                    case utils::URI::ParsingSteps::Path: {
                        uriView = tryParsePath(uriView);
                        step = ParsingSteps::CheckSeparator;
                        break;
                    }
                    case utils::URI::ParsingSteps::Query: {
                        uriView = tryParseQuery(uriView);
                        step = ParsingSteps::Fragment;
                        break;
                    }
                    case utils::URI::ParsingSteps::Fragment: {
                        uriView = tryParseFragment(uriView);
                        break;
                    }
                }
            }

            return ParsingResult::NoError;
        }

        std::string uri_;

        bool isAbsolutePath_ = false;

        strv scheme_;
        strv user_;
        strv host_;
        strv port_;
        strv path_;
        strv queryLine_;
        strv fragment_;

        std::vector<strv> pathSegments_;
        std::map<strv, strv> queries_;
    };
} // namespace uri
