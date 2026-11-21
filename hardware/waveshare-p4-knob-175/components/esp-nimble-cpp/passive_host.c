// PlatformIO includes each registered component archive in the P4 linker inputs.
#include "sdkconfig.h"
_Static_assert(CONFIG_BT_BLUEDROID_ENABLED, "Passive P4 discovery uses Bluedroid");
