#include <client/chat/chat_text.hh>

#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    using namespace nx3d::client::chat;

    const std::string mixed = "aöä漢🚀";
    require(characterCount(mixed) == 5 && truncate(mixed, 3) == "aöä" &&
                    tail(mixed, 2) == "漢🚀",
            "UTF-8 truncation split a character");

    auto edited = mixed;
    popBack(edited);
    require(edited == "aöä漢", "Backspace split a four-byte character");
    popBack(edited);
    require(edited == "aöä", "Backspace split a three-byte character");
    popBack(edited);
    require(edited == "aö", "Backspace split a Finnish character");

    std::string input;
    appendWithinByteLimit(input, "öä🚀", 5);
    require(input == "öä", "Byte limit cut a pasted character");
    appendWithinLimit(input, "漢🚀", 3);
    require(input == "öä漢", "Character limit cut a pasted character");

    const std::string malformed{"\xc3\x28", 2};
    require(sanitize(malformed) == "\xef\xbf\xbd(" &&
                    characterCount(malformed) == 2 &&
                    truncate(malformed, 1) == "\xef\xbf\xbd",
            "Malformed UTF-8 was passed through to text rendering");
    require(sanitize("\xed\xa0\x80") ==
                    "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd",
            "UTF-8 surrogate was accepted");

    const auto lines = wrapMessage("Mägi", "漢字🚀漢字🚀", 8);
    require(lines.size() >= 2, "Unicode chat text did not wrap");
    for (const auto& line : lines)
        require(sanitize(line) == line && characterCount(line) <= 8,
                "Chat wrapping split a Unicode character");
}
