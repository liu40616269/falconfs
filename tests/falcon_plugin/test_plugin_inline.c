// Test plugin - INLINE type

#include <stdio.h>
#include <stdlib.h>
#include "plugin/falcon_plugin_framework.h"

static int g_work_called = 0;
static int g_initialized = 0;

int plugin_init(void)
{
    printf("[TEST PLUGIN INLINE] plugin_init() called\n");
    g_initialized = 1;
    return 0;
}

FalconPluginWorkType plugin_get_type(void)
{
    printf("[TEST PLUGIN INLINE] plugin_get_type() called, returning INLINE\n");
    return FALCON_PLUGIN_TYPE_INLINE;
}

int plugin_work(const FalconPluginSharedData *shared_data)
{
    printf("[TEST PLUGIN INLINE] plugin_work() called\n");
    if (shared_data) {
        printf("[TEST PLUGIN INLINE] Plugin name: %s\n", shared_data->plugin_name);
        printf("[TEST PLUGIN INLINE] Plugin path: %s\n", shared_data->plugin_path);
        printf("[TEST PLUGIN INLINE] Custom config: %s\n", shared_data->custom_config);
    }
    g_work_called++;
    return 1; // stop work for INLINE type
}

void plugin_cleanup(void)
{
    printf("[TEST PLUGIN INLINE] plugin_cleanup() called\n");
    g_initialized = 0;
}

// Test helper functions
int test_get_work_count(void) { return g_work_called; }

void test_reset_counters(void)
{
    g_work_called = 0;
    g_initialized = 0;
}