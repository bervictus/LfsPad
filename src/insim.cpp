#include "runtime.hpp"
#include "core.hpp"
#include <algorithm>
#include <cstring>
#include "insim_packets.hpp"
namespace pad {
static SOCKET socketFd = INVALID_SOCKET;
static bool connecting = false, ready = false;
static ULONGLONG retry = 0, last = 0;
static ULONGLONG lastState = 0;
static SimState state;
static ReplayState replay;
static ULONGLONG nextReplayQuery = 0, lastReplayInfo = 0, seekDeadline = 0;
static unsigned char seekRequest = 0;
constexpr unsigned char replayQuery = 250;

static DrivingTelemetry carTelemetry;
static ULONGLONG nextCarQuery = 0;
static unsigned telemetryPlayer = 0;
static void disconnect() {
  if (socketFd != INVALID_SOCKET)
    closesocket(socketFd);
  socketFd = INVALID_SOCKET;
  connecting = ready = false;
  state = {};
  replay = {};
  carTelemetry = {};
  nextCarQuery = 0;
  telemetryPlayer = 0;
  nextReplayQuery = lastReplayInfo = seekDeadline = 0;
  retry = GetTickCount64() + 2000;
}
static bool sendPacket(const unsigned char *packet, int size) {
  return send(socketFd, reinterpret_cast<const char *>(packet), size, 0) == size;
}
static bool tiny(unsigned char sub) {
  const unsigned char packet[] = {1, 3, 1, sub};
  return sendPacket(packet, sizeof(packet));
}
static bool sendNow(const unsigned char *packet, int size) {
  return ready && state.known && sendPacket(packet, size);
}

unsigned short inSimPort() {
  static const auto port = [] {
    const auto value = GetPrivateProfileIntW(
        L"General", L"InSimPort", 29999, path(L"LfsPad.ini").c_str());
    return static_cast<unsigned short>(value > 0 && value <= 65535 ? value : 29999);
  }();
  return port;
}
DrivingTelemetry pollInSimCarTelemetry(bool enabled) {
  const auto player = enabled ? state.localPlayer : 0;
  if (player != telemetryPlayer) {
    carTelemetry = {};
    nextCarQuery = 0;
    telemetryPlayer = player;
  }
  if (telemetryPlayer) {
    const auto now = GetTickCount64();
    if (now >= nextCarQuery) {
      // IS_SMALL / SMALL_AII only requests existing car data.
      const unsigned char query[]{2, 4, rumbleCarRequest, 11,
                                  static_cast<unsigned char>(telemetryPlayer), 0, 0, 0};
      sendNow(query, sizeof(query));
      nextCarQuery = now + 40;
    }
  }
  return carTelemetry;
}
bool simCommand(const char *command) {
  unsigned char packet[68]{17, 13};
  auto length = std::strlen(command);
  if (length >= sizeof(packet) - 4)
    return false;
  std::memcpy(packet + 4, command, length);
  return sendNow(packet, sizeof(packet));
}
ReplayState replayState() { return replay; }
bool seekReplay(int direction) {
  if (!replay.active || replay.seeking)
    return false;
  unsigned destination = seekTime(replay.position, replay.duration, direction);
  if (destination == replay.position)
    return false;
  seekRequest = seekRequest % 249 + 1;
  unsigned char packet[80]{20, 48, seekRequest, 0,
                           static_cast<unsigned char>(replay.multiplayer),
                           static_cast<unsigned char>(bool(state.flags & 4))};
  std::memcpy(packet + 8, &destination, sizeof(destination));
  if (!sendNow(packet, sizeof(packet)))
    return false;
  replay.seeking = true;
  seekDeadline = GetTickCount64() + 8000;
  log("Replay seek: %u -> %u ms", replay.position, destination);
  return true;
}
SimState pollSim() {
  static bool started = []() {
    WSADATA w;
    int rc = WSAStartup(MAKEWORD(2, 2), &w);
    log("WSAStartup=%d", rc);
    return rc == 0;
  }();
  if (!started)
    return {};
  auto now = GetTickCount64();
  if (socketFd == INVALID_SOCKET) {
    if (now < retry)
      return {};
    socketFd = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0,
                          WSA_FLAG_NO_HANDLE_INHERIT);
    if (socketFd == INVALID_SOCKET) {
      retry = now + 2000;
      return {};
    }
    u_long nonblock = 1;
    if (ioctlsocket(socketFd, FIONBIO, &nonblock) != 0) {
      disconnect();
      return {};
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(inSimPort());
    int rc = connect(socketFd, (sockaddr *)&a, sizeof(a));
    if (rc != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
      log("InSim connect error=%d", WSAGetLastError());
      disconnect();
      return {};
    }
    connecting = true;
    last = now;
  }
  if (connecting) {
    unsigned char init[44] = {11, 1, 1, 0, 0, 0, 4, 0, 10};
    std::memcpy(init + 28, "LfsPad", 6);
    if (!sendPacket(init, sizeof(init))) {
      if (WSAGetLastError() != WSAEWOULDBLOCK || now - last > 5000)
        disconnect();
      return {};
    }
    tiny(7);
    tiny(14); // TINY_NPL: discover an already active local player when reconnecting.
    connecting = false;
    ready = true;
    last = lastState = now;
    log("Local InSim UDP initialisation sent");
  }
  if (ready) {
    if (now - lastState > 5000) {
      log("InSim state timed out; menu input disabled");
      disconnect();
      return {};
    }
    if (now - last > 1000) {
      tiny(7);
      last = now;
    }
    unsigned char datagram[4096];
    for (int reads = 0; reads < 8; ++reads) {
      const int bytes = recv(socketFd, reinterpret_cast<char *>(datagram), sizeof(datagram), 0);
      if (bytes < 0) {
        const int error = WSAGetLastError();
        if (error == WSAEMSGSIZE)
          continue;
        if (error != WSAEWOULDBLOCK)
          disconnect();
        break;
      }
      if (!validInSimDatagram(datagram, bytes))
        continue;
      for (int offset = 0; offset < bytes;) {
        const auto *p = datagram + offset;
        const unsigned n = p[0] * 4u;
        offset += n;
        if (p[1] == 5 && n == 28) {
          state.flags = (unsigned short)(p[8] | p[9] << 8);
          state.viewedPlayer = p[11];
          state.known = true;
          lastState = now;
        }
        // IS_NPL marks AI and remote drivers in PType. OutSim in single
        // player can follow an AI, so use the viewed local human only.
        if (p[1] == 21 && n == 76 && !(p[5] & (2 | 4)) && p[73])
          state.localPlayer = p[3];
        if (((p[1] == 22 || p[1] == 23) && n == 4 && p[3] == state.localPlayer) ||
            (p[1] == 3 && n == 4 && (p[3] == 9 || p[3] == 11 || p[3] == 12)))
          state.localPlayer = 0;
        if (p[1] == 31 && n == 8) {
          if (p[3] == state.localPlayer)
            state.localPlayer = 0;
          tiny(14);
        }
        if (telemetryPlayer && telemetryPlayer == state.localPlayer &&
            decodeInSimRumble(p, n, telemetryPlayer, static_cast<std::uint32_t>(now),
                              carTelemetry.sample))
          carTelemetry.received = now;
        if (p[1] == 64 && n == 8 && p[3] == 0) {
          state.mode = p[4];
          state.subMode = p[5];
        }
        if (p[1] == 48 && n == 80 &&
            (p[2] == replayQuery || (replay.seeking && p[2] == seekRequest))) {
          if (p[2] == seekRequest) {
            log("Replay seek reply: error=%u", p[3]);
            replay.seeking = false;
          } else if (replay.seeking) {
            continue;
          }
          std::memcpy(&replay.position, p + 8, sizeof(replay.position));
          std::memcpy(&replay.duration, p + 12, sizeof(replay.duration));
          replay.active = p[3] <= 1 && p[16] && replay.duration;
          replay.confirmedAbsent = p[3] == 4 && !p[16];
          replay.multiplayer = p[4] != 0;
          replay.paused = p[5] != 0;
          lastReplayInfo = now;
        }
        if (p[1] == 3 && n == 4 && p[3] == 0)
          sendPacket(p, 4);
      }
    }
  }
  if (!state.known || !(state.flags & (1 | 2)) || (state.flags & 256)) {
    replay = {};
    nextReplayQuery = 0;
  } else {
    if (replay.seeking && now >= seekDeadline) {
      replay = {};
      log("Replay seek timed out");
    }
    if (!replay.seeking && now - lastReplayInfo > 1400) {
      replay.active = false;
      replay.confirmedAbsent = false;
    }
    if (!replay.seeking && now >= nextReplayQuery) {
      const unsigned char query[]{1, 3, replayQuery, 22};
      sendNow(query, sizeof(query));
      nextReplayQuery = now + (replay.active || (state.flags & 2) ? 250 : 600);
    }
  }
  return state;
}
void simKey(const char *key) {
  // InSim /press does not dismiss every native text dialog. Use the same
  // foreground keyboard message path as a physical Escape/Return instead.
  if (std::strcmp(key, "esc") == 0)
    localKey(VK_ESCAPE);
  else if (std::strcmp(key, "enter") == 0)
    localKey(VK_RETURN);
}
} // namespace pad
