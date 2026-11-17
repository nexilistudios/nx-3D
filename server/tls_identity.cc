#include <server/tls_identity.hh>

#include <nexilis/tls.hh>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <system_error>

namespace nx3d::server
{
namespace
{

void setPermissions(const std::string& path, bool privateKey)
{
    const std::filesystem::perms permissions =
            privateKey ? std::filesystem::perms::owner_read | std::filesystem::perms::owner_write
                       : std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
                                 std::filesystem::perms::group_read | std::filesystem::perms::others_read;
    std::error_code error;
    std::filesystem::permissions(path, permissions, std::filesystem::perm_options::replace, error);
    if (error)
        std::cerr << "[TLS] Could not set permissions on " << path << ": " << error.message() << std::endl;
}

void writeFile(const std::string& path, const std::string& contents, bool privateKey)
{
    const auto filePath = std::filesystem::path(path);
    if (filePath.has_parent_path() && !filePath.parent_path().empty())
        std::filesystem::create_directories(filePath.parent_path());

    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file)
        throw std::runtime_error("Cannot write TLS file " + path);
    file << contents;
    file.close();
    if (!file)
        throw std::runtime_error("Cannot write TLS file " + path);

    setPermissions(path, privateKey);
}

} // namespace

TlsIdentity loadOrCreateTlsIdentity(const TlsOptions& options)
{
    const bool hasCertificate = std::filesystem::exists(options.certificatePath);
    const bool hasPrivateKey = std::filesystem::exists(options.privateKeyPath);
    if (hasCertificate != hasPrivateKey)
        throw std::runtime_error("Provide both a TLS certificate and its private key (" +
                                 options.certificatePath + " and " + options.privateKeyPath + ")");

    TlsIdentity identity;
    if (hasCertificate)
    {
        identity.certificatePem = readTextFile(options.certificatePath);
        identity.privateKeyPem = readTextFile(options.privateKeyPath);
        identity.publicKeySha256 = nexilis::tls::publicKeySha256(identity.certificatePem);
        if (identity.privateKeyPem.empty() || identity.publicKeySha256.empty())
            throw std::runtime_error("TLS certificate or private key in " + options.certificatePath +
                                     " and " + options.privateKeyPath + " is invalid");
        setPermissions(options.privateKeyPath, true);
    }
    else
    {
        const auto generated = nexilis::tls::generateSelfSignedIdentity();
        if (generated.publicKeySha256.empty())
            throw std::runtime_error("Could not generate a TLS identity");
        identity.certificatePem = generated.certificatePem;
        identity.privateKeyPem = generated.privateKeyPem;
        identity.publicKeySha256 = generated.publicKeySha256;
        identity.generated = true;

        writeFile(options.certificatePath, identity.certificatePem, false);
        writeFile(options.privateKeyPath, identity.privateKeyPem, true);
    }

    if (!nexilis::tls::createServerContext(identity.certificatePem, identity.privateKeyPem))
        throw std::runtime_error("TLS certificate and private key do not match");

    // Publish the pin for local clients. Also mirror it to the checkout/data
    // root so the client works even when the two binaries run from different
    // directories (for example server/build and client/build).
    if (!options.pinPath.empty())
    {
        const std::string contents = identity.publicKeySha256 + "\n";
        try
        {
            writeFile(options.pinPath, contents, false);
        }
        catch (const std::exception& error)
        {
            std::cerr << "[TLS] Could not write pin file " << options.pinPath << ": " << error.what() << std::endl;
        }

        const auto shared = projectPinPath();
        if (options.pinPath == "tls/server.pin" && !shared.empty() && shared != options.pinPath)
        {
            try
            {
                writeFile(shared, contents, false);
            }
            catch (const std::exception& error)
            {
                std::cerr << "[TLS] Could not mirror pin to " << shared << ": " << error.what() << std::endl;
            }
        }
    }

    return identity;
}

} // namespace nx3d::server
