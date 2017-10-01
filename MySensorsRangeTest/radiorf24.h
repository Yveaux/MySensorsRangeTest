#pragma once

#include "radio.h"

class RadioRF24 : public Radio
{
public:
  RadioRF24();
  void SetConfigDefault() override;
  bool ConfigActivate() override;
  String ConfigToString() override;
  bool ConfigSerialize(uint8_t * out, size_t& size) override;
  bool ConfigDeserialize(const uint8_t * in, const size_t size) override;

  bool GetBaseId(uint8_t * out, size_t& size) override;
  String BaseIdToString(const uint8_t * baseId, const size_t size) override;

  int16_t GetRssi() override;
  int16_t GetRssiMin() override  { return -149; };
  int16_t GetRssiMax() override  { return -29; };
  int16_t GetRssiStep() override { return 8; };

  bool    SetChannel(const uint8_t channel) override;
  uint8_t GetChannel() override     { return m_radioConfig.m_channel; };
  uint8_t GetChannelMin() override  { return 0u; };
  uint8_t GetChannelMax() override  { return 125u; };
  uint8_t GetChannelStep() override { return 1u; };

  bool    SetDataRate(const uint8_t rate) override;
  uint8_t GetDataRate() override      { return m_radioConfig.m_dataRate; };
  uint8_t GetDataRateMin() override   { return 0u; };
  uint8_t GetDataRateMax() override   { return 2u; };
  uint8_t GetDataRateStep() override  { return 1u; };
  String DataRateToString(const uint8_t rate) override;

  bool    SetPowerLevel(const uint8_t level) override;
  uint8_t GetPowerLevel() override     { return m_radioConfig.m_paLevel; };
  uint8_t GetPowerLevelMin() override  { return 0u; };
  uint8_t GetPowerLevelMax() override  { return 3u; };
  uint8_t GetPowerLevelStep() override { return 1u; };
  String PowerLevelToString(const uint8_t level) override;

  bool   SetAutoRetransmits(const uint8_t retransmits) override;
  uint8_t GetAutoRetransmits() override      { return m_radioConfig.m_retransmits; };
  uint8_t GetAutoRetransmitsMin() override   { return 0u; };
  uint8_t GetAutoRetransmitsMax() override   { return 15u; };
  uint8_t GetAutoRetransmitsStep() override  { return 1u; };

  bool    SetAutoRetransmitDelay(const uint8_t delay) override;
  uint8_t GetAutoRetransmitDelay() override     { return m_radioConfig.m_retransmitDelay; };
  uint8_t GetAutoRetransmitDelayMin() override  { return 0u; };
  uint8_t GetAutoRetransmitDelayMax() override  { return 1u; };
  uint8_t GetAutoRetransmitDelayStep() override { return 15u; };
  String AutoRetransmitDelayToString(const uint8_t delay) override;

protected:
  // Radio config, exchanged between master & slave
#pragma pack(push, 1)
  struct {
    uint8_t m_channel;
    uint8_t m_dataRate;
    uint8_t m_paLevel;
    uint8_t m_retransmits;
    uint8_t m_retransmitDelay;
  } m_radioConfig;
#pragma pack(pop)

//  uint8_t m_count;
  uint8_t m_baseId[5];
  uint8_t m_addrWidth;
};

RadioRF24::RadioRF24()
{
  SetConfigDefault();
}

void RadioRF24::SetConfigDefault()
{
  m_radioConfig.m_channel         = MY_RF24_CHANNEL;
  m_radioConfig.m_dataRate        = MY_RF24_DATARATE;
  m_radioConfig.m_paLevel         = MY_RF24_PA_LEVEL;
  m_radioConfig.m_retransmits     = RF24_SET_ARC;
  m_radioConfig.m_retransmitDelay = RF24_SET_ARD;

//  m_count           = 0;
  m_addrWidth       = MY_RF24_ADDR_WIDTH;
  (void)memcpy(&m_baseId, RF24_BASE_ID, m_addrWidth);
}

bool RadioRF24::ConfigActivate()
{
  // Activate config on radio
//  RF24_setAddressWidth(m_addrWidth);
  RF24_setChannel(m_radioConfig.m_channel);
  RF24_setRetries(m_radioConfig.m_retransmits, m_radioConfig.m_retransmitDelay);
  RF24_setRFSetup((uint8_t)(   ((m_radioConfig.m_dataRate & 0b10 ) << 4)
                             | ((m_radioConfig.m_dataRate & 0b01 ) << 3)
                             |  (m_radioConfig.m_paLevel << 1) 
                           ) + 1);
//    Serial.println(F("** Changing BaseId & Address width not yet supported **"));
  return true;
}

bool RadioRF24::GetBaseId(uint8_t * out, size_t& size)
{
  if ((not out) or (size < m_addrWidth))
    return false;

  size = m_addrWidth;

  (void)memcpy(out, m_baseId, size);
  return true;
}

bool RadioRF24::ConfigSerialize(uint8_t * out, size_t& size)
{
  if (not out)
    return false;

  if (size < sizeof(m_radioConfig))
    return false;
  size = sizeof(m_radioConfig);

  (void)memcpy(out, &m_radioConfig, sizeof(m_radioConfig));
  return true;
}

bool RadioRF24::ConfigDeserialize(const uint8_t * in, const size_t size)
{
  if (not in)
    return false;

  if (size != sizeof(m_radioConfig))
    return false;

  (void)memcpy(&m_radioConfig, in, sizeof(m_radioConfig));
  return true;
}

int16_t RadioRF24::GetRssi()
{
  return RF24_getSendingRSSI();
}

bool RadioRF24::SetChannel(const uint8_t channel)
{
  m_radioConfig.m_channel = channel;
  return true; 
}

bool RadioRF24::SetDataRate(const uint8_t rate)
{
  m_radioConfig.m_dataRate = rate;
  return true; 
}

bool RadioRF24::SetPowerLevel(const uint8_t level)
{
  m_radioConfig.m_paLevel = level;
  return true; 
}

bool RadioRF24::SetAutoRetransmits(const uint8_t retransmits)
{
  m_radioConfig.m_retransmits = retransmits;
  return true; 
}

bool RadioRF24::SetAutoRetransmitDelay(const uint8_t delay)
{
  m_radioConfig.m_retransmitDelay = delay;
  return true; 
}

String RadioRF24::ConfigToString()
{
  // Serial.println(F("-- Radio config --"));
  // Serial.print(F("Channel           ")); Serial.println(m_channel);
  // Serial.print(F("BaseId            ")); Serial.println(ToBaseIdStr(&m_baseId[0], m_addrWidth));
  // Serial.print(F("Datarate          ")); Serial.print(rateToStr(m_dataRate));
  // Serial.print(F(" (")); Serial.print(m_dataRate); Serial.println(F(")"));
  // Serial.print(F("PaLevel           ")); Serial.print(paLevelToStr(m_paLevel));
  // Serial.print(F(" (")); Serial.print(m_paLevel); Serial.println(F(")"));
  // Serial.print(F("Retransmits       ")); Serial.println(m_retransmits);
  // Serial.print(F("Retransmit delay  ")); Serial.print(uint16_t(m_retransmitDelay)*250);
  // Serial.print(F("[us] (")); Serial.print(m_retransmitDelay); Serial.println(F(")"));
  // Serial.println(F(""));
  return String();
}


String RadioRF24::BaseIdToString(const uint8_t * baseId, const size_t size)
{
  if ((not baseId) or (size == 0))
    return String();
  
  String s = "0x";
  for (size_t i = size-1; i != size_t(-1); --i)
  {
    s += ToHexStr(baseId[i]);
  }
  return s;
}

String RadioRF24::DataRateToString(const uint8_t rate)
{
  switch(rate)
  {
    case RF24_250KBPS: return F("250Kb/s");
    case RF24_1MBPS:   return F("1Mb/s");
    case RF24_2MBPS:   return F("2Mb/s");
  }
  return F("Unknown");
}

String RadioRF24::PowerLevelToString(const uint8_t level)
{
  switch(level)
  {
    case RF24_PA_MIN:  return F("Min");
    case RF24_PA_LOW:  return F("Low");
    case RF24_PA_HIGH: return F("High");
    case RF24_PA_MAX:  return F("Max");
  }
  return F("Unknown");
}

String RadioRF24::AutoRetransmitDelayToString(const uint8_t delay)
{
  return String(delay*250); // convert to [us]
}
