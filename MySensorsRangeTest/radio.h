#pragma once
#include "util.h"

#define TO_MHZ(hz)  ((hz)*1000000ul)

class Radio
{
public:
  virtual void SetConfigDefault() = 0;
  virtual bool ConfigActivate() = 0;
  virtual String ConfigToString();
  virtual bool ConfigSerialize(uint8_t * out, size_t& size) = 0;
  virtual bool ConfigDeserialize(const uint8_t * in, const size_t size) = 0;

  virtual bool SanityCheck() = 0;

  virtual bool GetBaseId(uint8_t * out, size_t& size)  { (void)out; (void)size; return false; };
  virtual String BaseIdToString(const uint8_t * baseId, const size_t size) { (void)baseId; (void)size; return String(); };

  virtual int16_t GetRssi()     { return 0; };
  virtual int16_t GetRssiMin()  { return 0; };
  virtual int16_t GetRssiMax()  { return 0; };
  virtual int16_t GetRssiStep() { return 0; };
  virtual String  RssiToString(const int16_t rssi) { return String(rssi); };
  virtual bool    CanGetRssi()     { return GetRssiStep() != 0; };

  virtual bool     SetFrequencyHz(const uint32_t frequency) { (void)frequency; return false; };
  virtual uint32_t GetFrequencyHz()     { return 0ul; };
  virtual uint32_t GetFrequencyMinHz()  { return 0ul; };
  virtual uint32_t GetFrequencyMaxHz()  { return 0ul; };
  virtual uint32_t GetFrequencyStepHz() { return 0ul; };
  virtual String   FrequencyHzToString(const uint32_t frequency); // { return String(frequency); };
  virtual bool     CanSetFrequencyHz()     { return GetFrequencyStepHz() != 0ul; };

  virtual bool    SetDataRate(const uint8_t rate) { (void)rate; return false; };
  virtual uint8_t GetDataRate()      { return 0u; };
  virtual uint8_t GetDataRateMin()   { return 0u; };
  virtual uint8_t GetDataRateMax()   { return 0u; };
  virtual uint8_t GetDataRateStep()  { return 0u; };
  virtual String  DataRateToString(const uint8_t rate) { return String(rate); };
  virtual bool    CanSetDataRate()     { return GetDataRateStep() != 0u; };

  virtual bool   SetPowerLevel(const int8_t level) { (void)level; return false; };
  virtual int8_t GetPowerLevel()     { return 0; };
  virtual int8_t GetPowerLevelMin()  { return 0; };
  virtual int8_t GetPowerLevelMax()  { return 0; };
  virtual int8_t GetPowerLevelStep() { return 0; };
  virtual String PowerLevelToString(const int8_t level) { return String(level); };
  virtual bool   CanSetPowerLevel()  { return GetPowerLevelStep() != 0; };

  virtual bool    SetAutoRetransmits(const uint8_t retransmits) { (void)retransmits; return false; };
  virtual uint8_t GetAutoRetransmits()      { return 0u; };
  virtual uint8_t GetAutoRetransmitsMin()   { return 0u; };
  virtual uint8_t GetAutoRetransmitsMax()   { return 0u; };
  virtual uint8_t GetAutoRetransmitsStep()  { return 0u; };
  virtual String  AutoRetransmitsToString(const uint8_t retransmits) { return String(retransmits); };
  virtual bool    CanSetAutoRetransmits()  { return GetAutoRetransmitsStep() != 0u; };

  virtual bool    SetAutoRetransmitDelay(const uint8_t delay) { (void)delay; return false; };
  virtual uint8_t GetAutoRetransmitDelay()     { return 0u; };
  virtual uint8_t GetAutoRetransmitDelayMin()  { return 0u; };
  virtual uint8_t GetAutoRetransmitDelayMax()  { return 0u; };
  virtual uint8_t GetAutoRetransmitDelayStep() { return 0u; };
  virtual String  AutoRetransmitDelayToString(const uint8_t delay) { return String(delay); };
  virtual bool    CanSetAutoRetransmitDelay()  { return GetAutoRetransmitDelayStep() != 0u; };
};

String Radio::FrequencyHzToString(const uint32_t frequency)
{
  String s = String(frequency/1000000ul);
  s += "MHz";
  return s;
};
