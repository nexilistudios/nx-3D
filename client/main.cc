#include <client/app/client_app.hh>

#include <nexilis/cmd_line_options.hh>
#include <nexilis/ports.hh>

#include <shared/network_ports.hh>

#include <algorithm>
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
    nx3d::client::ClientApp app(serverAddress, username, argc > 1 && !previewMap && !editMap,
                                previewMap, editMap, mapPath);
    app.run();

    return 0;
}
