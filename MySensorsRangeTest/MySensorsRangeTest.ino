#define MASTER
//#define SLAVE


#if !defined(MASTER) && !defined(SLAVE)
#error Define either MASTER or SLAVE to set role of this node
#endif
#if defined(MASTER) && defined(SLAVE)
#error Define either MASTER or SLAVE to set role of this node
#endif

#define SKETCH_VERSION_STR     "1.1"

#define NODE_ID_MASTER         (100)
#define NODE_ID_SLAVE          (101)
#define SCREEN_UPDATE_COUNT    (10)     // Update screen each xx Ping-pong sequences
#define MAX_NUM_CONFIG_RETRIES (10)     // Number of transmission attempts when changing config

#ifdef MASTER
#define MY_NODE_ID             NODE_ID_MASTER
#define MY_PARENT_NODE_ID      NODE_ID_SLAVE
#define ROLE_STR               "Master"       
#define PING_INTERVAL_MS       (100)    // Interval for the master to emit a Ping message
#endif

#ifdef SLAVE
#define MY_NODE_ID             NODE_ID_SLAVE
#define MY_PARENT_NODE_ID      NODE_ID_MASTER
#define ROLE_STR               "Slave"       
#endif

#define CHILD_ID_PINGPONG      (1)
#define CHILD_ID_CONFIG        (2)

#define MY_SPLASH_SCREEN_DISABLED
#define MY_PARENT_NODE_IS_STATIC
#define MY_TRANSPORT_UPLINK_CHECK_DISABLED
// #undef MY_REGISTRATION_FEATURE in MyConfig.h to speed up startup by 2 seconds
//#define MY_DEBUG

#define MY_RADIO_NRF24
//#define MY_RF24_CHANNEL        (112)    
//#define MY_RF24_BASE_RADIO_ID  0x00,0xFC,0xE1,0xA8,0xA8
//#define MY_RF24_DATARATE       (RF24_1MBPS)
#define MY_RF24_PA_LEVEL       (RF24_PA_MIN)    // Start at low level to prevent issues with nodes close to eachother
//#define MY_RF24_PA_LEVEL       (RF24_PA_LOW)
//#define MY_RF24_PA_LEVEL       (RF24_PA_HIGH)
//#define MY_RF24_PA_LEVEL       (RF24_PA_MAX)


#ifndef MY_RADIO_NRF24
#error Currently only nRF24 radio is supported!
#endif


#ifdef MY_RADIO_NRF24
#define RSSI_MIN                   (-149.0)
#define RSSI_MAX                   (-29.0)
#define RSSI_STEP                  (8.0)
#define RADIO_CHANNEL_MIN          (0)
#define RADIO_CHANNEL_MAX          (125)
#define RADIO_DATARATE_MIN         (0)
#define RADIO_DATARATE_MAX         (2)
#define RADIO_PALEVEL_MIN          (0)
#define RADIO_PALEVEL_MAX          (3)
#define RADIO_RETRANSMITS_MIN      (0)
#define RADIO_RETRANSMITS_MAX      (15)
#define RADIO_RETRANSMITSDELAY_MIN (0)
#define RADIO_RETRANSMITSDELAY_MAX (15)
#endif

#define PAYLOAD_LENGTH_MIN         (1)
#define PAYLOAD_LENGTH_MAX         (MAX_PAYLOAD)
#define HISTOGRAM_DECAY_RATE_MIN   (0)
#define HISTOGRAM_DECAY_RATE_MAX   (10)

//#ifdef MASTER
//#define MY_INDICATION_HANDLER
//#endif

#include <MySensors.h>
#include <AnsiStream.h>
#include "config.h"
#include "histogram.h"

using namespace ANSI;


// Serial command buffer
struct t_command
{
  t_command()
  {
    clear();
  }
  void clear()
  {
    m_text = F("");
    m_complete = false;
  }
  String m_text;
  bool   m_complete;
};
static t_command cmd;

// Actual data exchanged as ping-pong message 
#pragma pack(push, 1)
union t_pingPongData {
  uint8_t m_count;
  uint8_t m_dummy[MAX_PAYLOAD];
};
#pragma pack(pop)

// Internal housekeeping data
struct t_txData
{
  t_txData()
  {
    clear();
  }

  void clear()
  {
    m_txTsUs       = 0ul;
    m_turnaroundUs = 0ul;
    m_txOk         = false;
    m_rxOk         = false;
    m_rssi         = 0;
  }
  unsigned long  m_txTsUs;        // Ping-message transmit timestamp
  unsigned long  m_turnaroundUs;  // Ping-pong turnaround time. 0 means no pong received (yet)
  t_pingPongData m_data;          // Actual ping-pong data exchanged
  bool           m_txOk;          // Result of sending Ping or pong message
  bool           m_rxOk;          // Result of retrieving pong message
  int16_t        m_rssi;          // RSSI of sending Ping or pong message
};
static t_txData  g_txData;
static MyMessage g_txMsgPingPong;
static MyMessage g_txMsgConfig;
static t_configData& g_config = *(static_cast<t_configData*>(g_txMsgConfig.getCustom()));
static uint8_t g_histogramDecayRate = 0;
static size_t  g_payloadLen = PAYLOAD_LENGTH_MIN;      // Length of ping-pong payload

// First bucket in histogram is used to store transmitssion failures.
// This looks most intuitive as the results will be displayed next to the lowest RSSI.
static histogram<int16_t> g_histRssi((RSSI_MAX-RSSI_MIN)/RSSI_STEP+1+1, RSSI_MIN-RSSI_STEP/2-RSSI_STEP, RSSI_MAX+RSSI_STEP/2);
// Histogram to track succes/fail ratio for transmissions.
static histogram<uint8_t> g_histTxOk(2, 0, 2);
// Histogram to track succes/fail ratio for reception (complete ping-pong cycle).
static histogram<uint8_t> g_histTxRxOk(2, 0, 2);


void before()
{
  // Prepare ping-pong transmit message to other node.
  g_txMsgPingPong.setDestination(MY_PARENT_NODE_ID);
  g_txMsgPingPong.setSensor(CHILD_ID_PINGPONG);
  g_txMsgPingPong.setType(V_VAR1);  

  // Prepare config message.
  g_txMsgConfig.setDestination(MY_PARENT_NODE_ID);
  g_txMsgConfig.setSensor(CHILD_ID_CONFIG);
  g_txMsgConfig.setType(V_VAR1);  
  g_config.setDefault();
  g_txMsgConfig.set(&g_config, sizeof(g_config));  

  screenUpdate(true);
}

void setup()
{
}

String rightAlignStr(const String str, const size_t width, const char fill = ' ')
{
  if (str.length() >= width)
  {
    return str.substring(str.length() - width);
  }

  String s;
  size_t i = width - str.length();
  do
  {
    s += fill;
  } while (--i != 0);
  s += str;
  return s;
}


#define ROW_HEADER                (1)
#define ROW_CONFIG                (ROW_HEADER+2)
#define COL_WIDTH_CONFIG          (15)
#define ROW_HIST_RSSI             (ROW_CONFIG+5)
#define WIDTH_HIST_RSSI_BUCKETS   (COL_WIDTH_CONFIG)
#define WIDTH_HIST_RSSI           (3*COL_WIDTH_CONFIG)
#define WIDTH_HIST_RSSI_COUNTS    (10)
#define ROW_HIST_TXOK             (ROW_HIST_RSSI+18)
#define WIDTH_HIST_TXOK_BUCKETS   (WIDTH_HIST_RSSI_BUCKETS)
#define WIDTH_HIST_TXOK           (WIDTH_HIST_RSSI)
#define WIDTH_HIST_TXOK_COUNTS    (WIDTH_HIST_RSSI_COUNTS)
#define ROW_HIST_RXOK             (ROW_HIST_RSSI+19)
#define WIDTH_HIST_RXOK_BUCKETS   (WIDTH_HIST_RSSI_BUCKETS)
#define WIDTH_HIST_RXOK           (WIDTH_HIST_RSSI)
#define WIDTH_HIST_RXOK_COUNTS    (WIDTH_HIST_RSSI_COUNTS)
#define ROW_MENU                  (ROW_HIST_RXOK+2)
#define COL_WIDTH_MENU            (30)

void screenUpdate(const bool rebuild)
{
  if (rebuild)
  {
    Serial << home() << eraseScreen() << defaultBackground() << defaultForeground();
    // Header
    Serial << setForegroundColor(YELLOW);
    Serial << xy(1,ROW_HEADER) << boldOn() << F("MySensors range test ") << F(SKETCH_VERSION_STR) << F(" -- ") << F(ROLE_STR) << boldOff();
    // Config
    Serial << setForegroundColor(BLUE);
    Serial << xy(1,ROW_CONFIG)                      << F("Channel");
    Serial << xy(1+COL_WIDTH_CONFIG,ROW_CONFIG)     << boldOn() << rightAlignStr(String(g_config.m_channel), COL_WIDTH_CONFIG-1) << boldOff();
    Serial << xy(1+2*COL_WIDTH_CONFIG,ROW_CONFIG)   << F("BaseId");
    Serial << xy(1+3*COL_WIDTH_CONFIG,ROW_CONFIG)   << boldOn() << rightAlignStr(g_config.toBaseIdStr(&g_config.m_baseId[0], g_config.m_addrWidth), COL_WIDTH_CONFIG-1) << boldOff();
    Serial << xy(1,ROW_CONFIG+1)                    << F("Datarate");
    Serial << xy(1+COL_WIDTH_CONFIG,ROW_CONFIG+1)   << boldOn() << rightAlignStr(String(g_config.rateToStr(g_config.m_dataRate)), COL_WIDTH_CONFIG-1) << boldOff();
    Serial << xy(1+2*COL_WIDTH_CONFIG,ROW_CONFIG+1) << F("Pa-Level");
    Serial << xy(1+3*COL_WIDTH_CONFIG,ROW_CONFIG+1) << boldOn() << rightAlignStr(String(g_config.paLevelToStr(g_config.m_paLevel)), COL_WIDTH_CONFIG-1) << boldOff();
    Serial << xy(1,ROW_CONFIG+2)                    << F("Retransmits");
    Serial << xy(1+COL_WIDTH_CONFIG,ROW_CONFIG+2)   << boldOn() << rightAlignStr(String(g_config.m_retransmits), COL_WIDTH_CONFIG-1) << boldOff();
    Serial << xy(1+2*COL_WIDTH_CONFIG,ROW_CONFIG+2) << F("RetrDelay");
    String strDelay = String( uint16_t(g_config.m_retransmitDelay + 1)*250 );
    strDelay += F("us");
    Serial << xy(1+3*COL_WIDTH_CONFIG,ROW_CONFIG+2) << boldOn() << rightAlignStr(strDelay, COL_WIDTH_CONFIG-1) << boldOff() ;
    Serial << xy(1,ROW_CONFIG+3)                    << F("Payload size");
    Serial << xy(1+COL_WIDTH_CONFIG,ROW_CONFIG+3)   << boldOn() << rightAlignStr(String(g_payloadLen), COL_WIDTH_CONFIG-1) << boldOff();
  }

  // RSSI histogram
  {
    uint8_t x = WIDTH_HIST_RSSI_BUCKETS+1;
    uint8_t y = ROW_HIST_RSSI;
    uint8_t w = WIDTH_HIST_RSSI;
    for (size_t b = 0; b < g_histRssi.size(); ++b)
    {
      double relcount = g_histRssi.relcount(b);
      int xc = int(double(w) * relcount + 0.5);
      if (rebuild)
      {
        int16_t db = g_histRssi.lowerbound(b) + (g_histRssi.upperbound(b) - g_histRssi.lowerbound(b)) / 2;

        Serial << defaultBackground() << setForegroundColor(YELLOW);
        Serial << xy(1,y);
        if (b == 0)
        {
          // Special case: Failed transmissions are stored in bucket 0
          Serial << boldOn() << F("Failed") << boldOff();
        }
        else
        {
          Serial << db << F(" dB");
        }
      }
      Serial << setBackgroundColor(YELLOW);
      Serial << fill(x,  y, x+xc-1, y, ' ');
      Serial << defaultBackground() << setForegroundColor(YELLOW);
      Serial << fill(x+xc, y, x+w-1,  y, '-');

      Serial << xy(WIDTH_HIST_RSSI_BUCKETS+WIDTH_HIST_RSSI+1+1,y) << boldOn() << rightAlignStr(String(g_histRssi.count(b)), WIDTH_HIST_RSSI_COUNTS) << boldOff();
      ++y;
    }
    // SLowly decrease bucket values, to limit history time
    g_histRssi.decay(g_histogramDecayRate);
  }

  // Tx Ok/Fail histogram
  {
    uint8_t x = WIDTH_HIST_TXOK_BUCKETS+1;
    uint8_t y = ROW_HIST_TXOK;
    uint8_t w = WIDTH_HIST_TXOK;  
    if (rebuild)
    {
      Serial << defaultBackground() << setForegroundColor(YELLOW);
      Serial << xy(1,y) << F("Tx ") << setForegroundColor(GREEN) << F("Ok") << setForegroundColor(YELLOW) << '/' <<  setForegroundColor(RED) << F("Fail");
      Serial << defaultForeground();
    }
    double relOk = g_histTxOk.relcount(1);
    int xc = int(double(w) * relOk + 0.5);
    Serial << setBackgroundColor(GREEN);
    Serial << fill(x,  y, x+xc-1, y, ' ');
    Serial << setBackgroundColor(RED);
    Serial << fill(x+xc, y, x+w-1,  y, ' ');
    Serial << defaultBackground();
    uint8_t perc = 100.0*double(g_histTxOk.relcount(1)) + 0.5;
    String ratio = String(perc);
    ratio += ' ';
    ratio += '%';
    Serial << xy(WIDTH_HIST_TXOK_BUCKETS+WIDTH_HIST_TXOK+1+1,y) << boldOn() << setForegroundColor(GREEN) << rightAlignStr(ratio, WIDTH_HIST_TXOK_COUNTS) << boldOff();
    Serial << defaultForeground();
    // SLowly decrease bucket values, to limit history time
    g_histTxOk.decay(g_histogramDecayRate);
  }

#ifdef MASTER
  // Rx Ok/Fail histogram
  {
    uint8_t x = WIDTH_HIST_RXOK_BUCKETS+1;
    uint8_t y = ROW_HIST_RXOK;
    uint8_t w = WIDTH_HIST_RXOK;  
    if (rebuild)
    {
      Serial << defaultBackground() << setForegroundColor(YELLOW);
      Serial << xy(1,y) << F("TxRx ") << setForegroundColor(GREEN) << F("Ok") << setForegroundColor(YELLOW) << '/' <<  setForegroundColor(RED) << F("Fail");
      Serial << defaultForeground();
    }
    double relOk = g_histTxRxOk.relcount(1);
    int xc = int(double(w) * relOk + 0.5);
    Serial << setBackgroundColor(GREEN);
    Serial << fill(x,  y, x+xc-1, y, ' ');
    Serial << setBackgroundColor(RED);
    Serial << fill(x+xc, y, x+w-1,  y, ' ');
    Serial << defaultBackground();
    uint8_t perc = 100.0*double(g_histTxRxOk.relcount(1)) + 0.5;
    String ratio = String(perc);
    ratio += ' ';
    ratio += '%';
    Serial << xy(WIDTH_HIST_RXOK_BUCKETS+WIDTH_HIST_RXOK+1+1,y) << boldOn() << setForegroundColor(GREEN) << rightAlignStr(ratio, WIDTH_HIST_RXOK_COUNTS) << boldOff();
    Serial << defaultForeground();
    // SLowly decrease bucket values, to limit history time
    g_histTxRxOk.decay(g_histogramDecayRate);
  }
#endif

  // Menu
  if (rebuild)
  {
    Serial << setForegroundColor(BLUE);
#ifdef MASTER
    Serial << xy(1,ROW_MENU)                  << F("cn - Set channel [0..125]");
    Serial << xy(1+COL_WIDTH_MENU,ROW_MENU)   << F("rn - Set datarate [0..2]");
    Serial << xy(1,ROW_MENU+1)                << F("pn - Set PaLevel [0..3]");
    Serial << xy(1+COL_WIDTH_MENU,ROW_MENU+1) << F("tn - Set retransmits [0..15]");
    Serial << xy(1,ROW_MENU+2)                << F("yn - Set retr. delay [0..15]");
    Serial << xy(1,ROW_MENU+3)                << F("ln - Set payload len [1..") << MAX_PAYLOAD << ']';
    Serial << xy(1+COL_WIDTH_MENU,ROW_MENU+3) << F("x - Reset settings");
#endif
    Serial << xy(1,ROW_MENU+4)                << F("0 - Reset statistics");
    Serial << xy(1+COL_WIDTH_MENU,ROW_MENU+4) << F("dn - Set hist. decay [0..5]");
  }
  Serial << home();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
bool processCommand(t_command& cmd)
{
  bool configChanged = false;
  if (cmd.m_complete)
  {
    uint8_t value = uint8_t(cmd.m_text.substring(1, cmd.m_text.length()).toInt());
    if (cmd.m_text.startsWith(F("0")))
    {
      g_histRssi.clear();
      g_histTxOk.clear();
      g_histTxRxOk.clear();
    }
    else if (cmd.m_text.startsWith(F("d")))
    {
      if ((value >= HISTOGRAM_DECAY_RATE_MIN) && (value <= HISTOGRAM_DECAY_RATE_MAX))
      {
        g_histogramDecayRate = value;
      }
    }
#ifdef MASTER
    else if (cmd.m_text.startsWith(F("x")))
    {
      g_config.setDefault();
      configChanged = true;
    }
    else if (cmd.m_text.startsWith(F("c")))
    {
      if ((value >= RADIO_CHANNEL_MIN) && (value <= RADIO_CHANNEL_MAX))
      {
        g_config.m_channel = value;
        configChanged = true;
      }
    }
    else if (cmd.m_text.startsWith(F("r")))
    {
      if ((value >= RADIO_DATARATE_MIN) && (value <= RADIO_DATARATE_MAX))
      {
        g_config.m_dataRate = value;
        configChanged = true;
      }
    }
    else if (cmd.m_text.startsWith(F("p")))
    {
      if ((value >= RADIO_PALEVEL_MIN) && (value <= RADIO_PALEVEL_MAX))
      {
        g_config.m_paLevel = value;
        configChanged = true;
      }
    }
    else if (cmd.m_text.startsWith(F("t")))
    {
      if ((value >= RADIO_RETRANSMITS_MIN) && (value <= RADIO_RETRANSMITS_MAX))
      {
        g_config.m_retransmits = value;
        configChanged = true;
      }
    }
    else if (cmd.m_text.startsWith(F("y")))
    {
      if ((value >= RADIO_RETRANSMITSDELAY_MIN) && (value <= RADIO_RETRANSMITSDELAY_MAX))
      {
        g_config.m_retransmitDelay = value;
        configChanged = true;
      }
    }
    else if (cmd.m_text.startsWith(F("l")))
    {
      if ((value >= PAYLOAD_LENGTH_MIN) && (value <= PAYLOAD_LENGTH_MAX))
      {
        g_payloadLen = value;
      }
    }
#endif
    cmd.clear();
    screenUpdate(true);
  }
  return configChanged;
}
#pragma GCC diagnostic pop

void serialEvent()
{
  while(Serial.available())
  {
    char c = static_cast<char>(Serial.read());
    if (c == '\n') cmd.m_complete = true;
    else           cmd.m_text += c;
  }
}

void logResults(const t_txData& data)
{
  g_histTxOk.store(data.m_txOk ? 1 : 0);
  g_histTxRxOk.store(data.m_rxOk ? 1 : 0);
  // If Tx failed: Store in RSSI for first bucket
  g_histRssi.store(data.m_txOk ? data.m_rssi : (RSSI_MIN - RSSI_STEP));
}

// Slave: receive ping, send pong
void receivedPing(const unsigned long rxTsUs, const t_pingPongData& data)
{
  (void)rxTsUs;
  // Slave received Ping; Send pong reply with the same data
  g_txMsgPingPong.set(const_cast<void*>(static_cast<const void*>(&data)), g_payloadLen);

  g_txData.clear();
  g_txData.m_txTsUs = micros();
  g_txData.m_txOk   = send(g_txMsgPingPong);
  g_txData.m_rssi   = transportGetSendingRSSI();
  logResults(g_txData);
}

// Master: Send ping
void sendPing(t_txData& data)
{
  static bool first = true;
  static t_pingPongData pingPongData;
  pingPongData.m_count = first ? 0u : (pingPongData.m_count + 1);
  g_txMsgPingPong.set(&pingPongData, g_payloadLen);

  // Keep track of data sent and timestamp, then send the ping message
  data.clear();
  data.m_data   = pingPongData;
  data.m_txTsUs = micros();
  data.m_txOk   = send(g_txMsgPingPong);
  data.m_rssi   = transportGetSendingRSSI();
  first = false;
}

// Master: Received pong
void receivedPong(const unsigned long rxTsUs, const t_pingPongData& data)
{
  // Master received Pong; Store result
  if (data.m_count == g_txData.m_data.m_count)
  {
    // This is a pong-reply to the last message sent 
    g_txData.m_turnaroundUs = rxTsUs - g_txData.m_txTsUs;
    g_txData.m_rxOk = true;
  }
}

bool sendConfig()
{
  bool sendOk = false;
  for (size_t i = 0; !sendOk && (i < MAX_NUM_CONFIG_RETRIES); ++i)
  {
    // Serial.print(F("Send config. Attempt "));
    // Serial.print(i+1);
    // Serial.print('/');
    // Serial.println(MAX_NUM_CONFIG_RETRIES); 
    sendOk = send(g_txMsgConfig);
  }
  return sendOk;
}

void receive(const MyMessage &rxMsg)
{
  unsigned long rxTsUs = micros();
  if ((CHILD_ID_PINGPONG == rxMsg.sensor) && (V_VAR1 == rxMsg.type))
  {
    const t_pingPongData& data = *(static_cast<t_pingPongData*>(rxMsg.getCustom()));
#ifdef MASTER
    receivedPong(rxTsUs, data);
#else
    size_t len = mGetLength(rxMsg);
    bool screenRefresh = len != g_payloadLen;
    g_payloadLen = len;
    receivedPing(rxTsUs, data);
    if (screenRefresh)
    {
      screenUpdate(true);
    }
#endif

    static uint8_t count = 0;
    if (count % SCREEN_UPDATE_COUNT == 0)
    {
      screenUpdate(false);
    }
    ++count;
  }
  else if ((CHILD_ID_CONFIG == rxMsg.sensor) && (V_VAR1 == rxMsg.type))
  {
    t_configData& newConfig = *(static_cast<t_configData*>(rxMsg.getCustom()));
    g_config = newConfig;
    screenUpdate(true);
    wait(10);
    g_config.activate();
  }
}

void loop()
{
#ifdef MASTER
  static unsigned long prevMs = millis();
  const unsigned long nowMs = millis();
  static uint8_t count = 0;
  if ((nowMs - prevMs) >= PING_INTERVAL_MS)
  {
    if (count % SCREEN_UPDATE_COUNT == 0)
    {
      screenUpdate(false);
    }
    else
    {
      static bool first = true;
      if (first)
      {
        prevMs = nowMs;
        first = false;
      }
      else
      {
        prevMs = prevMs + PING_INTERVAL_MS;
        // Ping-pong has been performed. Log the results.
        logResults(g_txData);
      }
      // Initiate next ping-pong
      sendPing(g_txData);
    }
    ++count;
  }

  if (processCommand(cmd))
  {
    // Config changed. Send new config to parent and activate it.
    bool configSentOk = sendConfig();
    if (configSentOk)
    {
//      Serial.println(F("-- Activating new radio configuration on Master --"));
//      g_config.log();
//      wait(10);
      g_config.activate();
    }
    else
    {
      Serial.println(F("-- Failed to activate new configuration on both nodes! --"));
      Serial.println(F("Configuration of slave node is unknown. Better restart both nodes!"));      
    }
  }
#endif

#ifdef SLAVE
  // Slave can only clear/change statistics, not radio settings.
  (void)processCommand(cmd);
#endif
}
