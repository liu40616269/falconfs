/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#ifndef FALCON_PLUGIN_FRAMEWORK_H
#define FALCON_PLUGIN_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#define FALCON_PLUGIN_INIT_FUNC_NAME      "plugin_init"
#define FALCON_PLUGIN_WORK_FUNC_NAME      "plugin_work"
#define FALCON_PLUGIN_GET_TYPE_FUNC_NAME  "plugin_get_type"
#define FALCON_PLUGIN_CLEANUP_FUNC_NAME   "plugin_cleanup"

#define FALCON_PLUGIN_MAX_CONFIG_SIZE     4096
#define FALCON_PLUGIN_MAX_NAME_SIZE       256

typedef enum {
    FALCON_PLUGIN_TYPE_INLINE = 0,
    FALCON_PLUGIN_TYPE_BACKGROUND = 1
} FalconPluginWorkType;

typedef struct FalconPluginSharedData {
    char plugin_name[FALCON_PLUGIN_MAX_NAME_SIZE];
    char plugin_path[FALCON_PLUGIN_MAX_NAME_SIZE];
    char custom_config[FALCON_PLUGIN_MAX_CONFIG_SIZE];
    pid_t main_pid;
} FalconPluginSharedData;

typedef int (*falcon_plugin_init_func_t)(void);
typedef int (*falcon_plugin_work_func_t)(const FalconPluginSharedData *shared_data);
typedef FalconPluginWorkType (*falcon_plugin_get_type_func_t)(void);
typedef void (*falcon_plugin_cleanup_func_t)(void);

#ifdef __cplusplus
}
#endif

#endif /* FALCON_PLUGIN_FRAMEWORK_H */