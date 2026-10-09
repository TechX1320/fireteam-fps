# FIRETEAM - LAN discovery, blood impacts, death animations

Status: source committed to main; Windows compilation and runtime smoke tests still needed.

## LAN discovery (no port forwarding on the same LAN)

The standalone dedicated executable broadcasts a versioned FTLAN1 UDP announcement every ~2 seconds to 255.255.255.255 and 127.0.0.1 on discovery port **27888**. The launcher listens automatically and places responding servers near the top of Community Servers. The row reports the map, difficulty, and actual connected-player count through Jupiter GetNumClients(). Hosts expire from the list after 9 seconds without an announcement.

A LAN discovery packet contains an instance identifier, game port, max/current slots, difficulty, map, and server name. Joining uses the observed sender address and the advertised game port (normally **27889**), NOT UDP discovery port 27888. The local host also announces to loopback so its launcher can see itself. These packets are not authenticated; mod and ping compatibility remain unknown.

On a second PC sharing the same router/subnet, allow inbound UDP **27888** to the actual WinUI process under BUILT/Launcher/App in Windows Firewall. Guest network isolation, VLANs and VPNs can block broadcast. Allow the separate game port for gameplay traffic.

## Internet/public browsing still requires infrastructure

This does **NOT** add global Internet master-server registration, UPnP, NAT traversal or a relay. The existing config/public-servers.json is a static opt-in list. Other networks cannot receive LAN broadcasts. A lightweight hosted HTTPS heartbeat registry is needed for dynamic public discoverability, with explicit opt-in and expiring listings.

For Internet gameplay, players must ALSO be able to reach the game port, using user-enabled UPnP (automatic port mapping on compatible routers), manual router forwarding, or a relay/tunnel when CGNAT prevents incoming connections. UPnP does not provide server registration or universal NAT bypass; do not claim the game has Internet zero-configuration hosting yet. Never automatically publish someone's home IP to a public registry without clear consent.

## Blood impacts (new)

The authoritative hitscan damage path now passes the actual world hit position to its shooter's client. That client renders up to five lightweight red droplets at the impact for ~330ms, with 12 concurrent bursts maximum. No commercial DTX/SPR dependencies, permanent decals, or long-lived emitters. Existing center hit-marker behavior stays. Other clients do not yet see the burst; full multiplayer replication is future work.

## Special zombie death fallback (new)

The runtime checks the composed model for the configured death animation and common CA death names using GetAnimIndex. Only validated names are played; allow sufficient animation time before removing the corpse. If none are valid, topple and lower the non-solid corpse visibly. The procedural fall is NOT an original Combat Arms death animation.

Other Assassin/Tanker attack and movement animation mappings remain unverified. Their correct motion cannot be deduced from guessed names. For precise mapping, send the text reports from assets-local/Reports/ANI_VI_ASSASSIN_CH-strings.txt and ANI_VI_TANKER_SH-strings.txt, plus server log lines starting Fireteam special audit:. Attributes.zip can also be inspected by scripts/audit-assassin-assets.ps1.

## Windows QA

1. Stop any existing dedicated server process. From repository root run: git pull --ff-only
2. Run: build.cmd
3. Run: build-dedicated.cmd
4. Start from the updated launcher Dedicated tab, or run: run-dedicated.cmd
5. Look for console text: FIRETEAM LAN discovery: advertising to launchers on UDP 27888.
6. Launch the updated launcher. In SERVERS expect the local dedicated host under LAN within a few seconds, with a real player count.
7. Join on host machine by 127.0.0.1:27889. Next test another PC on same LAN. A successful game join is separate from seeing discovery.
8. Stop dedicated, wait 9-12 seconds, confirm its LAN row disappears.
9. In game: hit an infected to test X marker and short red world-space burst; shooting walls should not generate the blood burst.
10. Kill an Assassin/Tanker: watch actual death motion if found, or procedural fall, without breaking round counts or enemy navigation.

No Windows build or gameplay testing has been performed by the authoring assistant. Send the FIRST compiler error or relevant game log if any problem occurs.
