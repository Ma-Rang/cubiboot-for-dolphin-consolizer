#include "../../cubeboot/source/config.h"

#define PRINT_READDIR_NAMES
#define PRINT_READDIR_FILES
// #define PRINT_FONT_PTRS

#if defined(GECKO_PRINT_ENABLE) || defined(DOLPHIN_PRINT_ENABLE)
#define DEBUG
#endif
