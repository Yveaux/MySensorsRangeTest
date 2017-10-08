#pragma once

#include "radio.h"

class RadioRFM69 : public Radio
{
public:
  RadioRFM69();
  void SetConfigDefault() override;
  bool ConfigActivate() override;
  String ConfigToString() override;
  bool ConfigSerialize(uint8_t * out, size_t& size) override;
  bool ConfigDeserialize(const uint8_t * in, const size_t size) override;

  bool SanityCheck() override { return RFM69_sanityCheck(); }
  String GetName() override { return F("RFM69"); };

  int16_t GetRssi() override;
  int16_t GetRssiMin() override  { return -115; };                // Datasheet: DR_RSSI
  int16_t GetRssiMax() override  { return 0; };                   // Datasheet: DR_RSSI
  int16_t GetRssiStep() override { return 1; };                   // Rssi stepsize. In real actually 0.5dB.

  bool     SetFrequencyHz(const uint32_t frequency) override;
  uint32_t GetFrequencyHz()     { return m_radioConfig.m_frequency; };
  uint32_t GetFrequencyMinHz() override;
  uint32_t GetFrequencyMaxHz() override;
  uint32_t GetFrequencyStepHz() { return RFM69_FSTEP; };          // 61 [Hz]

  // bool    SetDataRate(const uint8_t rate) override;
  // uint8_t GetDataRate() override      { return m_radioConfig.m_dataRate; };
  // uint8_t GetDataRateMin() override   { return 0u; };
  // uint8_t GetDataRateMax() override   { return 2u; };
  // uint8_t GetDataRateStep() override  { return 1u; };
  // String  DataRateToString(const uint8_t rate) override;

  bool   SetPowerLevel(const int8_t level) override;
  int8_t GetPowerLevel() override     { return m_radioConfig.m_powerLevel; };
  int8_t GetPowerLevelMin() override  { return RFM69_MIN_POWER_LEVEL_DBM; };
  int8_t GetPowerLevelMax() override  { return RFM69_MAX_POWER_LEVEL_DBM; };
  int8_t GetPowerLevelStep() override { return 1u; };
  String PowerLevelToString(const int8_t level) override;

protected:
  // Radio config, exchanged between master & slave
#pragma pack(push, 1)
  struct {
    uint32_t m_frequency;
//    uint8_t m_dataRate;
    uint8_t m_powerLevel;
//    uint8_t m_retransmits;
//    uint8_t m_retransmitDelay;
  } m_radioConfig;
#pragma pack(pop)

//  uint8_t m_count;
  uint8_t m_baseId[5];
  uint8_t m_addrWidth;
};

RadioRFM69::RadioRFM69()
{
  SetConfigDefault();
}

void RadioRFM69::SetConfigDefault()
{

  m_radioConfig.m_frequency  = MY_RFM69_FREQUENCY;
  m_radioConfig.m_powerLevel = MY_RFM69_TX_POWER_DBM;

// MY_RFM69_TX_POWER_DBM
// MY_RFM69_ATC_MODE_DISABLED
// MY_RFM69_NETWORKID
  
//   m_radioConfig.m_channel         = MY_RF24_CHANNEL;
//   m_radioConfig.m_dataRate        = MY_RF24_DATARATE;
//   m_radioConfig.m_paLevel         = MY_RF24_PA_LEVEL;
//   m_radioConfig.m_retransmits     = RF24_SET_ARC;
//   m_radioConfig.m_retransmitDelay = RF24_SET_ARD;

// //  m_count           = 0;
//   m_addrWidth       = MY_RF24_ADDR_WIDTH;
//   (void)memcpy(&m_baseId, RF24_BASE_ID, m_addrWidth);
}

bool RadioRFM69::ConfigActivate()
{
  bool ok = true;
  // Activate config on radio
  RFM69_setFrequency(m_radioConfig.m_frequency);
  ok &= RFM69_setTxPowerLevel(m_radioConfig.m_powerLevel);

//  RF24_setAddressWidth(m_addrWidth);
//   RF24_setChannel(m_radioConfig.m_channel);
//   RF24_setRetries(m_radioConfig.m_retransmits, m_radioConfig.m_retransmitDelay);
//   RF24_setRFSetup((uint8_t)(   ((m_radioConfig.m_dataRate & 0b10 ) << 4)
//                              | ((m_radioConfig.m_dataRate & 0b01 ) << 3)
//                              |  (m_radioConfig.m_paLevel << 1) 
//                            ) + 1);
// //    Serial.println(F("** Changing BaseId & Address width not yet supported **"));
  return ok;
}

bool RadioRFM69::ConfigSerialize(uint8_t * out, size_t& size)
{
  if (not out)
    return false;

  if (size < sizeof(m_radioConfig))
    return false;
  size = sizeof(m_radioConfig);

  (void)memcpy(out, &m_radioConfig, sizeof(m_radioConfig));
  return true;
}

bool RadioRFM69::ConfigDeserialize(const uint8_t * in, const size_t size)
{
  if (not in)
    return false;

  if (size != sizeof(m_radioConfig))
    return false;

  (void)memcpy(&m_radioConfig, in, sizeof(m_radioConfig));
  return true;
}

int16_t RadioRFM69::GetRssi()
{
  return RFM69_getReceivingRSSI();
}

bool RadioRFM69::SetFrequencyHz(const uint32_t frequency)
{
  if ((frequency < GetFrequencyMinHz()) or (frequency > GetFrequencyMaxHz()))
    return false;

  m_radioConfig.m_frequency = frequency; 
  return true;
}

uint32_t RadioRFM69::GetFrequencyMinHz()
{
  if ((MY_RFM69_FREQUENCY >= TO_MHZ(290)) and (MY_RFM69_FREQUENCY <= TO_MHZ(340)))
  {
    return TO_MHZ(290);
  }
  if ((MY_RFM69_FREQUENCY >= TO_MHZ(431)) and (MY_RFM69_FREQUENCY <= TO_MHZ(510)))
  {
    return TO_MHZ(431);
  }
  if ((MY_RFM69_FREQUENCY >= TO_MHZ(862)) and (MY_RFM69_FREQUENCY <= TO_MHZ(1020)))
  {
    return TO_MHZ(862);
  }
  return 0ul;
}

uint32_t RadioRFM69::GetFrequencyMaxHz()
{
  if ((MY_RFM69_FREQUENCY >= TO_MHZ(290)) and (MY_RFM69_FREQUENCY <= TO_MHZ(340)))
  {
    return TO_MHZ(340);
  }
  if ((MY_RFM69_FREQUENCY >= TO_MHZ(431)) and (MY_RFM69_FREQUENCY <= TO_MHZ(510)))
  {
    return TO_MHZ(510);
  }
  if ((MY_RFM69_FREQUENCY >= TO_MHZ(862)) and (MY_RFM69_FREQUENCY <= TO_MHZ(1020)))
  {
    return TO_MHZ(1020);
  }
  return 0ul;
}

// bool RadioRFM69::SetDataRate(const uint8_t rate)
// {
//   m_radioConfig.m_dataRate = rate;
//   return true; 
// }

bool RadioRFM69::SetPowerLevel(const int8_t level)
{
  if ((level < RFM69_MIN_POWER_LEVEL_DBM) or (level > RFM69_MAX_POWER_LEVEL_DBM))
    return false;

  m_radioConfig.m_powerLevel = level;
  return true; 
}

String RadioRFM69::ConfigToString()
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

// String RadioRFM69::DataRateToString(const uint8_t rate)
// {
//   switch(rate)
//   {
//     // case RF24_250KBPS: return F("250Kb/s");
//     // case RF24_1MBPS:   return F("1Mb/s");
//     // case RF24_2MBPS:   return F("2Mb/s");
//   }
//   return F("Unknown");
// }

String RadioRFM69::PowerLevelToString(const int8_t level)
{
  return String(level) + F("dBm");
}

