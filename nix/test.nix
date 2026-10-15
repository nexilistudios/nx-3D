# Boots a VM with the NixOS module and checks that the server listens on the
# configured ports and stops cleanly on SIGTERM.
self: {
  name = "nx-3d-server";

  nodes.machine = {
    imports = [ self.nixosModules.default ];
    services.nx3dServer = {
      enable = true;
      tcpPort = 55200;
      udpPort = 55201;
    };
  };

  # The session port is only bound once a client authenticates.
  testScript = ''
    machine.wait_for_unit("nx3dServer.service")
    machine.wait_for_open_port(55200)
    machine.wait_until_succeeds("ss -Hlun 'sport = :55201' | grep -q .")
    machine.succeed("journalctl -u nx3dServer.service --grep 'game server is running'")

    machine.systemctl("stop nx3dServer.service")
    machine.succeed("systemctl show -P Result nx3dServer.service | grep -qx success")
  '';
}
