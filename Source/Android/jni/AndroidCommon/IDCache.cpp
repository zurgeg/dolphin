// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/AndroidCommon/IDCache.h"

#include <jni.h>

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

static JavaVM* s_java_vm;

static jclass s_native_library_class;
static jmethodID s_display_alert_msg;
static jmethodID s_do_rumble;
static jmethodID s_update_touch_pointer;
static jmethodID s_on_title_changed;
static jmethodID s_finish_emulation_activity;

static jclass s_game_file_class;
static jfieldID s_game_file_pointer;
static jmethodID s_game_file_constructor;

static jclass s_game_file_cache_class;
static jfieldID s_game_file_cache_pointer;

static jclass s_analytics_class;
static jmethodID s_send_analytics_report;
static jmethodID s_get_analytics_value;

static jclass s_linked_hash_map_class;
static jmethodID s_linked_hash_map_init;
static jmethodID s_linked_hash_map_put;

static jclass s_ini_file_class;
static jfieldID s_ini_file_pointer;
static jclass s_ini_file_section_class;
static jfieldID s_ini_file_section_pointer;
static jmethodID s_ini_file_section_constructor;

static jclass s_compress_cb_class;
static jmethodID s_compress_cb_run;

static jclass s_content_handler_class;
static jmethodID s_content_handler_open_fd;
static jmethodID s_content_handler_delete;

namespace IDCache
{
JNIEnv* GetEnvForThread()
{
  thread_local static struct OwnedEnv
  {
    OwnedEnv()
    {
      status = s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
      if (status == JNI_EDETACHED)
        s_java_vm->AttachCurrentThread(&env, nullptr);
    }

    ~OwnedEnv()
    {
      if (status == JNI_EDETACHED)
        s_java_vm->DetachCurrentThread();
    }

    int status;
    JNIEnv* env = nullptr;
  } owned;
  return owned.env;
}

jclass GetNativeLibraryClass()
{
  return s_native_library_class;
}

jmethodID GetDisplayAlertMsg()
{
  return s_display_alert_msg;
}

jmethodID GetDoRumble()
{
  return s_do_rumble;
}

jmethodID GetUpdateTouchPointer()
{
  return s_update_touch_pointer;
}

jmethodID GetOnTitleChanged()
{
  return s_on_title_changed;
}

jmethodID GetFinishEmulationActivity()
{
  return s_finish_emulation_activity;
}

jclass GetAnalyticsClass()
{
  return s_analytics_class;
}

jmethodID GetSendAnalyticsReport()
{
  return s_send_analytics_report;
}

jmethodID GetAnalyticsValue()
{
  return s_get_analytics_value;
}

jclass GetGameFileClass()
{
  return s_game_file_class;
}

jfieldID GetGameFilePointer()
{
  return s_game_file_pointer;
}

jmethodID GetGameFileConstructor()
{
  return s_game_file_constructor;
}

jclass GetGameFileCacheClass()
{
  return s_game_file_cache_class;
}

jfieldID GetGameFileCachePointer()
{
  return s_game_file_cache_pointer;
}

jclass GetLinkedHashMapClass()
{
  return s_linked_hash_map_class;
}

jmethodID GetLinkedHashMapInit()
{
  return s_linked_hash_map_init;
}

jmethodID GetLinkedHashMapPut()
{
  return s_linked_hash_map_put;
}

jclass GetIniFileClass()
{
  return s_ini_file_class;
}

jfieldID GetIniFilePointer()
{
  return s_ini_file_pointer;
}

jclass GetIniFileSectionClass()
{
  return s_ini_file_section_class;
}

jfieldID GetIniFileSectionPointer()
{
  return s_ini_file_section_pointer;
}

jmethodID GetIniFileSectionConstructor()
{
  return s_ini_file_section_constructor;
}

jclass GetCompressCallbackClass()
{
  return s_compress_cb_class;
}

jmethodID GetCompressCallbackRun()
{
  return s_compress_cb_run;
}

jclass GetContentHandlerClass()
{
  return s_content_handler_class;
}

jmethodID GetContentHandlerOpenFd()
{
  return s_content_handler_open_fd;
}

jmethodID GetContentHandlerDelete()
{
  return s_content_handler_delete;
}

}  // namespace IDCache

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
  s_java_vm = vm;

  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
    return JNI_ERR;

  const jclass native_library_class = env->FindClass("org/dolphinemu/dolphinemu/NativeLibrary");
  s_native_library_class = reinterpret_cast<jclass>(env->NewGlobalRef(native_library_class));
  s_display_alert_msg = env->GetStaticMethodID(s_native_library_class, "displayAlertMsg",
                                               "(Ljava/lang/String;Ljava/lang/String;ZZZ)Z");
  s_do_rumble = env->GetStaticMethodID(s_native_library_class, "rumble", "(ID)V");
  s_update_touch_pointer =
      env->GetStaticMethodID(s_native_library_class, "updateTouchPointer", "()V");
  s_on_title_changed = env->GetStaticMethodID(s_native_library_class, "onTitleChanged", "()V");
  s_finish_emulation_activity =
      env->GetStaticMethodID(s_native_library_class, "finishEmulationActivity", "()V");
  env->DeleteLocalRef(native_library_class);

  const jclass game_file_class = env->FindClass("org/dolphinemu/dolphinemu/model/GameFile");
  s_game_file_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_file_class));
  s_game_file_pointer = env->GetFieldID(game_file_class, "mPointer", "J");
  s_game_file_constructor = env->GetMethodID(game_file_class, "<init>", "(J)V");
  env->DeleteLocalRef(game_file_class);

  const jclass game_file_cache_class =
      env->FindClass("org/dolphinemu/dolphinemu/model/GameFileCache");
  s_game_file_cache_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_file_cache_class));
  s_game_file_cache_pointer = env->GetFieldID(game_file_cache_class, "mPointer", "J");
  env->DeleteLocalRef(game_file_cache_class);

  const jclass analytics_class = env->FindClass("org/dolphinemu/dolphinemu/utils/Analytics");
  s_analytics_class = reinterpret_cast<jclass>(env->NewGlobalRef(analytics_class));
  s_send_analytics_report =
      env->GetStaticMethodID(s_analytics_class, "sendReport", "(Ljava/lang/String;[B)V");
  s_get_analytics_value = env->GetStaticMethodID(s_analytics_class, "getValue",
                                                 "(Ljava/lang/String;)Ljava/lang/String;");
  env->DeleteLocalRef(analytics_class);

  const jclass ini_file_class = env->FindClass("org/dolphinemu/dolphinemu/utils/IniFile");
  s_ini_file_class = reinterpret_cast<jclass>(env->NewGlobalRef(ini_file_class));
  s_ini_file_pointer = env->GetFieldID(ini_file_class, "mPointer", "J");
  env->DeleteLocalRef(ini_file_class);

  const jclass ini_file_section_class =
      env->FindClass("org/dolphinemu/dolphinemu/utils/IniFile$Section");
  s_ini_file_section_class = reinterpret_cast<jclass>(env->NewGlobalRef(ini_file_section_class));
  s_ini_file_section_pointer = env->GetFieldID(ini_file_section_class, "mPointer", "J");
  s_ini_file_section_constructor = env->GetMethodID(
      ini_file_section_class, "<init>", "(Lorg/dolphinemu/dolphinemu/utils/IniFile;J)V");
  env->DeleteLocalRef(ini_file_section_class);

  const jclass map_class = env->FindClass("java/util/LinkedHashMap");
  s_linked_hash_map_class = reinterpret_cast<jclass>(env->NewGlobalRef(map_class));
  s_linked_hash_map_init = env->GetMethodID(s_linked_hash_map_class, "<init>", "(I)V");
  s_linked_hash_map_put = env->GetMethodID(
      s_linked_hash_map_class, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

  const jclass compress_cb_class =
      env->FindClass("org/dolphinemu/dolphinemu/utils/CompressCallback");
  s_compress_cb_class = reinterpret_cast<jclass>(env->NewGlobalRef(compress_cb_class));
  s_compress_cb_run = env->GetMethodID(s_compress_cb_class, "run", "(Ljava/lang/String;F)Z");

  const jclass content_handler_class =
      env->FindClass("org/dolphinemu/dolphinemu/utils/ContentHandler");
  s_content_handler_class = reinterpret_cast<jclass>(env->NewGlobalRef(content_handler_class));
  s_content_handler_open_fd = env->GetStaticMethodID(s_content_handler_class, "openFd",
                                                     "(Ljava/lang/String;Ljava/lang/String;)I");
  s_content_handler_delete =
      env->GetStaticMethodID(s_content_handler_class, "delete", "(Ljava/lang/String;)Z");

  return JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved)
{
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
    return;

  env->DeleteGlobalRef(s_native_library_class);
  env->DeleteGlobalRef(s_game_file_class);
  env->DeleteGlobalRef(s_game_file_cache_class);
  env->DeleteGlobalRef(s_analytics_class);
  env->DeleteGlobalRef(s_linked_hash_map_class);
  env->DeleteGlobalRef(s_ini_file_class);
  env->DeleteGlobalRef(s_ini_file_section_class);
  env->DeleteGlobalRef(s_compress_cb_class);
  env->DeleteGlobalRef(s_content_handler_class);
}

#ifdef __cplusplus
}
#endif
