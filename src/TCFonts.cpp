#include "TCFonts.h"

uint8_t TCFonts::getSymbolWidth( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall ) {
  switch( fontIndex ) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5: {
      switch( isSmall ) {
        case false: {
          switch( symbol ) {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '0':
            case ' ':
            case '-': {
              switch( isWide ) {
                case true: {
                  return 6;
                }
                case false: {
                  return 4;
                }
              }
            }
            case ':':
            case '\b':
            case '\f':
            case '\t': {
              switch( isWide ) {
                case true: {
                  return 1;
                }
                case false: {
                  return 1;
                }
              }
            }
          }
        }
        case true: {
          switch( isWide ) {
            case false: {
              switch( isCompact ) {
                case false: {
                  return 3;
                }
                case true: {
                  return 4;
                }
              }
            }
          }
        }
      }
    }
  }
  return 0;
}

uint8_t TCFonts::getSymbolLp( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall ) {
  switch( fontIndex ) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5: {
      switch( isSmall ) {
        case false: {
          switch( symbol ) {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '0':
            case ' ':
            case '-': {
              switch( isCompact ) {
                case false: {
                  switch( isWide ) {
                    case false: {
                      return 0;
                    }
                    case true: {
                      return 1;
                    }
                  }
                }
                case true: {
                  switch( isWide ) {
                    case false: {
                      return 0;
                    }
                    case true: {
                      return 1;
                    }
                  }
                }
              }
            }
            case ':':
            case '\b':
            case '\f':
            case '\t': {
              switch( isCompact ) {
                case false: {
                  switch( isWide ) {
                    case false: {
                      return 1;
                    }
                    case true: {
                      return 2;
                    }
                  }
                }
                case true: {
                  switch( isWide ) {
                    case false: {
                      return 0;
                    }
                    case true: {
                      return 1;
                    }
                  }
                }
              }
            }
          }
        }
        case true: {
          switch( isWide ) {
            case false: {
              return 1;
            }
          }
        }
      }
    }
  }
  return 0;
}

uint8_t TCFonts::getSymbolRp( uint8_t fontIndex, char symbol, bool isCompact, bool isWide, bool isSmall ) {
  switch( fontIndex ) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5: {
      switch( isSmall ) {
        case false: {
          switch( symbol ) {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '0':
            case ' ':
            case '-': {
              switch( isCompact ) {
                case false: {
                  switch( isWide ) {
                    case false: {
                      return 1;
                    }
                    case true: {
                      return 0;
                    }
                  }
                }
                case true: {
                  switch( isWide ) {
                    case false: {
                      return 1;
                    }
                    case true: {
                      return 0;
                    }
                  }
                }
              }
            }
            case ':':
            case '\b':
            case '\f':
            case '\t': {
              switch( isCompact ) {
                case false: {
                  switch( isWide ) {
                    case false: {
                      return 2;
                    }
                    case true: {
                      return 1;
                    }
                  }
                }
                case true: {
                  switch( isWide ) {
                    case false: {
                      return 1;
                    }
                    case true: {
                      return 0;
                    }
                  }
                }
              }
            }
          }
        }
        case true: {
          switch( isWide ) {
            case false: {
              return 0;
            }
          }
        }
      }
    }
  }
  return 0;
}


std::pair<const uint8_t (*)[TCFonts::FONT_HEIGHT], bool> TCFonts::getFont(uint8_t fontIndex) {
    if( fontIndex == 1 ) {
      return { TCFont1::font, false };
    } else if( fontIndex == 2 ) {
      return { TCFont2::font, false };
    } else if ( fontIndex == 3 ) {
      return { TCFont3::font, false };
    } else if ( fontIndex == 4 ) {
      return { TCFont4::font, false };
    } else if ( fontIndex == 5 ) {
      return { TCFonts::customFont, true };
    } else {
      return { TCFont1::font, false }; // Default to TCFont1
    }
}

std::map<char, uint8_t> TCFonts::charToCharIndex = {
  { '1', 0 },
  { '2', 1 },
  { '3', 2 },
  { '4', 3 },
  { '5', 4 },
  { '6', 5 },
  { '7', 6 },
  { '8', 7 },
  { '9', 8 },
  { '0', 9 },
  { '-', 10 },
  { ':', 11 },
  { '\b', 12 },
  { '\f', 13 },
};

std::vector<uint8_t> TCFonts::getSymbol( uint8_t fontIndex, char symbol, bool isCompact, bool isBold, bool isWide, bool isSmall, bool isProgress ) {
  std::pair<const uint8_t (*)[TCFonts::FONT_HEIGHT], bool> fontInfo = TCFonts::getFont(fontIndex);
  const uint8_t (*fontToUse)[TCFonts::FONT_HEIGHT] = fontInfo.first;
  bool isCustomFont = fontInfo.second;

  auto charPositionIt = TCFonts::charToCharIndex.find( symbol );
  if( charPositionIt == TCFonts::charToCharIndex.end() ) {
    return { 0, 0, 0, 0, 0, 0, 0, 0 };
  }

  //  0 big    normal    wide
  //  1 big    normal    wide   bold
  //  2 big    normal    thin
  //  3 big    normal    thin   bold
  //  4 big    progress  wide
  //  5 big    progress  wide   bold
  //  6 big    progress  thin
  //  7 big    progress  thin   bold
  //  8 small  normal    wider
  //  9 small  normal    wider  bold
  // 10 small  normal    thin
  // 11 small  normal    thin   bold
  // 12 small  progress  wider  
  // 13 small  progress  wider  bold
  // 14 small  progress  thin
  // 15 small  progress  thin   bold
  uint16_t charPosition = charPositionIt->second;
  charPosition *= 16;
  if( isSmall ) {
    charPosition += 8;
  }
  if( isProgress ) {
    charPosition += 4;
  }
  if( ( !isSmall && !isWide ) || ( isSmall && !isCompact ) ) { //small & wide or large and compact can not exist
    charPosition += 2;
  }
  if( isBold ) {
    charPosition += 1;
  }

  std::vector<uint8_t> symbolToUse;
  for( uint8_t symbolLineIndex = 0; symbolLineIndex < TCFonts::FONT_HEIGHT; symbolLineIndex++ ) {
    uint8_t symbolLine;
    if( isCustomFont ) {
      symbolLine = fontToUse[charPosition][symbolLineIndex];
    } else {
      symbolLine = pgm_read_byte( &(fontToUse[charPosition][symbolLineIndex]) );
    }
    symbolToUse.push_back( symbolLine );
  }

  return symbolToUse;
}


uint8_t TCFonts::customFont[TCFonts::FONT_SYMBOLS][TCFonts::FONT_HEIGHT]; //pre-allocated RAM for custom font

uint8_t (*TCFonts::getCustomFont())[TCFonts::FONT_HEIGHT] {
  return TCFonts::customFont;
}

void TCFonts::setCustomFont( uint8_t (*fontToUse)[TCFonts::FONT_HEIGHT] ) {
    for( size_t i = 0; i < TCFonts::FONT_SYMBOLS; i++ ) {
        for( size_t j = 0; j < TCFonts::FONT_HEIGHT; j++ ) {
            TCFonts::customFont[i][j] = fontToUse[i][j];
        }
    }
}