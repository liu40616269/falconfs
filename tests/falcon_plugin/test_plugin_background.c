#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "plugin/falcon_plugin_framework.h"

static int g_work_called = 0;
static int g_work_limit = 3;
static int g_initialized = 0;

int plugin_init(void)
{
    printf("[TEST PLUGIN BACKGROUND] plugin_init() called\n");
    return 0;
}

FalconPluginWorkType plugin_get_type(void)
{
    printf("[TEST PLUGIN BACKGROUND] plugin_get_type() called, returning BACKGROUND\n");
    return FALCON_PLUGIN_TYPE_BACKGROUND;
}

int plugin_work(const FalconPluginSharedData *shared_data)
{
    /* Self-initialize on first call */
    if (!g_initialized) {
        printf("[TEST PLUGIN BACKGROUND] Self-initializing in work function\n");
        if (shared_data) {
            printf("[TEST PLUGIN BACKGROUND] Plugin name: %s\n", shared_data->plugin_name);
            printf("[TEST PLUGIN BACKGROUND] Plugin path: %s\n", shared_data->plugin_path);
            printf("[TEST PLUGIN BACKGROUND] Custom config: %s\n", shared_data->custom_config);
        }
        g_initialized = 1;
    }

    printf("[TEST PLUGIN BACKGROUND] plugin_work() called (count: %d)\n", g_work_called + 1);
    g_work_called++;

    usleep(10000); // simulate work

    if (g_work_called >= g_work_limit) {
        printf("[TEST PLUGIN BACKGROUND] Work limit reached, stopping\n");
        return 1; // stop work
    }

    return 0; // continue working
}

void plugin_cleanup(void)
{
    printf("[TEST PLUGIN BACKGROUND] plugin_cleanup() called\n");
}

int test_get_work_count(void) { return g_work_called; }

void test_set_work_limit(int limit)
{
    g_work_limit = limit;
}

void test_reset_counters(void)
{
    g_work_called = 0;
    g_initialized = 0;
}