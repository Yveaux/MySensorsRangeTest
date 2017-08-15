#pragma once

// Data exchanged as configuration message 
struct t_configData
{
  t_configData()
  {
    setDefault();
  }
  
  void setDefault()
  {
    m_count           = 0;
    m_channel         = MY_RF24_CHANNEL;
    m_addrWidth       = MY_RF24_ADDR_WIDTH;
    (void)memcpy(&m_baseId, RF24_BASE_ID, m_addrWidth);
    m_dataRate        = MY_RF24_DATARATE;
    m_paLevel         = MY_RF24_PA_LEVEL;
  	m_retransmits     = RF24_SET_ARC;
  	m_retransmitDelay = RF24_SET_ARD;
  }

  void activate()
  {
  	// Activate config on radio
//	RF24_setAddressWidth(m_addrWidth);
  	RF24_setChannel(m_channel);
  	RF24_setRetries(m_retransmits, m_retransmitDelay);
	  RF24_setRFSetup((uint8_t)( ((m_dataRate & 0b10 ) << 4) | ((m_dataRate & 0b01 ) << 3) | (m_paLevel << 1) ) + 1);
//	  Serial.println(F("** Changing BaseId & Address width not yet supported **"));
  }

  void log()
  {
    Serial.println(F("-- Radio config --"));
    Serial.print(F("Channel           ")); Serial.println(m_channel);
    Serial.print(F("BaseId            ")); Serial.println(toBaseIdStr(&m_baseId[0], m_addrWidth));
    Serial.print(F("Datarate          ")); Serial.print(rateToStr(m_dataRate));
    Serial.print(F(" (")); Serial.print(m_dataRate); Serial.println(F(")"));
    Serial.print(F("PaLevel           ")); Serial.print(paLevelToStr(m_paLevel));
    Serial.print(F(" (")); Serial.print(m_paLevel); Serial.println(F(")"));
    Serial.print(F("Retransmits       ")); Serial.println(m_retransmits);
    Serial.print(F("Retransmit delay  ")); Serial.print(uint16_t(m_retransmitDelay)*250);
    Serial.print(F("[us] (")); Serial.print(m_retransmitDelay); Serial.println(F(")"));
    Serial.println(F(""));
  }

  static char toHexChar(uint8_t v)
  {
    v &= 0x0F;
    if (v < 10)
      return '0'+v;
    return 'A'-10+v;
  }

  template <typename T> static String toHexStr(T val, const size_t numDigits = 0)
  {
    String s;
    const size_t digits = numDigits > 0 ? numDigits : sizeof(val)*2;
    for (size_t i = 0; i < digits; ++i)
    {
      const uint8_t hex = val >> (8*sizeof(val)-4);
      s += toHexChar(hex);
      val <<= 4;
    }
    return s;
  }

  static String toBaseIdStr( const uint8_t* baseId, const size_t len)
  {
    String s = "0x";
    for (size_t i = len-1; i != size_t(-1); --i)
    {
      s += toHexStr(baseId[i]);
    }
    return s;
  }

  static const __FlashStringHelper* rateToStr(const uint8_t rate)
  {
    switch(rate)
    {
      case RF24_250KBPS: return F("250Kb/s");
      case RF24_1MBPS:   return F("1Mb/s");
      case RF24_2MBPS:   return F("2Mb/s");
    }
    return F("Unknown");
  }

  static const __FlashStringHelper* paLevelToStr(const uint8_t level)
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
  uint8_t m_count;
  uint8_t m_channel;
  uint8_t m_baseId[5];
  uint8_t m_addrWidth;
  uint8_t m_dataRate;
  uint8_t m_paLevel;
  uint8_t m_retransmits;
  uint8_t m_retransmitDelay;
}; 