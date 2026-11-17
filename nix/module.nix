# NixOS options for the nx-3D dedicated server and game client.
self:
{
  config,
  lib,
  pkgs,
  utils,
  ...
}:

let
  packages = self.packages.${pkgs.stdenv.hostPlatform.system};
  server = config.services.nx3dServer;
  client = config.programs.nx3dClient;

  portOption =
    default: description:
    lib.mkOption {
      type = lib.types.port;
      inherit default description;
    };
in
{
  options.services.nx3dServer = {
    enable = lib.mkEnableOption "the nx-3D dedicated game server";

    package = lib.mkOption {
      type = lib.types.package;
      default = packages.server;
      defaultText = lib.literalMD "the `server` package of the nx-3D flake";
      description = "The nx-3D server package to run.";
    };

    tcpPort = portOption 54200 "TCP port for authentication.";
    udpPort = portOption 54201 "UDP port for gameplay traffic.";
    tcpSessionPort = portOption 54202 "TCP port for sessions.";

    tlsCertificateFile = lib.mkOption {
      type = lib.types.str;
      default = "/var/lib/nx3dServer/tls/server.crt";
      description = "PEM certificate for the server's Boost TCP TLS identity.";
    };
    tlsPrivateKeyFile = lib.mkOption {
      type = lib.types.str;
      default = "/var/lib/nx3dServer/tls/server.key";
      description = "Private key for the server's Boost TCP TLS identity.";
    };
    tlsPinFile = lib.mkOption {
      type = lib.types.str;
      default = "/var/lib/nx3dServer/tls/server.pin";
      description = "File where the server writes its public-key pin for local clients.";
    };

    openFirewall = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Whether to open the server ports in the firewall.";
    };
  };

  options.programs.nx3dClient = {
    enable = lib.mkEnableOption "the nx-3D game client";

    package = lib.mkOption {
      type = lib.types.package;
      default = packages.client;
      defaultText = lib.literalMD "the `client` package of the nx-3D flake";
      description = "The nx-3D client package to install.";
    };
  };

  config = lib.mkMerge [
    (lib.mkIf server.enable {
      systemd.services.nx3dServer = {
        description = "nx-3D dedicated game server";
        wantedBy = [ "multi-user.target" ];
        after = [ "network.target" ];

        # The server shuts down cleanly on SIGTERM.
        serviceConfig = {
          ExecStart = utils.escapeSystemdExecArgs [
            (lib.getExe server.package)
            "--tcp-port"
            server.tcpPort
            "--udp-port"
            server.udpPort
            "--tcp-session-port"
            server.tcpSessionPort
            "--tls-cert"
            server.tlsCertificateFile
            "--tls-key"
            server.tlsPrivateKeyFile
            "--tls-pin-file"
            server.tlsPinFile
          ];
          # The server generates its TLS identity on first start; keep it
          # writable across restarts despite ProtectSystem=strict.
          StateDirectory = "nx3dServer";
          WorkingDirectory = "/var/lib/nx3dServer";
          DynamicUser = true;
          Restart = "on-failure";
          RestartSec = 3;

          CapabilityBoundingSet = "";
          LockPersonality = true;
          MemoryDenyWriteExecute = true;
          PrivateDevices = true;
          ProcSubset = "pid";
          ProtectClock = true;
          ProtectControlGroups = true;
          ProtectHome = true;
          ProtectHostname = true;
          ProtectKernelLogs = true;
          ProtectKernelModules = true;
          ProtectKernelTunables = true;
          ProtectProc = "invisible";
          ProtectSystem = "strict";
          RestrictAddressFamilies = [
            "AF_INET"
            "AF_INET6"
            "AF_UNIX"
          ];
          RestrictNamespaces = true;
          RestrictRealtime = true;
          SystemCallArchitectures = "native";
          UMask = "0077";
        };
      };

      networking.firewall = lib.mkIf server.openFirewall {
        allowedTCPPorts = [
          server.tcpPort
          server.tcpSessionPort
        ];
        allowedUDPPorts = [ server.udpPort ];
      };
    })

    (lib.mkIf client.enable {
      environment.systemPackages = [ client.package ];
      # The Vulkan drivers come from here, not from the client's closure.
      hardware.graphics.enable = lib.mkDefault true;
    })
  ];
}
