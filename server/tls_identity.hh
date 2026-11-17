#ifndef NX3D_SERVER_TLS_IDENTITY_HH
#define NX3D_SERVER_TLS_IDENTITY_HH

#include <shared/network_tls.hh>

#include <string>

namespace nx3d::server
{

struct TlsIdentity
{
    std::string certificatePem;
    std::string privateKeyPem;
    std::string publicKeySha256;
    bool generated = false;
};

/// Load the configured PEM identity, generating and persisting a self-signed
/// one on first start. The pin is also written for local clients to read.
/// Throws std::runtime_error when the configuration is incomplete or invalid.
TlsIdentity loadOrCreateTlsIdentity(const TlsOptions& options);

} // namespace nx3d::server

#endif
