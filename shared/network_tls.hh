#ifndef NX3D_SHARED_NETWORK_TLS_HH
#define NX3D_SHARED_NETWORK_TLS_HH

#include <shared/network_ports.hh>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace nx3d
{

/// Paths are relative to the process working directory unless overridden.
struct TlsOptions
{
    std::string certificatePath = "tls/server.crt";
    std::string privateKeyPath = "tls/server.key";
    std::string pinPath = "tls/server.pin";
    std::string pin;
};

/// SHA-256 of a DER public key, as accepted by nexilis: 64 hex digits.
inline bool isValidPublicKeyPin(std::string_view pin)
{
    if (pin.size() != 64)
        return false;
    for (const char character : pin)
    {
        const bool hex = (character >= '0' && character <= '9') ||
                         (character >= 'a' && character <= 'f') ||
                         (character >= 'A' && character <= 'F');
        if (!hex)
            return false;
    }
    return true;
}

/// Parse the TLS identity and pin options, leaving other program options alone.
/// Both single- and double-dash forms are accepted, like the port options.
inline TlsOptions parseTlsOptions(int argc, char* const argv[])
{
    TlsOptions options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument(argv[i]);
        std::string* destination = nullptr;

        if (isOption(argument, "-tls-cert"))
            destination = &options.certificatePath;
        else if (isOption(argument, "-tls-key"))
            destination = &options.privateKeyPath;
        else if (isOption(argument, "-tls-pin-file"))
            destination = &options.pinPath;
        else if (isOption(argument, "-tls-pin"))
            destination = &options.pin;

        if (!destination)
            continue;
        if (i + 1 >= argc)
            throw std::invalid_argument(std::string(argument) + " requires a value");

        *destination = argv[++i];
    }

    if (!options.pin.empty() && !isValidPublicKeyPin(options.pin))
        throw std::invalid_argument("tls-pin must be 64 hexadecimal digits");

    return options;
}

/// Read a whole text file, or return an empty string when it cannot be opened.
inline std::string readTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        return {};
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

/// Read a pin written by the server, tolerating surrounding whitespace.
inline std::string readPinFile(const std::string& path)
{
    const std::string contents = readTextFile(path);
    const std::size_t first = contents.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const std::size_t last = contents.find_last_not_of(" \t\r\n");
    return contents.substr(first, last - first + 1);
}

/// Pin mirrored next to the compiled-in game data (the checkout root in local
/// builds). Shared by the server and client so their working directories can differ.
inline std::string projectPinPath()
{
#ifdef PROJECT_ROOT
    return (std::filesystem::path(PROJECT_ROOT) / "tls" / "server.pin").string();
#else
    return {};
#endif
}

/// Places where a locally started server may have published its pin. The
/// current directory comes first, followed by the shared project root and
/// directories relative to the client executable (for separate build folders).
inline std::vector<std::string> pinSearchPaths(const TlsOptions& options, const char* executable = nullptr)
{
    std::vector<std::string> paths;
    const auto add = [&paths](const std::string& path)
    {
        if (!path.empty() && std::find(paths.begin(), paths.end(), path) == paths.end())
            paths.push_back(path);
    };

    add(options.pinPath.empty() ? "tls/server.pin" : options.pinPath);
    const auto shared = projectPinPath();
    if (options.pinPath.empty() || options.pinPath == "tls/server.pin")
        add(shared);

    if (executable)
    {
        const std::filesystem::path directory = std::filesystem::path(executable).parent_path();
        if (!directory.empty())
        {
            add((directory / "tls" / "server.pin").string());
            add((directory / ".." / "tls" / "server.pin").string());
            add((directory / ".." / ".." / "tls" / "server.pin").string());
        }
    }
    return paths;
}

/// First valid pin from the search paths, or an empty string when none exists.
inline std::string findServerPin(const TlsOptions& options, const char* executable = nullptr)
{
    for (const auto& path : pinSearchPaths(options, executable))
    {
        const std::string pin = readPinFile(path);
        if (isValidPublicKeyPin(pin))
            return pin;
    }
    return {};
}

} // namespace nx3d

#endif
