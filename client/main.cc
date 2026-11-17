#include <client/app/client_app.hh>

#include <nexilis/cmd_line_options.hh>
#include <nexilis/ports.hh>

#include <shared/network_ports.hh>
#include <shared/network_tls.hh>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char* argv[])
{
    try
    {
        const auto ports = nx3d::parseNetworkPorts(argc, argv);
        nexilis::Ports::setBoostTCPPort(ports.tcp);
        nexilis::Ports::setBoostUDPPort(ports.udp);
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "Invalid network port: " << error.what() << std::endl;
        return 1;
    }

    nx3d::TlsOptions tlsOptions;
    try
    {
        tlsOptions = nx3d::parseTlsOptions(argc, argv);
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "Invalid TLS option: " << error.what() << std::endl;
        return 1;
    }

    // Parse --server and --username arguments
    nexilis::CmdLineOptions cmdLine(argc, argv);
    std::string serverAddress = cmdLine.getValue<std::string>("-server", "127.0.0.1");
    std::string username = cmdLine.getValue<std::string>("-username", "");

    const auto hasOption = [&](std::string_view option)
    { return std::any_of(argv + 1, argv + argc, [&](const char* arg)
                         { return std::string_view(arg) == option; }); };
    const bool editMap = hasOption("--map-editor");
    const bool previewMap = hasOption("--map-preview");
    std::string mapPath;
    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]) == "--map")
        {
            if (++i >= argc)
            {
                std::cerr << "--map requires a map file path\n";
                return 1;
            }
            mapPath = argv[i];
        }
    // Nexilis pins the server's public key for Boost TCP TLS. Use the command
    // line or NX3D_TLS_PIN for a remote server; a local server writes its pin
    // to tls/server.pin on first start.
    std::string tlsPin = tlsOptions.pin;
    if (tlsPin.empty())
    {
        const char* environmentPin = std::getenv("NX3D_TLS_PIN");
        if (environmentPin)
            tlsPin = environmentPin;
    }
    if (tlsPin.empty())
        tlsPin = nx3d::findServerPin(tlsOptions, argv[0]);
    if (!nx3d::isValidPublicKeyPin(tlsPin))
    {
        if (previewMap || editMap)
            tlsPin.clear();
        else
        {
            std::cerr << "A valid server public-key pin is required for Boost TCP TLS.\n"
                      << "Start the server once to create tls/server.pin, or pass "
                         "-tls-pin <64 hex digits>.\n"
                      << "Searched:";
            for (const auto& path : nx3d::pinSearchPaths(tlsOptions, argv[0]))
                std::cerr << " " << path;
            std::cerr << std::endl;
            return 1;
        }
    }

    nx3d::client::ClientApp app(serverAddress, username, argc > 1 && !previewMap && !editMap,
                                previewMap, editMap, mapPath, tlsPin);
    app.run();

    return 0;
}
