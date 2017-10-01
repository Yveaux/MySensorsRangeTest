#pragma once
#include "util.h"

class Radio
{
public:
  virtual void SetConfigDefault() = 0;
  virtual bool ConfigActivate() = 0;
  virtual String ConfigToString();
  virtual bool ConfigSerialize(uint8_t * out, size_t& size) = 0;
  virtual bool ConfigDeserialize(const uint8_t * in, const size_t size) = 0;

  virtual bool GetBaseId(uint8_t * out, size_t& size)  { (void)out; (void)size; return false; };
  virtual String BaseIdToString(const uint8_t * baseId, const size_t size) { (void)baseId; (void)size; return String(); };

  virtual int16_t GetRssi()     { return 0; };
  virtual int16_t GetRssiMin()  { return 0; };
  virtual int16_t GetRssiMax()  { return 0; };
  virtual int16_t GetRssiStep() { return 0; };
  virtual String RssiToString(const int16_t rssi) { return String(rssi); };

  virtual bool    SetChannel(const uint8_t channel) { (void)channel; return false; };
  virtual uint8_t GetChannel()     { return 0u; };
  virtual uint8_t GetChannelMin()  { return 0u; };
  virtual uint8_t GetChannelMax()  { return 0u; };
  virtual uint8_t GetChannelStep() { return 0u; };
  virtual String ChannelToString(const uint8_t channel) { return String(channel); };

  virtual bool    SetDataRate(const uint8_t rate) { (void)rate; return false; };
  virtual uint8_t GetDataRate()      { return 0u; };
  virtual uint8_t GetDataRateMin()   { return 0u; };
  virtual uint8_t GetDataRateMax()   { return 0u; };
  virtual uint8_t GetDataRateStep()  { return 0u; };
  virtual String DataRateToString(const uint8_t rate) { return String(rate); };

  virtual bool    SetPowerLevel(const uint8_t level) { (void)level; return false; };
  virtual uint8_t GetPowerLevel()     { return 0u; };
  virtual uint8_t GetPowerLevelMin()  { return 0u; };
  virtual uint8_t GetPowerLevelMax()  { return 0u; };
  virtual uint8_t GetPowerLevelStep() { return 0u; };
  virtual String PowerLevelToString(const uint8_t level) { return String(level); };

  virtual bool    SetAutoRetransmits(const uint8_t retransmits) { (void)retransmits; return false; };
  virtual uint8_t GetAutoRetransmits()      { return 0u; };
  virtual uint8_t GetAutoRetransmitsMin()   { return 0u; };
  virtual uint8_t GetAutoRetransmitsMax()   { return 0u; };
  virtual uint8_t GetAutoRetransmitsStep()  { return 0u; };
  virtual String AutoRetransmitsToString(const uint8_t retransmits) { return String(retransmits); };

  virtual bool    SetAutoRetransmitDelay(const uint8_t delay) { (void)delay; return false; };
  virtual uint8_t GetAutoRetransmitDelay()     { return 0u; };
  virtual uint8_t GetAutoRetransmitDelayMin()  { return 0u; };
  virtual uint8_t GetAutoRetransmitDelayMax()  { return 0u; };
  virtual uint8_t GetAutoRetransmitDelayStep() { return 0u; };
  virtual String AutoRetransmitDelayToString(const uint8_t delay) { return String(delay); };
};

