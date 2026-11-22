#include "platform/android/AndroidStorage.h"

#include "core/Storage.h"
#include "platform/Platform.h"
#include "platform/desktop/DesktopStorage.h"

#include <jni.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ab {
namespace platform {

namespace {

namespace fs = std::filesystem;

ANativeActivity* g_activity = nullptr;

struct JniScope {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;

    JniScope() {
        if (!g_activity || !g_activity->vm) {
            return;
        }
        vm = g_activity->vm;
        const jint result =
            vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (result == JNI_EDETACHED) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            } else {
                env = nullptr;
            }
        } else if (result != JNI_OK) {
            env = nullptr;
        }
    }

    ~JniScope() {
        if (attached && vm) {
            vm->DetachCurrentThread();
        }
    }

    explicit operator bool() const {
        return env != nullptr && g_activity && g_activity->clazz;
    }
};

void clearException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

jmethodID staticMethod(JNIEnv* env, const char* name, const char* signature,
                       jclass* outClass) {
    *outClass = nullptr;
    if (!g_activity || !g_activity->clazz) {
        return nullptr;
    }
    jclass cls = env->GetObjectClass(g_activity->clazz);
    if (!cls) {
        return nullptr;
    }
    jmethodID method = env->GetStaticMethodID(cls, name, signature);
    if (!method) {
        clearException(env);
        env->DeleteLocalRef(cls);
        return nullptr;
    }
    *outClass = cls;
    return method;
}

std::string toStd(JNIEnv* env, jstring value) {
    if (!value) {
        return "";
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    std::string out = chars ? chars : "";
    if (chars) {
        env->ReleaseStringUTFChars(value, chars);
    }
    env->DeleteLocalRef(value);
    return out;
}

jstring toJava(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

std::string callString1(const char* name, const char* signature,
                        const std::string& a) {
    JniScope scope;
    if (!scope) {
        return "";
    }
    JNIEnv* env = scope.env;
    jclass cls = nullptr;
    jmethodID method = staticMethod(env, name, signature, &cls);
    if (!method) {
        return "";
    }
    jstring ja = toJava(env, a);
    jstring result =
        static_cast<jstring>(env->CallStaticObjectMethod(cls, method, ja));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = nullptr;
    }
    std::string out = toStd(env, result);
    env->DeleteLocalRef(ja);
    env->DeleteLocalRef(cls);
    return out;
}

std::string callString2(const char* name, const char* signature,
                        const std::string& a, const std::string& b) {
    JniScope scope;
    if (!scope) {
        return "";
    }
    JNIEnv* env = scope.env;
    jclass cls = nullptr;
    jmethodID method = staticMethod(env, name, signature, &cls);
    if (!method) {
        return "";
    }
    jstring ja = toJava(env, a);
    jstring jb = toJava(env, b);
    jstring result = static_cast<jstring>(
        env->CallStaticObjectMethod(cls, method, ja, jb));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = nullptr;
    }
    std::string out = toStd(env, result);
    env->DeleteLocalRef(ja);
    env->DeleteLocalRef(jb);
    env->DeleteLocalRef(cls);
    return out;
}

std::string callString3(const char* name, const char* signature,
                        const std::string& a, const std::string& b,
                        const std::string& c) {
    JniScope scope;
    if (!scope) {
        return "";
    }
    JNIEnv* env = scope.env;
    jclass cls = nullptr;
    jmethodID method = staticMethod(env, name, signature, &cls);
    if (!method) {
        return "";
    }
    jstring ja = toJava(env, a);
    jstring jb = toJava(env, b);
    jstring jc = toJava(env, c);
    jstring result = static_cast<jstring>(
        env->CallStaticObjectMethod(cls, method, ja, jb, jc));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = nullptr;
    }
    std::string out = toStd(env, result);
    env->DeleteLocalRef(ja);
    env->DeleteLocalRef(jb);
    env->DeleteLocalRef(jc);
    env->DeleteLocalRef(cls);
    return out;
}

std::string callString4(const char* name, const char* signature,
                        const std::string& a, const std::string& b,
                        const std::string& c, const std::string& d) {
    JniScope scope;
    if (!scope) {
        return "";
    }
    JNIEnv* env = scope.env;
    jclass cls = nullptr;
    jmethodID method = staticMethod(env, name, signature, &cls);
    if (!method) {
        return "";
    }
    jstring ja = toJava(env, a);
    jstring jb = toJava(env, b);
    jstring jc = toJava(env, c);
    jstring jd = toJava(env, d);
    jstring result = static_cast<jstring>(
        env->CallStaticObjectMethod(cls, method, ja, jb, jc, jd));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = nullptr;
    }
    std::string out = toStd(env, result);
    env->DeleteLocalRef(ja);
    env->DeleteLocalRef(jb);
    env->DeleteLocalRef(jc);
    env->DeleteLocalRef(jd);
    env->DeleteLocalRef(cls);
    return out;
}

bool callBool1(const char* name, const char* signature,
               const std::string& a) {
    JniScope scope;
    if (!scope) {
        return false;
    }
    JNIEnv* env = scope.env;
    jclass cls = nullptr;
    jmethodID method = staticMethod(env, name, signature, &cls);
    if (!method) {
        return false;
    }
    jstring ja = toJava(env, a);
    jboolean result = env->CallStaticBooleanMethod(cls, method, ja);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = JNI_FALSE;
    }
    env->DeleteLocalRef(ja);
    env->DeleteLocalRef(cls);
    return result == JNI_TRUE;
}

std::vector<std::string> callListChildren(const std::string& tree,
                                          const std::string& document) {
    std::vector<std::string> out;
    JniScope scope;
    if (!scope) {
        return out;
    }
    JNIEnv* env = scope.env;
    jclass cls = nullptr;
    jmethodID method = staticMethod(
        env, "listChildren",
        "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;", &cls);
    if (!method) {
        return out;
    }
    jstring jtree = toJava(env, tree);
    jstring jdocument = toJava(env, document);
    jobjectArray result = static_cast<jobjectArray>(
        env->CallStaticObjectMethod(cls, method, jtree, jdocument));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = nullptr;
    }
    if (result) {
        const jsize count = env->GetArrayLength(result);
        for (jsize i = 0; i < count; ++i) {
            jstring item =
                static_cast<jstring>(env->GetObjectArrayElement(result, i));
            out.push_back(toStd(env, item));
        }
        env->DeleteLocalRef(result);
    }
    env->DeleteLocalRef(jtree);
    env->DeleteLocalRef(jdocument);
    env->DeleteLocalRef(cls);
    return out;
}

std::vector<unsigned char> callReadBytes(const std::string& document) {
    std::vector<unsigned char> out;
    JniScope scope;
    if (!scope) {
        return out;
    }
    JNIEnv* env = scope.env;
    jclass cls = nullptr;
    jmethodID method = staticMethod(
        env, "readBytes", "(Ljava/lang/String;)[B", &cls);
    if (!method) {
        return out;
    }
    jstring jdocument = toJava(env, document);
    jbyteArray result = static_cast<jbyteArray>(
        env->CallStaticObjectMethod(cls, method, jdocument));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = nullptr;
    }
    if (result) {
        const jsize count = env->GetArrayLength(result);
        out.resize(static_cast<size_t>(count));
        if (count > 0) {
            env->GetByteArrayRegion(result, 0, count,
                                    reinterpret_cast<jbyte*>(out.data()));
        }
        env->DeleteLocalRef(result);
    }
    env->DeleteLocalRef(jdocument);
    env->DeleteLocalRef(cls);
    return out;
}

bool callWriteBytes(const std::string& document, const std::string& data) {
    JniScope scope;
    if (!scope) {
        return false;
    }
    JNIEnv* env = scope.env;
    jclass cls = nullptr;
    jmethodID method = staticMethod(
        env, "writeBytes", "(Ljava/lang/String;[B)Z", &cls);
    if (!method) {
        return false;
    }
    jstring jdocument = toJava(env, document);
    jbyteArray bytes = env->NewByteArray(static_cast<jsize>(data.size()));
    if (bytes && !data.empty()) {
        env->SetByteArrayRegion(
            bytes, 0, static_cast<jsize>(data.size()),
            reinterpret_cast<const jbyte*>(data.data()));
    }
    jboolean result =
        env->CallStaticBooleanMethod(cls, method, jdocument, bytes);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = JNI_FALSE;
    }
    if (bytes) {
        env->DeleteLocalRef(bytes);
    }
    env->DeleteLocalRef(jdocument);
    env->DeleteLocalRef(cls);
    return result == JNI_TRUE;
}

std::string mimeForName(const std::string& name) {
    std::string extension = platform::pathToUtf8(
        platform::pathFromUtf8(name).extension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    if (extension == ".md" || extension == ".markdown" ||
        extension == ".txt") {
        return "text/plain";
    }
    return "application/octet-stream";
}

class AndroidSafStorage final : public Storage {
public:
    bool exists(const Path& location) const override {
        return !resolve(location).empty();
    }

    bool isDirectory(const Path& location) const override {
        const std::string document = resolve(location);
        return !document.empty() &&
               callBool1("isDirectory", "(Ljava/lang/String;)Z", document);
    }

    bool isRegularFile(const Path& location) const override {
        const std::string document = resolve(location);
        return !document.empty() &&
               !callBool1("isDirectory", "(Ljava/lang/String;)Z", document);
    }

    bool readText(const Path& location, std::string& out) const override {
        std::vector<unsigned char> bytes = callReadBytes(resolve(location));
        if (bytes.empty()) {
            return false;
        }
        out.assign(bytes.begin(), bytes.end());
        return true;
    }

    bool writeText(const Path& location,
                   const std::string& content) override {
        std::string tree;
        Path relative;
        if (!split(location, tree, relative) || relative.empty()) {
            return false;
        }
        const Path parentRelative = relative.parent_path();
        const std::string parentDocument =
            parentRelative.empty()
                ? rootDocumentUri(tree)
                : resolve(encode(tree, parentRelative));
        if (parentDocument.empty()) {
            return false;
        }
        const std::string name =
            platform::pathToUtf8(relative.filename());
        std::string document = callString3(
            "findChild",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)"
            "Ljava/lang/String;",
            tree, parentDocument, name);
        if (document.empty()) {
            document = callString4(
                "createFile",
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
                "Ljava/lang/String;)Ljava/lang/String;",
                tree, parentDocument, name, mimeForName(name));
        }
        if (document.empty()) {
            return false;
        }
        const bool ok = callWriteBytes(document, content);
        if (ok) {
            m_cache[platform::pathToUtf8(location)] = document;
        }
        return ok;
    }

    bool readBytes(const Path& location,
                   std::vector<unsigned char>& out) const override {
        out = callReadBytes(resolve(location));
        return !out.empty();
    }

    bool rename(const Path& from, const std::string& newName) override {
        const std::string document = resolve(from);
        if (document.empty() || newName.empty()) {
            return false;
        }
        const std::string renamed = callString2(
            "renameDocument",
            "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
            document, newName);
        m_cache.clear();
        return !renamed.empty();
    }

    bool remove(const Path& location) override {
        const std::string document = resolve(location);
        if (document.empty()) {
            return false;
        }
        const bool removed =
            callBool1("deleteDocument", "(Ljava/lang/String;)Z", document);
        m_cache.clear();
        return removed;
    }

    Path join(const Path& base, const Path& child) const override {
        std::string tree;
        Path relative;
        if (!split(base, tree, relative)) {
            return {};
        }
        const Path next = relative.empty() ? child : relative / child;
        return encode(tree, next);
    }

    Path parent(const Path& location) const override {
        std::string tree;
        Path relative;
        if (!split(location, tree, relative) || relative.empty()) {
            return location;
        }
        return encode(tree, relative.parent_path());
    }

    bool scanRecursive(const Path& root,
                       std::vector<Entry>& out) const override {
        std::string tree;
        Path relative;
        if (!split(root, tree, relative)) {
            return false;
        }
        m_cache.clear();
        const std::string document = resolve(root);
        if (document.empty()) {
            return false;
        }
        out.clear();
        scanDirectory(tree, document, relative, out);
        return true;
    }

    bool scanFlat(const Path& folder, std::vector<Item>& out) const override {
        std::string tree;
        Path relative;
        if (!split(folder, tree, relative)) {
            return false;
        }
        m_cache.clear();
        const std::string document = resolve(folder);
        if (document.empty()) {
            return false;
        }
        out.clear();
        for (const std::string& child : callListChildren(tree, document)) {
            const std::string name = callString1(
                "displayName",
                "(Ljava/lang/String;)Ljava/lang/String;", child);
            if (name.empty()) {
                continue;
            }
            const std::string mime = callString1(
                "mimeType", "(Ljava/lang/String;)Ljava/lang/String;", child);
            if (mime == "vnd.android.document/directory") {
                continue;
            }
            const Path location =
                encode(tree, relative / platform::pathFromUtf8(name));
            out.push_back({location, name});
            m_cache[platform::pathToUtf8(location)] = child;
        }
        return true;
    }

private:
    static bool split(const Path& location, std::string& tree, Path& relative) {
        const std::string text = platform::pathToUtf8(location);
        const size_t separator = text.find('\n');
        if (separator == std::string::npos) {
            tree = text;
            relative.clear();
        } else {
            tree = text.substr(0, separator);
            relative = platform::pathFromUtf8(text.substr(separator + 1));
        }
        return !tree.empty();
    }

    static Path encode(const std::string& tree, const Path& relative) {
        return platform::pathFromUtf8(
            tree + "\n" + platform::pathToUtf8(relative));
    }

    std::string rootDocumentUri(const std::string& tree) const {
        return callString1(
            "rootDocumentUri",
            "(Ljava/lang/String;)Ljava/lang/String;", tree);
    }

    std::string resolve(const Path& location) const {
        std::string tree;
        Path relative;
        if (!split(location, tree, relative)) {
            return "";
        }
        const std::string key = platform::pathToUtf8(location);
        auto cached = m_cache.find(key);
        if (cached != m_cache.end()) {
            return cached->second;
        }
        std::string document = rootDocumentUri(tree);
        if (document.empty()) {
            return "";
        }
        for (const Path& part : relative) {
            const std::string name = platform::pathToUtf8(part);
            if (name.empty()) {
                continue;
            }
            const std::string child = callString3(
                "findChild",
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)"
                "Ljava/lang/String;",
                tree, document, name);
            if (child.empty()) {
                return "";
            }
            document = child;
        }
        m_cache[key] = document;
        return document;
    }

    void scanDirectory(const std::string& tree, const std::string& document,
                       const Path& relative,
                       std::vector<Entry>& out) const {
        for (const std::string& child : callListChildren(tree, document)) {
            const std::string name = callString1(
                "displayName",
                "(Ljava/lang/String;)Ljava/lang/String;", child);
            if (name.empty()) {
                continue;
            }
            const std::string mime = callString1(
                "mimeType", "(Ljava/lang/String;)Ljava/lang/String;", child);
            const bool isDir = mime == "vnd.android.document/directory";
            const Path childRelative =
                relative / platform::pathFromUtf8(name);
            out.push_back({childRelative, isDir});
            m_cache[platform::pathToUtf8(encode(tree, childRelative))] =
                child;
            if (isDir) {
                scanDirectory(tree, child, childRelative, out);
            }
        }
    }

    mutable std::unordered_map<std::string, std::string> m_cache;
};

class AndroidRoutingStorage final : public Storage {
public:
    bool exists(const Path& location) const override {
        return isSaf(location) ? m_saf.exists(location)
                               : m_desktop.exists(location);
    }

    bool isDirectory(const Path& location) const override {
        return isSaf(location) ? m_saf.isDirectory(location)
                               : m_desktop.isDirectory(location);
    }

    bool isRegularFile(const Path& location) const override {
        return isSaf(location) ? m_saf.isRegularFile(location)
                               : m_desktop.isRegularFile(location);
    }

    bool readText(const Path& location, std::string& out) const override {
        return isSaf(location) ? m_saf.readText(location, out)
                               : m_desktop.readText(location, out);
    }

    bool writeText(const Path& location,
                   const std::string& content) override {
        return isSaf(location) ? m_saf.writeText(location, content)
                               : m_desktop.writeText(location, content);
    }

    bool readBytes(const Path& location,
                   std::vector<unsigned char>& out) const override {
        return isSaf(location) ? m_saf.readBytes(location, out)
                               : m_desktop.readBytes(location, out);
    }

    bool rename(const Path& from, const std::string& newName) override {
        return isSaf(from) ? m_saf.rename(from, newName)
                           : m_desktop.rename(from, newName);
    }

    bool remove(const Path& location) override {
        return isSaf(location) ? m_saf.remove(location)
                               : m_desktop.remove(location);
    }

    Path join(const Path& base, const Path& child) const override {
        return isSaf(base) ? m_saf.join(base, child)
                           : m_desktop.join(base, child);
    }

    Path parent(const Path& location) const override {
        return isSaf(location) ? m_saf.parent(location)
                               : m_desktop.parent(location);
    }

    bool scanRecursive(const Path& root,
                       std::vector<Entry>& out) const override {
        return isSaf(root) ? m_saf.scanRecursive(root, out)
                           : m_desktop.scanRecursive(root, out);
    }

    bool scanFlat(const Path& folder, std::vector<Item>& out) const override {
        return isSaf(folder) ? m_saf.scanFlat(folder, out)
                             : m_desktop.scanFlat(folder, out);
    }

private:
    static bool isSaf(const Path& location) {
        const std::string text = platform::pathToUtf8(location);
        return text.rfind("content://", 0) == 0;
    }

    DesktopStorage m_desktop;
    AndroidSafStorage m_saf;
};

}  // namespace

void setAndroidActivity(ANativeActivity* activity) {
    g_activity = activity;
}

std::unique_ptr<Storage> makeAndroidStorage() {
    return std::make_unique<AndroidRoutingStorage>();
}

void androidRequestFolderPick(int kind) {
    JniScope scope;
    if (!scope) {
        return;
    }
    JNIEnv* env = scope.env;
    jclass cls = env->GetObjectClass(g_activity->clazz);
    if (!cls) {
        return;
    }
    jmethodID method = env->GetMethodID(cls, "pickFolder", "(I)V");
    if (method) {
        env->CallVoidMethod(g_activity->clazz, method,
                            static_cast<jint>(kind));
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(cls);
}

bool androidPollFolderPick(std::string& uri, int& kind) {
    JniScope scope;
    if (!scope) {
        return false;
    }
    JNIEnv* env = scope.env;
    jclass cls = env->GetObjectClass(g_activity->clazz);
    if (!cls) {
        return false;
    }
    jmethodID method =
        env->GetMethodID(cls, "takePickedFolder", "()Ljava/lang/String;");
    if (!method) {
        clearException(env);
        env->DeleteLocalRef(cls);
        return false;
    }
    jstring result = static_cast<jstring>(
        env->CallObjectMethod(g_activity->clazz, method));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = nullptr;
    }
    const std::string text = toStd(env, result);
    env->DeleteLocalRef(cls);
    const size_t separator = text.find('\n');
    if (separator == std::string::npos) {
        return false;
    }
    kind = std::atoi(text.substr(0, separator).c_str());
    uri = text.substr(separator + 1);
    return !uri.empty();
}

}  // namespace platform
}  // namespace ab
