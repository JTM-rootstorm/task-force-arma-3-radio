#pragma once

#ifdef AppVeyorBuild
#define PLUGIN_VERSION AppVeyorBuild
#define isCI 1
#else
#define PLUGIN_VERSION "1.0.0.1"
#endif

#ifdef TFAR_LINUX_PLUGIN
#define PLUGIN_VERSION_DISPLAY PLUGIN_VERSION "-linux"
#else
#define PLUGIN_VERSION_DISPLAY PLUGIN_VERSION
#endif

#define RESOURCE_VERSION 1,0,0,1
#ifdef X64BUILD
#define RESOURCE_FILENAME  "task_force_radio_win64.dll"
#else
#define RESOURCE_FILENAME  "task_force_radio_win32.dll"
#endif
