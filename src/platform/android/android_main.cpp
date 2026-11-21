#include "app/Application.h"
#include "platform/Platform.h"

#include <backends/imgui_impl_android.h>
#include <imgui.h>

#include <android/asset_manager.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

android_app* g_app = nullptr;
ab::Application* g_application = nullptr;
bool g_initialized = false;

int showSoftKeyboardInput() {
    JavaVM* javaVm = g_app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    const jint envResult =
        javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (envResult == JNI_EDETACHED) {
        if (javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return -1;
        }
        attached = true;
    } else if (envResult != JNI_OK) {
        return -1;
    }
    jclass clazz = env->GetObjectClass(g_app->activity->clazz);
    if (!clazz) {
        return -2;
    }
    jmethodID method = env->GetMethodID(clazz, "showSoftInput", "()V");
    if (!method) {
        return -3;
    }
    env->CallVoidMethod(g_app->activity->clazz, method);
    if (attached) {
        javaVm->DetachCurrentThread();
    }
    return 0;
}

int hideSoftKeyboardInput() {
    JavaVM* javaVm = g_app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    const jint envResult =
        javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (envResult == JNI_EDETACHED) {
        if (javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return -1;
        }
        attached = true;
    } else if (envResult != JNI_OK) {
        return -1;
    }
    jclass clazz = env->GetObjectClass(g_app->activity->clazz);
    if (!clazz) {
        return -2;
    }
    jmethodID method = env->GetMethodID(clazz, "hideSoftInput", "()V");
    if (!method) {
        return -3;
    }
    env->CallVoidMethod(g_app->activity->clazz, method);
    if (attached) {
        javaVm->DetachCurrentThread();
    }
    return 0;
}

int pollUnicodeChars() {
    JavaVM* javaVm = g_app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    const jint envResult =
        javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (envResult == JNI_EDETACHED) {
        if (javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return -1;
        }
        attached = true;
    } else if (envResult != JNI_OK) {
        return -1;
    }
    jclass clazz = env->GetObjectClass(g_app->activity->clazz);
    if (!clazz) {
        return -2;
    }
    jmethodID method = env->GetMethodID(clazz, "pollUnicodeChar", "()I");
    if (!method) {
        return -3;
    }
    ImGuiIO& io = ImGui::GetIO();
    jint character = 0;
    while ((character = env->CallIntMethod(g_app->activity->clazz, method)) !=
           0) {
        io.AddInputCharacter(static_cast<unsigned int>(character));
    }
    if (attached) {
        javaVm->DetachCurrentThread();
    }
    return 0;
}

namespace fs = std::filesystem;

void copyAsset(AAssetManager* manager, const char* assetName,
               const fs::path& destination) {
    if (fs::exists(destination)) {
        return;
    }
    AAsset* asset =
        AAssetManager_open(manager, assetName, AASSET_MODE_BUFFER);
    if (!asset) {
        return;
    }
    std::ofstream out(destination, std::ios::binary | std::ios::trunc);
    if (out) {
        char buffer[4096];
        int count = 0;
        while ((count = AAsset_read(asset, buffer, sizeof(buffer))) > 0) {
            out.write(buffer, count);
        }
    }
    AAsset_close(asset);
}

void prepareBundledFonts(android_app* app) {
    const fs::path dir = fs::path(app->activity->internalDataPath) / "fonts";
    std::error_code ec;
    fs::create_directories(dir, ec);
    AAssetManager* manager = app->activity->assetManager;
    static const char* kFonts[] = {
        "LiberationSans-Regular.ttf", "LiberationSans-Bold.ttf",
        "LiberationSans-Italic.ttf", "LiberationSans-BoldItalic.ttf",
        "LiberationMono-Regular.ttf"};
    for (const char* name : kFonts) {
        const std::string asset = std::string("fonts/") + name;
        copyAsset(manager, asset.c_str(), dir / name);
    }
    ab::platform::setBundledFontDir(ab::platform::pathToUtf8(dir));
}

void handleAppCmd(android_app* app, int32_t command) {
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window && g_application && !g_initialized) {
                ab::platform::setAndroidDataDir(
                    app->activity->internalDataPath);
                prepareBundledFonts(app);
                g_initialized =
                    g_application->initAndroid(app->window);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            if (g_initialized && g_application) {
                g_application->shutdownAndroid();
                g_initialized = false;
            }
            break;
        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS:
        case APP_CMD_SAVE_STATE:
        default:
            break;
    }
}

int32_t handleInputEvent(android_app*, AInputEvent* event) {
    return ImGui_ImplAndroid_HandleInputEvent(event);
}

}  // namespace

void android_main(android_app* app) {
    app->onAppCmd = handleAppCmd;
    app->onInputEvent = handleInputEvent;
    g_app = app;

    ab::Application application;
    g_application = &application;

    bool wantTextInputLast = false;
    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;
        while (ALooper_pollOnce(g_initialized ? 0 : -1, nullptr, &events,
                                reinterpret_cast<void**>(&source)) >= 0) {
            if (source) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) {
                if (g_initialized) {
                    application.shutdownAndroid();
                    g_initialized = false;
                }
                g_application = nullptr;
                g_app = nullptr;
                return;
            }
        }

        if (!g_initialized) {
            continue;
        }

        pollUnicodeChars();
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput && !wantTextInputLast) {
            showSoftKeyboardInput();
        } else if (!io.WantTextInput && wantTextInputLast) {
            hideSoftKeyboardInput();
        }
        wantTextInputLast = io.WantTextInput;

        application.frameAndroid();
    }
}
