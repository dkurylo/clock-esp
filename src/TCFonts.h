#include <Arduino.h>
#include <vector>
#include <map>

#include "TCFont0.h"
#include "TCFont1.h"
#include "TCFont2.h"

class TCFonts {

  public:
    static const uint8_t NUMBER_OF_FONTS_SUPPORTED = 4;
    static const uint8_t FONT_HEIGHT = 8;
    static const uint16_t FONT_SYMBOLS = 16*14;

    static uint8_t getSymbolWidth( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall );
    static uint8_t getSymbolLp( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall );
    static uint8_t getSymbolRp( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall );
    static std::pair<const uint8_t (*)[TCFonts::FONT_HEIGHT], bool> getFont( uint8_t fontIndex );
    static std::vector<uint8_t> getSymbol( uint8_t fontIndex, char symbol, bool isCompact, bool isBold, bool isWide, bool isSmall, bool isProgress );
    static uint8_t (*getCustomFont())[TCFonts::FONT_HEIGHT];
    static void setCustomFont( uint8_t (*fontToUse)[TCFonts::FONT_HEIGHT] );

  private:
    static std::map<char, uint8_t> charToCharIndex;
    static uint8_t customFont[TCFonts::FONT_SYMBOLS][TCFonts::FONT_HEIGHT];
};
