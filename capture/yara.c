/* yara.c  -- Functions dealing with yara library
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "moloch.h"
#include "yara.h"

extern MolochConfig_t config;

/******************************************************************************/
char *moloch_yara_version() {
    static char buf[100];
#ifdef YR_MAJOR_VERSION
 #ifdef YR_MINOR_VERSION
  #ifdef YR_MICRO_VERSION
    snprintf(buf, sizeof(buf), "%d.%d.%d", YR_MAJOR_VERSION, YR_MINOR_VERSION, YR_MICRO_VERSION);
  #else /* YR_MICRO_VERSION */
    snprintf(buf, sizeof(buf), "%d.%d", YR_MAJOR_VERSION, YR_MINOR_VERSION);
  #endif /* YR_MICRO_VERSION */
 #else /* YR_MINOR_VERSION */
    snprintf(buf, sizeof(buf), "%d.x", YR_MAJOR_VERSION);
 #endif /* YR_MINOR_VERSION */
#else /* YR_MAJOR_VERSION */
 #ifdef STRING_IS_HEX
    snprintf(buf, sizeof(buf), "2.x");
 #endif /* STRING_IS_HEX */
#endif /* YR_MAJOR_VERSION */
    return buf;
}




#if YR_MAJOR_VERSION == 4
// Yara 4, https://github.com/VirusTotal/yara/wiki/Backward-incompatible-changes-in-YARA-4.0-API
LOCAL  YR_COMPILER *yCompiler = 0;
LOCAL  YR_COMPILER *yEmailCompiler = 0;
LOCAL  YR_RULES    *yRules = 0;
LOCAL  YR_RULES    *yEmailRules = 0;
LOCAL  int         yFlags = 0;

/******************************************************************************/
// Yara 4 compiler callback: const YR_RULE* rule inbetween int line_number and const char* message.
void moloch_yara_report_error(int error_level, const char* file_name, int line_number, const YR_RULE* UNUSED(rule), const char* error_message, void* UNUSED(user_data))
{
    LOG("%d %s:%d: %s\n", error_level, file_name, line_number, error_message);
}
/******************************************************************************/
void moloch_yara_open(char *filename, YR_COMPILER **compiler, YR_RULES **rules)
{
    yr_compiler_create(compiler);
    (*compiler)->callback = moloch_yara_report_error;

    if (filename) {
        FILE *rule_file;

        rule_file = fopen(filename, "r");

        if (rule_file != NULL) {
            int errors = yr_compiler_add_file(*compiler, rule_file, NULL, filename);

            fclose(rule_file);

            if (errors) {
                exit (0);
            }
            yr_compiler_get_rules(*compiler, rules);
        } else {
            CONFIGEXIT("yara could not open file: %s\n", filename);
        }
    }
}
/******************************************************************************/
void moloch_yara_load(char *name)
{
    YR_COMPILER *compiler;
    YR_RULES *rules;

    if (!name)
        return;

    moloch_yara_open(name, &compiler, &rules);

    if (yRules)
        moloch_free_later(yRules, (GDestroyNotify) yr_rules_destroy);
    if (yCompiler)
        moloch_free_later(yCompiler, (GDestroyNotify) yr_compiler_destroy);

    yCompiler = compiler;
    yRules = rules;
}
/******************************************************************************/
void moloch_yara_load_email(char *name)
{
    YR_COMPILER *compiler;
    YR_RULES *rules;

    if (!name)
        return;

    moloch_yara_open(name, &compiler, &rules);

    if (yEmailRules)
        moloch_free_later(yEmailRules, (GDestroyNotify) yr_rules_destroy);
    if (yEmailCompiler)
        moloch_free_later(yEmailCompiler, (GDestroyNotify) yr_compiler_destroy);

    yEmailCompiler = compiler;
    yEmailRules = rules;
}
/******************************************************************************/
void moloch_yara_init()
{
    if (moloch_config_boolean(NULL, "yaraFastMode", TRUE))
        yFlags |= SCAN_FLAGS_FAST_MODE;

    yr_initialize();

    if (config.yara)
        moloch_config_monitor_file("yara file", config.yara, moloch_yara_load);

    if (config.emailYara)
        moloch_config_monitor_file("yara email file", config.emailYara, moloch_yara_load_email);
}

/******************************************************************************/
// Yara 4: scanning callback now has a YR_SCAN_CONTEXT* context as 0th param.
int moloch_yara_callback(YR_SCAN_CONTEXT* UNUSED(context), int message, YR_RULE* rule, MolochSession_t* session)
{
    if (message != CALLBACK_MSG_RULE_MATCHING)
        return CALLBACK_CONTINUE;

    char tagname[256];
    const char* tag;

    snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier);
    moloch_session_add_tag(session, tagname);
    tag = rule->tags;
    while(tag != NULL && *tag) {
        snprintf(tagname, sizeof(tagname), "yara:%s", tag);
        moloch_session_add_tag(session, tagname);
        tag += strlen(tag) + 1;
    }

    return CALLBACK_CONTINUE;
}
/******************************************************************************/
void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yRules, (uint8_t *)data, len, yFlags, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void  moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yEmailRules, (uint8_t *)data, len, yFlags, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    if (yRules)
        yr_rules_destroy(yRules);
    if (yEmailRules)
        yr_rules_destroy(yEmailRules);

    if (yCompiler)
        yr_compiler_destroy(yCompiler);
    if (yEmailCompiler)
        yr_compiler_destroy(yEmailCompiler);
    yr_finalize();
}


#elif YR_MAJOR_VERSION == 3 && YR_MINOR_VERSION >= 4
// Yara 3
LOCAL  YR_COMPILER *yCompiler = 0;
LOCAL  YR_COMPILER *yEmailCompiler = 0;
LOCAL  YR_RULES    *yRules = 0;
LOCAL  YR_RULES    *yEmailRules = 0;
LOCAL  int         yFlags = 0;

/******************************************************************************/
void moloch_yara_report_error(int error_level, const char* file_name, int line_number, const char* error_message, void* UNUSED(user_data))
{
    LOG("%d %s:%d: %s\n", error_level, file_name, line_number, error_message);
}
/******************************************************************************/
void moloch_yara_open(char *filename, YR_COMPILER **compiler, YR_RULES **rules)
{
    yr_compiler_create(compiler);
    (*compiler)->callback = moloch_yara_report_error;

    if (filename) {
        FILE *rule_file;

        rule_file = fopen(filename, "r");

        if (rule_file != NULL) {
            int errors = yr_compiler_add_file(*compiler, rule_file, NULL, filename);

            fclose(rule_file);

            if (errors) {
                exit (0);
            }
            yr_compiler_get_rules(*compiler, rules);
        } else {
            CONFIGEXIT("yara could not open file: %s\n", filename);
        }
    }
}
/******************************************************************************/
void moloch_yara_load(char *name)
{
    YR_COMPILER *compiler;
    YR_RULES *rules;

    if (!name)
        return;

    moloch_yara_open(name, &compiler, &rules);

    if (yRules)
        moloch_free_later(yRules, (GDestroyNotify) yr_rules_destroy);
    if (yCompiler)
        moloch_free_later(yCompiler, (GDestroyNotify) yr_compiler_destroy);

    yCompiler = compiler;
    yRules = rules;
}
/******************************************************************************/
void moloch_yara_load_email(char *name)
{
    YR_COMPILER *compiler;
    YR_RULES *rules;

    if (!name)
        return;

    moloch_yara_open(name, &compiler, &rules);

    if (yEmailRules)
        moloch_free_later(yEmailRules, (GDestroyNotify) yr_rules_destroy);
    if (yEmailCompiler)
        moloch_free_later(yEmailCompiler, (GDestroyNotify) yr_compiler_destroy);

    yEmailCompiler = compiler;
    yEmailRules = rules;
}
/******************************************************************************/
void moloch_yara_init()
{
    if (moloch_config_boolean(NULL, "yaraFastMode", TRUE))
        yFlags |= SCAN_FLAGS_FAST_MODE;

    yr_initialize();

    if (config.yara)
        moloch_config_monitor_file("yara file", config.yara, moloch_yara_load);

    if (config.emailYara)
        moloch_config_monitor_file("yara email file", config.emailYara, moloch_yara_load_email);
}

/******************************************************************************/
int moloch_yara_callback(int message, YR_RULE* rule, MolochSession_t* session)
{
    if (message != CALLBACK_MSG_RULE_MATCHING)
        return CALLBACK_CONTINUE;

    char tagname[256];
    const char* tag;

    snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier);
    moloch_session_add_tag(session, tagname);
    tag = rule->tags;
    while(tag != NULL && *tag) {
        snprintf(tagname, sizeof(tagname), "yara:%s", tag);
        moloch_session_add_tag(session, tagname);
        tag += strlen(tag) + 1;
    }

    return CALLBACK_CONTINUE;
}
/******************************************************************************/
void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yRules, (uint8_t *)data, len, yFlags, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void  moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yEmailRules, (uint8_t *)data, len, yFlags, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    if (yRules)
        yr_rules_destroy(yRules);
    if (yEmailRules)
        yr_rules_destroy(yEmailRules);

    if (yCompiler)
        yr_compiler_destroy(yCompiler);
    if (yEmailCompiler)
        yr_compiler_destroy(yEmailCompiler);
    yr_finalize();
}
#elif defined(YR_COMPILER_H)
// Yara 3
LOCAL  YR_COMPILER *yCompiler = 0;
LOCAL  YR_COMPILER *yEmailCompiler = 0;
LOCAL  YR_RULES *yRules = 0;
LOCAL  YR_RULES *yEmailRules = 0;


/******************************************************************************/
void moloch_yara_report_error(int error_level, const char* file_name, int line_number, const char* error_message)
{
    LOG("%d %s:%d: %s\n", error_level, file_name, line_number, error_message);
}
/******************************************************************************/
void moloch_yara_open(char *filename, YR_COMPILER **compiler, YR_RULES **rules)
{
    yr_compiler_create(compiler);
    (*compiler)->callback = moloch_yara_report_error;

    if (filename) {
        FILE *rule_file;

        rule_file = fopen(filename, "r");

        if (rule_file != NULL) {
            int errors = yr_compiler_add_file(*compiler, rule_file, NULL, filename);

            fclose(rule_file);

            if (errors) {
                exit (0);
            }
            yr_compiler_get_rules(*compiler, rules);
        } else {
            CONFIGEXIT("yara could not open file: %s\n", filename);
        }
    }
}
/******************************************************************************/
void moloch_yara_init()
{
    yr_initialize();

    moloch_yara_open(config.yara, &yCompiler, &yRules);
    moloch_yara_open(config.emailYara, &yEmailCompiler, &yEmailRules);
}

/******************************************************************************/
int moloch_yara_callback(int message, YR_RULE* rule, MolochSession_t* session)
{
    if (message != CALLBACK_MSG_RULE_MATCHING)
        return CALLBACK_CONTINUE;

    char tagname[256];
    const char* tag;

    snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier);
    moloch_session_add_tag(session, tagname);
    tag = rule->tags;
    while(tag != NULL && *tag) {
        snprintf(tagname, sizeof(tagname), "yara:%s", tag);
        moloch_session_add_tag(session, tagname);
        tag += strlen(tag) + 1;
    }

    return CALLBACK_CONTINUE;
}
/******************************************************************************/
void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yRules, (uint8_t *)data, len, 0, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void  moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yEmailRules, (uint8_t *)data, len, 0, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    if (yRules)
        yr_rules_destroy(yRules);
    if (yEmailRules)
        yr_rules_destroy(yEmailRules);

    if (yCompiler)
        yr_compiler_destroy(yCompiler);
    if (yEmailCompiler)
        yr_compiler_destroy(yEmailCompiler);
    yr_finalize();
}
#elif defined(STRING_IS_HEX)
// Yara 2.x
LOCAL  YR_COMPILER *yCompiler = 0;
LOCAL  YR_COMPILER *yEmailCompiler = 0;
LOCAL  YR_RULES *yRules = 0;
LOCAL  YR_RULES *yEmailRules = 0;


/******************************************************************************/
void moloch_yara_report_error(int error_level, const char* file_name, int line_number, const char* error_message)
{
    LOG("%d %s:%d: %s\n", error_level, file_name, line_number, error_message);
}
/******************************************************************************/
void moloch_yara_open(char *filename, YR_COMPILER **compiler, YR_RULES **rules)
{
    yr_compiler_create(compiler);
    (*compiler)->error_report_function = moloch_yara_report_error;

    if (filename) {
        FILE *rule_file;

        rule_file = fopen(filename, "r");

        if (rule_file != NULL) {
            int errors = yr_compiler_add_file(*compiler, rule_file, NULL);

            fclose(rule_file);

            if (errors) {
                exit (0);
            }
            yr_compiler_get_rules(*compiler, rules);
        } else {
            CONFIGEXIT("yara could not open file: %s\n", filename);
        }
    }
}
/******************************************************************************/
void moloch_yara_init()
{
    yr_initialize();

    moloch_yara_open(config.yara, &yCompiler, &yRules);
    moloch_yara_open(config.emailYara, &yEmailCompiler, &yEmailRules);
}

/******************************************************************************/
int moloch_yara_callback(int message, YR_RULE* rule, MolochSession_t* session)
{
    if (message == CALLBACK_MSG_RULE_MATCHING)
        return CALLBACK_CONTINUE;

    char tagname[256];
    char* tag;

    snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier);
    moloch_session_add_tag(session, tagname);
    tag = rule->tags;
    while(tag != NULL && *tag) {
        snprintf(tagname, sizeof(tagname), "yara:%s", tag);
        moloch_session_add_tag(session, tagname);
        tag += strlen(tag) + 1;
    }

    return CALLBACK_CONTINUE;
}
/******************************************************************************/
void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yRules, (uint8_t *)data, len, (YR_CALLBACK_FUNC)moloch_yara_callback, session, FALSE, 0);
    return;
}
/******************************************************************************/
void  moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yEmailRules, (uint8_t *)data, len, (YR_CALLBACK_FUNC)moloch_yara_callback, session, FALSE, 0);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    if (yRules)
        yr_rules_destroy(yRules);
    if (yEmailRules)
        yr_rules_destroy(yEmailRules);

    if (yCompiler)
        yr_compiler_destroy(yCompiler);
    if (yEmailCompiler)
        yr_compiler_destroy(yEmailCompiler);
    yr_finalize();
}
#else
#error "Yara 1.x not supported"
#endif
