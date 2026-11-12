#ifndef NX3D_CLIENT_CHAT_CHAT_TEXT_HH
#define NX3D_CLIENT_CHAT_CHAT_TEXT_HH

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nx3d::client::chat
{

inline constexpr std::size_t kMaxMessageCharacters = 200;
inline constexpr std::size_t kLineCharacters = 34;

inline bool isUtf8Continuation(unsigned char byte)
{
    return (byte & 0xc0u) == 0x80u;
}

// Return the length of the next valid Unicode scalar value. Treat a malformed
// byte as one replacement character so callers always make forward progress.
inline std::size_t sequenceLength(std::string_view text, std::size_t offset)
{
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80u)
        return 1;
    const std::size_t length = first >= 0xc2u && first <= 0xdfu ? 2 :
                               first >= 0xe0u && first <= 0xefu ? 3 :
                               first >= 0xf0u && first <= 0xf4u ? 4 : 0;
    if (!length || length > text.size() - offset)
        return 0;
    for (std::size_t i = 1; i < length; ++i)
        if (!isUtf8Continuation(static_cast<unsigned char>(text[offset + i])))
            return 0;
    const auto second = static_cast<unsigned char>(text[offset + 1]);
    if ((first == 0xe0u && second < 0xa0u) ||
        (first == 0xedu && second >= 0xa0u) ||
        (first == 0xf0u && second < 0x90u) ||
        (first == 0xf4u && second >= 0x90u))
        return 0;
    return length;
}

inline std::string sanitize(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size();)
    {
        const auto length = sequenceLength(text, i);
        if (length)
            result.append(text.substr(i, length));
        else
            result += "\xef\xbf\xbd";
        i += length ? length : 1;
    }
    return result;
}

inline std::size_t characterCount(std::string_view text)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size(); ++count)
        i += std::max<std::size_t>(1, sequenceLength(text, i));
    return count;
}

inline std::size_t byteOffsetForCharacters(std::string_view text, std::size_t characters)
{
    if (characters == 0)
        return 0;

    std::size_t offset = 0;
    for (std::size_t count = 0; count < characters && offset < text.size(); ++count)
        offset += std::max<std::size_t>(1, sequenceLength(text, offset));
    return offset;
}

inline std::string truncate(std::string_view text, std::size_t maxCharacters)
{
    const auto valid = sanitize(text);
    return valid.substr(0, byteOffsetForCharacters(valid, maxCharacters));
}

inline void appendWithinLimit(std::string& destination,
                              std::string_view addition,
                              std::size_t maxCharacters = kMaxMessageCharacters)
{
    destination = sanitize(destination);
    const auto validAddition = sanitize(addition);
    const auto used = characterCount(destination);
    if (used >= maxCharacters)
        return;
    destination.append(validAddition.substr(0, byteOffsetForCharacters(validAddition, maxCharacters - used)));
}

inline void appendWithinByteLimit(std::string& destination,
                                  std::string_view addition,
                                  std::size_t maxBytes)
{
    destination = sanitize(destination);
    const auto validAddition = sanitize(addition);
    if (destination.size() >= maxBytes)
        return;
    std::size_t offset = 0;
    while (offset < validAddition.size())
    {
        const auto length = sequenceLength(validAddition, offset);
        if (destination.size() + offset + length > maxBytes)
            break;
        offset += length;
    }
    destination.append(validAddition.substr(0, offset));
}

inline std::string limitBytes(std::string_view text, std::size_t maxBytes)
{
    std::string result;
    appendWithinByteLimit(result, text, maxBytes);
    return result;
}

inline void popBack(std::string& text)
{
    if (text.empty())
        return;
    text = sanitize(text);
    const auto count = characterCount(text);
    const auto offset = byteOffsetForCharacters(text, count - 1);
    text.erase(offset);
}

inline std::string tail(std::string_view text, std::size_t maxCharacters)
{
    const auto valid = sanitize(text);
    const auto count = characterCount(valid);
    if (count <= maxCharacters)
        return valid;
    const auto offset = byteOffsetForCharacters(valid, count - maxCharacters);
    return valid.substr(offset);
}

inline std::vector<std::string> wrapMessage(const std::string& username,
                                            const std::string& payload,
                                            std::size_t lineCharacters = kLineCharacters)
{
    lineCharacters = std::max<std::size_t>(1, lineCharacters);
    std::string remaining = sanitize(username) + ": " + truncate(payload, kMaxMessageCharacters);
    for (char& character : remaining)
    {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20u || byte == 0x7fu)
            character = ' ';
    }

    std::vector<std::string> lines;
    while (characterCount(remaining) > lineCharacters)
    {
        const auto limit = byteOffsetForCharacters(remaining, lineCharacters);
        auto split = remaining.rfind(' ', limit - 1);
        if (split == std::string::npos || split == 0)
            split = limit;

        lines.push_back(remaining.substr(0, split));
        remaining.erase(0, split);
        while (!remaining.empty() && remaining.front() == ' ')
            remaining.erase(remaining.begin());
    }
    if (!remaining.empty())
        lines.push_back(std::move(remaining));
    return lines;
}

} // namespace nx3d::client::chat

#endif
