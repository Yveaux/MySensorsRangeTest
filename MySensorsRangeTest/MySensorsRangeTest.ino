//#define MASTER
//#define SLAVE
#if !defined(MASTER) && !defined(SLAVE)
#define MASTER
#endif

//#define MY_RADIO_NRF24
#define MY_RADIO_RFM69

// Default Pinout (use 3.3V Arduino for RFM69!)
//         nRF24    RFM69H(W)  nrf2rfm69  Arduino
// GND     1        GND        1          GND
// 3.3V    2        3.3V       2          3.3V
// CE      3        -          -          9
// CSN     4        NSS        4          10
// SCK     5        SCK        5          13
// MOSI    6        MOSI       6          11
// MISO    7        MISO       7          12
// IRQ     8        DIO0       8          2
// RESET   -        RESET      3          9

#if !defined(MASTER) && !defined(SLAVE)
#error Define either MASTER or SLAVE to set role of this node
#endif
#if defined(MASTER) && defined(SLAVE)
#error Define either MASTER or SLAVE to set role of this node
#endif

#define SKETCH_VERSION_STR     "1.2"

#define NODE_ID_MASTER            (100)
#define NODE_ID_SLAVE             (101)
#define SCREEN_UPDATE_INTERVAL_MS (1000)     // Update interval for screen updates, in [ms]
#define MAX_NUM_CONFIG_RETRIES    (10)       // Number of transmission attempts when changing config
#define MIN_PING_INTERVAL_MS      (10)
#define MAX_PING_INTERVAL_MS      (5000)

#ifdef MASTER
#define MY_NODE_ID             NODE_ID_MASTER
#define MY_PARENT_NODE_ID      NODE_ID_SLAVE
#define ROLE_STR               "Master"       
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

#if defined(MY_RADIO_NRF24)
#define MY_RF24_PA_LEVEL       (RF24_PA_MIN)    // Start at low level to prevent issues with nodes close to eachother
#elif defined(MY_RADIO_RFM69)
#define MY_RFM69_NEW_DRIVER
//#define MY_DEBUG_VERBOSE_RFM69
#define MY_RFM69_ATC_MODE_DISABLED
#define MY_RFM69_TX_POWER_DBM       (-2)
//void RFM69_ATCmode(const bool onOff, const int16_t targetRSSI)
// REF: https://www.mysensors.org/download/sensor_api_20
#define MY_IS_RFM69HW
#define MY_RFM69_RST_PIN (9)
// #define MY_RFM69_IRQ_PIN       Default 2
// #define MY_RFM69_IRQ_NUM       DigitalPinToInterrupt(MY_RFM69_IRQ_PIN)
// #define DEFAULT_RFM69_CS_PIN   Default (SS)   
// #define MY_RFM69_FREQUENCY     Default RFM69_868MHZ
// #define MY_RFM69_NETWORKID     Default 100
// Required? MY_RFM69_ENABLE_LISTENMODE
#else
#error A supported radio must be selected!
#endif

#define PAYLOAD_LENGTH_MIN         (1)
#define PAYLOAD_LENGTH_MAX         (MAX_PAYLOAD)
#define HISTOGRAM_DECAY_RATE_MIN   (0)
#define HISTOGRAM_DECAY_RATE_MAX   (10)
#define HISTOGRAM_RSSI_MAX_BUCKETS (25)
#define PING_INTERVAL_DEFAULT_MS   (500)

//#ifdef MASTER
//#define MY_INDICATION_HANDLER
//#endif

#include <MySensors.h>
#include <AnsiStream.h>
#include "histogram.h"

#if defined(MY_RADIO_NRF24)
#include "radiorf24.h"
static RadioRF24 g_radio;
#elif defined(MY_RADIO_RFM69)
#include "radiorfm69.h"
static RadioRFM69 g_radio;
#endif

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
static t_command g_serialCommand;

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
//static t_configData& g_config = *(static_cast<t_configData*>(g_txMsgConfig.getCustom()));
static uint8_t g_histogramDecayRate = 0;
static size_t  g_payloadLen = PAYLOAD_LENGTH_MIN;      // Length of ping-pong payload

// First bucket in histogram is used to store transmitssion failures.
// This looks most intuitive as the results will be displayed next to the lowest RSSI.
static histogram<int16_t> g_histRssi;
static int16_t g_histFailedRssiValue;   // Value to write to histogram for failed transmits
// Histogram to track succes/fail ratio for transmissions.
static histogram<uint8_t> g_histTxOk(2, 0, 2);
// Histogram to track succes/fail ratio for reception (complete ping-pong cycle).
static histogram<uint8_t> g_histTxRxOk(2, 0, 2);
const uint32_t freqDisplayStep = 1000000ul;
static uint16_t g_pingIntervalMs = PING_INTERVAL_DEFAULT_MS;    // Interval for the master to emit a Ping message

void before()
{
  // Clear eeprom, in case any old settings are preset
  for (size_t i = 0; i < EEPROM_LOCAL_CONFIG_ADDRESS; ++i)
  {
    hwWriteConfig(i, 0xFF);
  } 

  // Define RSSI histogram size
  const auto rssiMin  = g_radio.GetRssiMin();
  const auto rssiMax  = g_radio.GetRssiMax();
  const auto rssiRange = rssiMax - rssiMin;
  auto rssiStep = g_radio.GetRssiStep();

  const size_t extraBuckets = 2;       // +1 for rounding, +1 for failure bucket
  size_t numBuckets;
  for(;;)
  {
    numBuckets = rssiRange / rssiStep;
    if (numBuckets <= (HISTOGRAM_RSSI_MAX_BUCKETS - extraBuckets))
      break;
    ++rssiStep;
  } 
  numBuckets += extraBuckets;
  g_histRssi.resize( numBuckets,
                     rssiMin - rssiStep / 2 - rssiStep /*failure bucket*/,
                     rssiMax + rssiStep / 2 + 1 /*+1 for rounding*/);
  g_histFailedRssiValue = rssiMin - rssiStep / 2 - rssiStep;
  screenUpdate(true);
}

void setup()
{
  // Prepare ping-pong transmit message to other node.
  g_txMsgPingPong.setDestination(MY_PARENT_NODE_ID);
  g_txMsgPingPong.setSensor(CHILD_ID_PINGPONG);
  g_txMsgPingPong.setType(V_VAR1);  

  // Prepare config message.
  g_txMsgConfig.setDestination(MY_PARENT_NODE_ID);
  g_txMsgConfig.setSensor(CHILD_ID_CONFIG);
  g_txMsgConfig.setType(V_VAR1);  
  //  g_config.setDefault();
  //  g_txMsgConfig.set(&g_config, sizeof(g_config));  
}

#define ROW_WIDTH                 (60)
#define COL_WIDTH_CONFIG          (ROW_WIDTH/4)
#define WIDTH_HIST_RSSI_BUCKETS   (8)
#define WIDTH_HIST_RSSI_COUNTS    (8)
#define WIDTH_HIST_RSSI           (ROW_WIDTH-WIDTH_HIST_RSSI_BUCKETS-WIDTH_HIST_RSSI_COUNTS)
#define WIDTH_HIST_TXOK_BUCKETS   (WIDTH_HIST_RSSI_BUCKETS)
#define WIDTH_HIST_TXOK           (WIDTH_HIST_RSSI)
#define WIDTH_HIST_TXOK_COUNTS    (WIDTH_HIST_RSSI_COUNTS)
#define WIDTH_HIST_RXOK_BUCKETS   (WIDTH_HIST_RSSI_BUCKETS)
#define WIDTH_HIST_RXOK           (WIDTH_HIST_RSSI)
#define WIDTH_HIST_RXOK_COUNTS    (WIDTH_HIST_RSSI_COUNTS)
#define COL_WIDTH_MENU            (ROW_WIDTH/2)

void nextCol(uint8_t& col, uint8_t& row, const uint8_t colWidth, const uint8_t rowWidth)
{
  col += colWidth;
  if (col > rowWidth - colWidth + 1)
  {
    col = 1;
    ++row;
  }
}

void screenUpdate(const bool rebuild)
{
#ifndef MY_DEBUG
  static unsigned long prevMs = millis();
  const unsigned long nowMs = millis();
  if (not (rebuild or ((nowMs - prevMs) >= SCREEN_UPDATE_INTERVAL_MS)))
  {
    return;
  }
  prevMs = nowMs;

  uint8_t x = 1;
  uint8_t y = 1;
  if (rebuild)
  {
    Serial << home() << eraseScreen() << defaultBackground() << defaultForeground();
    // Header
    Serial << setForegroundColor(YELLOW)
           << xy(x,y) << boldOn() << F("MySensors range test ") << F(SKETCH_VERSION_STR)
           << F(" -- ") << g_radio.GetName() << F(" -- ") << F(ROLE_STR) << boldOff();
    y += 2;
    // Config
    Serial << setForegroundColor(CYAN);
    Serial << xy(x,y) << F("Frequency");
    nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
    Serial << xy(x,y) << rightAlignStr(g_radio.FrequencyHzToString(g_radio.GetFrequencyHz()), COL_WIDTH_CONFIG-1);
    nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);

    uint8_t baseId[10];
    size_t size = sizeof(baseId);
    if (g_radio.GetBaseId(baseId, size))
    {
      Serial << xy(x,y) << F("BaseId");
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
      Serial << xy(x,y) << rightAlignStr(g_radio.BaseIdToString(baseId, size), COL_WIDTH_CONFIG-1);
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
    }
    if (g_radio.CanSetDataRate())
    {
      Serial << xy(x,y) << F("Datarate");
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
      Serial << xy(x,y) << rightAlignStr(g_radio.DataRateToString(g_radio.GetDataRate()), COL_WIDTH_CONFIG-1) << boldOff();
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
    }
    if (g_radio.CanSetPowerLevel())
    {
      Serial << xy(x,y) << F("PowerLevel");
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
      Serial << xy(x,y) << rightAlignStr(g_radio.PowerLevelToString(g_radio.GetPowerLevel()), COL_WIDTH_CONFIG-1) << boldOff();
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
    }
    if (g_radio.CanSetAutoRetransmits())
    {
      Serial << xy(x,y) << F("Retransmits");
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
      Serial << xy(x,y) << rightAlignStr(g_radio.AutoRetransmitsToString(g_radio.GetAutoRetransmits()), COL_WIDTH_CONFIG-1) << boldOff();
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
    }
    if (g_radio.CanSetAutoRetransmitDelay())
    {
      Serial << xy(x,y) << F("Retrans. delay");
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
      Serial << xy(x,y) << rightAlignStr(g_radio.AutoRetransmitDelayToString(g_radio.GetAutoRetransmitDelay()) + F("us"), COL_WIDTH_CONFIG-1);
      nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
    }
    Serial << xy(x,y) << F("Payload size");
    nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
    Serial << xy(x,y) << rightAlignStr(String(g_payloadLen), COL_WIDTH_CONFIG-1) << boldOff();
    nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);

    Serial << xy(x,y) << F("Ping interval");
    nextCol(x, y, COL_WIDTH_CONFIG, ROW_WIDTH);
    Serial << xy(x,y) << rightAlignStr(String(g_pingIntervalMs) + F("ms"), COL_WIDTH_CONFIG-1);
  }

  // Store y position of histogram. This is needed when redrawing only the histogram.
  static uint8_t yHist = 0;
  if (rebuild)
  {
    yHist = y + 2;
  }
  y = yHist;

  // RSSI histogram
  {
    x = WIDTH_HIST_RSSI_BUCKETS+1;
    uint8_t w = WIDTH_HIST_RSSI;
    for (size_t b = 0; b < g_histRssi.size(); ++b)
    {
      double relcount = g_histRssi.relcount(b);
      int xc = int(double(w) * relcount + 0.5);
      if (rebuild)
      {
        int16_t db = g_histRssi.lowerbound(b) + (g_histRssi.upperbound(b) - g_histRssi.lowerbound(b)) / 2;
        // Special case: Failed transmissions are stored in bucket 0
        String s = (b == 0) ? String(F("Failed")) : (String(db) + F(" dB"));
        Serial << xy(1,y) << defaultBackground() << setForegroundColor(YELLOW) << rightAlignStr(s, WIDTH_HIST_RSSI_BUCKETS-1);
      }
      Serial << setBackgroundColor(YELLOW);
      Serial << fill(x,  y, x+xc-1, y, ' ');
      Serial << defaultBackground() << setForegroundColor(YELLOW);
      Serial << fill(x+xc, y, x+w-1,  y, '-');

      Serial << xy(WIDTH_HIST_RSSI_BUCKETS+WIDTH_HIST_RSSI,y) << rightAlignStr(String(g_histRssi.count(b)), WIDTH_HIST_RSSI_COUNTS) << boldOff();
      ++y;
    }
    // SLowly decrease bucket values, to limit history time
    g_histRssi.decay(g_histogramDecayRate);
  }

  // Tx Ok/Fail histogram
  {
    x = WIDTH_HIST_TXOK_BUCKETS+1;
    ++y;
    uint8_t w = WIDTH_HIST_TXOK;  
    if (rebuild)
    {
      Serial << defaultBackground() << setForegroundColor(YELLOW);
      Serial << xy(1,y) << F("Tx ") << setForegroundColor(GREEN) << F("Ok");
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
    Serial << xy(WIDTH_HIST_TXOK_BUCKETS+WIDTH_HIST_TXOK,y) << setForegroundColor(GREEN) << rightAlignStr(String(perc) + F(" %"), WIDTH_HIST_TXOK_COUNTS) << boldOff();
    Serial << defaultForeground();
    // SLowly decrease bucket values, to limit history time
    g_histTxOk.decay(g_histogramDecayRate);
  }

#ifdef MASTER
  // Rx Ok/Fail histogram
  {
    x = WIDTH_HIST_RXOK_BUCKETS+1;
    ++y;
    uint8_t w = WIDTH_HIST_RXOK;  
    if (rebuild)
    {
      Serial << defaultBackground() << setForegroundColor(YELLOW);
      Serial << xy(1,y) << F("TxRx ") << setForegroundColor(GREEN) << F("Ok");
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
    Serial << xy(WIDTH_HIST_RXOK_BUCKETS+WIDTH_HIST_RXOK,y) << setForegroundColor(GREEN) << rightAlignStr(String(perc) + F(" %"), WIDTH_HIST_RXOK_COUNTS) << boldOff();
    Serial << defaultForeground();
    // SLowly decrease bucket values, to limit history time
    g_histTxRxOk.decay(g_histogramDecayRate);
  }
#endif

  // Menu
  if (rebuild)
  {
    x = 1;
    y += 2;
    Serial << setForegroundColor(CYAN);
#ifdef MASTER
    Serial << xy(x,y) << boldOn() << F("Menu") << boldOff();
    ++y;
    if (g_radio.CanSetFrequencyHz())
    {
      Serial << xy(x,y) << F("fn - Frequency [") << int(g_radio.GetFrequencyMinHz()/freqDisplayStep) << F("..") << int(g_radio.GetFrequencyMaxHz()/freqDisplayStep) << ']';
      nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
    }
    if (g_radio.CanSetDataRate())
    {
      Serial << xy(x,y) << F("rn - Datarate [") << int(g_radio.GetDataRateMin()) << F("..") << int(g_radio.GetDataRateMax()) << ']';
      nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
    }
    if (g_radio.CanSetPowerLevel())
    {
      Serial << xy(x,y) << F("pn - PowerLevel [") << int(g_radio.GetPowerLevelMin()) << F("..") << int(g_radio.GetPowerLevelMax()) << ']';
      nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
    }
    if (g_radio.CanSetAutoRetransmits())
    {
      Serial << xy(x,y) << F("tn - Retransmits [") << int(g_radio.GetAutoRetransmitsMin()) << F("..") << int(g_radio.GetAutoRetransmitsMax()) << ']';
      nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
    }
    if (g_radio.CanSetAutoRetransmitDelay())
    {
      Serial << xy(x,y) << F("yn - Retrans. delay [") << int(g_radio.GetAutoRetransmitDelayMin()) << F("..") << int(g_radio.GetAutoRetransmitDelayMax()) << ']';
      nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
    }
    Serial << xy(x,y) << F("ln - Payload size [1..") << MAX_PAYLOAD << ']';
    nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
    Serial << xy(x,y) << F("in - Ping interval [") << MIN_PING_INTERVAL_MS << F("..") << MAX_PING_INTERVAL_MS << ']';
    nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
    Serial << xy(x,y) << F("x  - Reset settings");
    nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
#endif
    Serial << xy(x,y) << F("0  - Reset statistics");
    nextCol(x, y, COL_WIDTH_MENU, ROW_WIDTH);
    Serial << xy(x,y) << F("dn - Histogram decay [0..5]");
  }
  // Position cursor on next line. Any serial prints in the rest of the program will end up here.
  ++y;
  Serial << xy(1,y);
#endif // ifndef MY_DEBUG
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
bool processCommand(t_command& cmd)
{
  bool configChanged = false;
  if (cmd.m_complete)
  {
    int32_t value = cmd.m_text.substring(1, cmd.m_text.length()).toInt();
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
      g_radio.SetConfigDefault();
      g_pingIntervalMs = PING_INTERVAL_DEFAULT_MS;
      configChanged = true;
    }
    else if (cmd.m_text.startsWith(F("f")))
    {
      const uint32_t freqHz = uint32_t(value) * freqDisplayStep;
      if ((freqHz >= g_radio.GetFrequencyMinHz()) and (freqHz <= g_radio.GetFrequencyMaxHz()))
      {
        configChanged = g_radio.SetFrequencyHz(freqHz);
      }
    }
    else if (cmd.m_text.startsWith(F("r")))
    {
      if ((value >= g_radio.GetDataRateMin()) and (value <= g_radio.GetDataRateMax()))
      {
        configChanged = g_radio.SetDataRate(value);
      }
    }
    else if (cmd.m_text.startsWith(F("p")))
    {
      if ((value >= g_radio.GetPowerLevelMin()) and (value <= g_radio.GetPowerLevelMax()))
      {
        configChanged = g_radio.SetPowerLevel(value);
      }
    }
    else if (cmd.m_text.startsWith(F("t")))
    {
      if ((value >= g_radio.GetAutoRetransmitsMin()) and (value <= g_radio.GetAutoRetransmitsMax()))
      {
        configChanged = g_radio.SetAutoRetransmits(value);
      }
    }
    else if (cmd.m_text.startsWith(F("y")))
    {
      if ((value >= g_radio.GetAutoRetransmitDelayMin()) and (value <= g_radio.GetAutoRetransmitDelayMax()))
      {
        configChanged = g_radio.SetAutoRetransmitDelay(value);
      }
    }
    else if (cmd.m_text.startsWith(F("l")))
    {
      if ((value >= PAYLOAD_LENGTH_MIN) && (value <= PAYLOAD_LENGTH_MAX))
      {
        g_payloadLen = value;
      }
    }
    else if (cmd.m_text.startsWith(F("i")))
    {
      if ((value >= MIN_PING_INTERVAL_MS) && (value <= MAX_PING_INTERVAL_MS))
      {
        g_pingIntervalMs = value;
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
    const char c = static_cast<char>(Serial.read());
    if (c == '\r')
      g_serialCommand.m_complete = true;
    else if ((c >= 32 ) and (c < 127))
      g_serialCommand.m_text += c;
  }
}

void logResults(const t_txData& data)
{
  g_histTxOk.store(data.m_txOk ? 1 : 0);
  g_histTxRxOk.store(data.m_rxOk ? 1 : 0);
  // If Tx failed: Store in RSSI for first bucket
  g_histRssi.store(data.m_txOk ? data.m_rssi : g_histFailedRssiValue);
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
  g_txData.m_rssi   = g_radio.GetRssi();
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
  data.m_rssi   = g_radio.GetRssi();
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
  size_t size = MAX_PAYLOAD;
  if (g_radio.ConfigSerialize(static_cast<uint8_t*>(g_txMsgConfig.getCustom()), size))
  {
    mSetLength(g_txMsgConfig, size);
    for (size_t i = 0; !sendOk && (i < MAX_NUM_CONFIG_RETRIES); ++i)
    {
      // Serial.print(F("Send config. Attempt "));
      // Serial.print(i+1);
      // Serial.print('/');
      // Serial.println(MAX_NUM_CONFIG_RETRIES); 
      sendOk = send(g_txMsgConfig);
    }
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
    const bool payloadLengthChanged = len != g_payloadLen;
    g_payloadLen = len;
    receivedPing(rxTsUs, data);
    if (payloadLengthChanged)
    {
      screenUpdate(true);
    }
#endif
  }
  else if ((CHILD_ID_CONFIG == rxMsg.sensor) && (V_VAR1 == rxMsg.type))
  {
    // New radio config received
    if (g_radio.ConfigDeserialize(const_cast<const uint8_t*>(static_cast<uint8_t*>(rxMsg.getCustom())), mGetLength(rxMsg)))
    {
      screenUpdate(true);
      wait(10);
      g_radio.ConfigActivate();
    }
  }
}

void loop()
{
#ifdef MASTER
  static unsigned long prevMs = millis();
  const unsigned long nowMs = millis();
  if ((nowMs - prevMs) >= g_pingIntervalMs)
  {
    static bool first = true;
    if (first)
    {
      prevMs = nowMs;
      first = false;
    }
    else
    {
      prevMs = prevMs + g_pingIntervalMs;
      // Ping-pong has been performed. Log the results.
      logResults(g_txData);
    }
    // Initiate next ping-pong
    sendPing(g_txData);
  }

  if (processCommand(g_serialCommand))
  {
    // Config changed. Send new config to parent and activate it.
    bool configSentOk = sendConfig();
    if (configSentOk)
    {
//      Serial.println(F("-- Activating new radio configuration on Master --"));
//      g_config.log();
//      wait(10);
      g_radio.ConfigActivate();
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
  (void)processCommand(g_serialCommand);
#endif

  screenUpdate(false);
}
