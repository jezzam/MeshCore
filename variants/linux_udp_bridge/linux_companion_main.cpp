// Hybrid entry point: the companion_radio role (WiFi/TCP companion protocol
// for the phone app, contacts/chat/BaseChatMesh) running as a dedicated,
// always-relaying repeater (client_repeat defaulted on - see MyMesh.cpp),
// with a second, independent stdin-based plain-text CLI preserved alongside
// it for the SSH-based diagnostics workflow this project already depends on.
// Modeled on companion_radio/main.cpp's WiFi branch + simple_repeater/
// main.cpp's stdin command loop - both run concurrently against the same
// MyMesh instance.

#include <Arduino.h>
#include <Mesh.h>
#include "MyMesh.h"
#include "LinuxTcpInterface.h"

StdRNG fast_rng;
SimpleMeshTables tables;
DataStore store(LinuxFS, rtc_clock);
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store);
LinuxTcpInterface tcp_interface;

void halt() {
  while (1) ;
}

static char command[160];

void setup() {
  Serial.begin(115200);

  board.begin();

  if (!radio_init()) {
    MESH_DEBUG_PRINTLN("Radio init failed!");
    halt();
  }

  fast_rng.begin(radio_driver.getRngSeed());

  sensors.begin();

  the_mesh.begin(false);   // no display

  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  const char* e = getenv("MESHCORE_TCP_PORT");
  int tcp_port = e ? atoi(e) : 5000;
  tcp_interface.begin(tcp_port);
  tcp_interface.enable();
  the_mesh.startInterface(tcp_interface);
  Serial.printf("Companion protocol listening on TCP port %d\n", tcp_port);

  command[0] = 0;

  board.onBootComplete();
}

void loop() {
  int len = strlen(command);
  while (Serial.available() && len < sizeof(command) - 1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
      Serial.print(c);
    }
    if (c == '\r') break;
  }
  if (len == sizeof(command) - 1) {
    command[sizeof(command) - 1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {
    Serial.print('\n');
    command[len - 1] = 0;
    char reply[160];
    the_mesh.handleCLICommand(command, reply);
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }
    command[0] = 0;
  }

  the_mesh.loop();
  sensors.loop();
  rtc_clock.tick();
}
