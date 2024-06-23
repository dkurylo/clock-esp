#include <Arduino.h>
#include <vector>
#include <map>

#include "TCFont0.h"
#include "TCFont1.h"
#include "TCFont2.h"

class TCFonts {

  public:
    static uint8_t getSymbolWidth( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall );
    static uint8_t getSymbolLp( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall );
    static uint8_t getSymbolRp( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall );
    static std::vector<uint8_t> getSymbol( uint8_t fontIndex, char symbol, bool isCompact, bool isBold, bool isWide, bool isSmall, bool isProgress );

  private:
    static std::map<char, uint8_t> charToCharIndex;
};
