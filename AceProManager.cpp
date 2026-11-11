#include "AceProManager.h"

#include <Arduino.h>

#include "config.h"

void AceProManager::setup() {
  Serial.println(F("AceProManager::setup: Initializing ACE Pro connection..."));
  Serial.print(F("AceProManager::setup: USB pins - D+: "));
  Serial.print(USB_D_PLUS_PIN);
  Serial.print(F(", D-: "));
  Serial.println(USB_D_MINUS_PIN);

  Serial.println(F("AceProManager::setup: USB communication is not implemented yet."));

  Serial.println(F("AceProManager::setup: Initializing ACE Pro connection DONE."));
}

AceProManager::Status AceProManager::getStatus() const {
  Serial.println(F("AceProManager::getStatus: USB communication is not implemented; returning default status."));
  return _status;
}
