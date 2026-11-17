#include <server/tls_identity.hh>

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

} // namespace

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "nx3d-tls-identity-test";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    nx3d::TlsOptions options;
    options.certificatePath = (directory / "server.crt").string();
    options.privateKeyPath = (directory / "server.key").string();
    options.pinPath = (directory / "server.pin").string();

    const auto generated = nx3d::server::loadOrCreateTlsIdentity(options);
    require(generated.generated, "First start did not generate an identity");
    require(nx3d::isValidPublicKeyPin(generated.publicKeySha256), "Generated pin is malformed");
    require(std::filesystem::exists(options.certificatePath), "Certificate was not persisted");
    require(std::filesystem::exists(options.privateKeyPath), "Private key was not persisted");
    require(nx3d::readPinFile(options.pinPath) == generated.publicKeySha256, "Pin file does not match the identity");

    const auto loaded = nx3d::server::loadOrCreateTlsIdentity(options);
    require(!loaded.generated, "Existing identity was replaced");
    require(loaded.publicKeySha256 == generated.publicKeySha256, "Pin changed between restarts");

    {
        std::ofstream file(options.certificatePath);
        file << "not a certificate\n";
    }
    bool rejected = false;
    try
    {
        nx3d::server::loadOrCreateTlsIdentity(options);
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    require(rejected, "Invalid certificate was accepted");

    std::filesystem::remove(options.privateKeyPath);
    rejected = false;
    try
    {
        nx3d::server::loadOrCreateTlsIdentity(options);
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    require(rejected, "A certificate without its private key was accepted");

    std::filesystem::remove_all(directory);
}
