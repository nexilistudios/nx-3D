#include <client/app/client_app.hh>

#include <nexilis/cmd_line_options.hh>

#include <string>

int main(int argc, char* argv[])
{
    // Parse --server and --username arguments
    nexilis::CmdLineOptions cmdLine(argc, argv);
    std::string serverAddress = cmdLine.getValue<std::string>("-server", "127.0.0.1");
    std::string username = cmdLine.getValue<std::string>("-username", "");

    nx3d::client::ClientApp app(serverAddress, username, argc > 1);
    app.run();

    return 0;
}