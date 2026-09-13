#include <client/app/client_app.hh>

#include <nexilis/cmd_line_options.hh>

#include <string>

int main(int argc, char* argv[])
{
    // Parse --server argument
    nexilis::CmdLineOptions cmdLine(argc, argv);
    std::string serverAddress = cmdLine.getValue<std::string>("-server", "127.0.0.1");

    nx3d::client::ClientApp app(serverAddress, argc > 1);
    app.run();

    return 0;
}