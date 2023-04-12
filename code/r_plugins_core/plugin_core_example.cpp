#include <stdio.h>
#include <math.h>
#include "../public/ruby_core_plugin.h"
#include "../public/utils/core_plugins_utils.h"

const char* g_szPluginNameExample = "Example Core Plugin";
const char* g_szUIDExample = "A34776T-A2123Q-WE19J-XX24";

u32 g_uRuntimeLocation = 0;
u32 g_uAllocatedCapabilities = 0;

#ifdef __cplusplus
extern "C" {
#endif

// This is the first method called at runtime, on any runtime location where the plugin is running (vehicle/controller).
// The plugin should just return the desired capabilities.
u32 core_plugin_on_requested_capabilities()
{
   core_plugin_util_log_line("My plugin request capabilities.");

   return CORE_PLUGIN_CAPABILITY_DATA_STREAM | CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART;
}

// The plugin should return a user friendly name for the plugin
const char* core_plugin_get_name()
{
   return g_szPluginNameExample;
}

// The plugin should return a unique alphanumeric string GUID (i.e. "342-ASDFSA-232-JASKD")
const char* core_plugin_get_guid()
{
   return g_szUIDExample;
}

// The plugin should return it's current version. It's used by Ruby to manage updates of the plugins and versioning.
int core_plugin_get_version()
{
   return 1;
}

// This method should be used to actually initialize any data the plugin needs.
// The actual capabilities allocated by Ruby to the plugin are in the uAllocatedCapabilities parameter.
// Plugin should return 0 if the initialization succeeded.
int core_plugin_init(u32 uRuntimeLocation, u32 uAllocatedCapabilities)
{
   g_uRuntimeLocation = uRuntimeLocation;
   g_uAllocatedCapabilities = uAllocatedCapabilities;

   core_plugin_util_log_line("My plugin init");
   return 0;
}

// This is the last method called at runtime, before a plugin is unloaded (due to a reboot or plugin uninstall).
void core_plugin_uninit()
{
   core_plugin_util_log_line("My plugin uninit");
}


#ifdef __cplusplus
}
#endif