#pragma once

static char ToHexChar(uint8_t v)
{
  v &= 0x0F;
  if (v < 10)
    return '0'+v;
  return 'A'-10+v;
}

template <typename T> static String ToHexStr(T val, const size_t numDigits = 0)
{
  String s;
  const size_t digits = numDigits > 0 ? numDigits : sizeof(val)*2;
  for (size_t i = 0; i < digits; ++i)
  {
    const uint8_t hex = val >> (8*sizeof(val)-4);
    s += ToHexChar(hex);
    val <<= 4;
  }
  return s;
}
