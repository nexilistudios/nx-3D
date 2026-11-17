#include <shared/network_tls.hh>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <std::size_t Size>
nx3d::TlsOptions parse(char* (&arguments)[Size])
{
    return nx3d::parseTlsOptions(static_cast<int>(Size), arguments);
}

} // namespace

int main()
{
    const std::string pin(64, 'a');

    {
        char program[] = "nx3d";
        char* arguments[] = {program};
        const auto options = parse(arguments);
        require(options.certificatePath == "tls/server.crt", "Default TLS certificate path changed");
        require(options.privateKeyPath == "tls/server.key", "Default TLS private key path changed");
        require(options.pinPath == "tls/server.pin", "Default TLS pin path changed");
        require(options.pin.empty(), "Unexpected default TLS pin");
    }

    {
        char program[] = "nx3d";
        char certOption[] = "--tls-cert";
        char certValue[] = "/srv/nx3d/cert.pem";
        char keyOption[] = "-tls-key";
        char keyValue[] = "/srv/nx3d/key.pem";
        char pinOption[] = "--tls-pin";
        char pinValue[65] = {};
        pin.copy(pinValue, pin.size());
        char* arguments[] = {
                program,
                certOption, certValue,
                keyOption, keyValue,
                pinOption, pinValue};
        const auto options = parse(arguments);
        require(options.certificatePath == certValue, "TLS certificate path was not parsed");
        require(options.privateKeyPath == keyValue, "TLS private key path was not parsed");
        require(options.pin == pin, "TLS pin was not parsed");
        require(nx3d::isValidPublicKeyPin(options.pin), "Valid TLS pin was rejected");
    }

    {
        char program[] = "nx3d";
        char pinOption[] = "-tls-pin";
        char invalidPin[] = "abcd";
        char* arguments[] = {program, pinOption, invalidPin};
        bool rejected = false;
        try
        {
            parse(arguments);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        require(rejected, "Malformed TLS pin was accepted");
    }

    {
        char program[] = "nx3d";
        char keyOption[] = "--tls-key";
        char* arguments[] = {program, keyOption};
        bool rejected = false;
        try
        {
            parse(arguments);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        require(rejected, "TLS option without a value was accepted");
    }

    {
        const char* path = "nx3d-test-server.pin";
        {
            std::ofstream file(path);
            file << "  " << pin << "\n";
        }
        require(nx3d::readPinFile(path) == pin, "Pin file whitespace was not trimmed");
        std::remove(path);
        require(nx3d::readPinFile(path).empty(), "Missing pin file did not return an empty pin");
    }

    {
        const auto directory = std::filesystem::temp_directory_path() / "nx3d-pin-search-test";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory / "tls");
        const auto path = (directory / "tls" / "server.pin").string();

        nx3d::TlsOptions options;
        options.pinPath = path;
        require(nx3d::findServerPin(options).empty(), "Missing pin file produced a pin");

        {
            std::ofstream file(path);
            file << pin << "\n";
        }
        require(nx3d::findServerPin(options) == pin, "Pin in the configured path was not found");

        {
            std::ofstream file(path);
            file << "invalid\n";
        }
        require(nx3d::findServerPin(options).empty(), "Malformed pin file was accepted");

        std::filesystem::remove_all(directory);
    }
}
