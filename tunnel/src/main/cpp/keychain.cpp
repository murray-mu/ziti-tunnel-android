/*
 * Copyright (c) 2024 NetFoundry. All rights reserved.
 */

#include <jni.h>
#include <string>
#include <functional>

#include <tlsuv/keychain.h>
#include <tlsuv/tls_engine.h>

int android_gen_key(keychain_key_t *pk, enum keychain_key_type type, const char *name);

int android_load_key(keychain_key_t *, const char *name);

int android_rem_key(const char *name);

enum keychain_key_type android_key_type(keychain_key_t k);

int android_key_public(keychain_key_t k, char *buf, size_t *len);

int android_key_sign(keychain_key_t k, const uint8_t *data, size_t datalen,
                     uint8_t *sig, size_t *siglen, int p);

void android_free_key(keychain_key_t k);

struct android_keychain_s {
    keychain_t api;
    jobject store; // AndroidKeyStore
    JavaVM *vm;
};

static android_keychain_s android_keychain{
        .api {
                .gen_key = android_gen_key,
                .load_key = android_load_key,
                .rem_key = android_rem_key,
                .key_type = android_key_type,
                .key_public = android_key_public,
                .key_sign = android_key_sign,
                .free_key = android_free_key,
        }
};
static struct {
    jmethodID loadKey;
    jmethodID keyType;
    jmethodID keyPub;
    jmethodID sign;
    jmethodID genKey;
    jmethodID delKey;
} methods;

static bool checkException(JNIEnv *env) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_openziti_tunnel_Keychain_registerKeychain(JNIEnv *env, jclass clazz, jobject chain) {
    android_keychain.store = env->NewGlobalRef(chain);
    tlsuv_set_keychain(&android_keychain.api);
    env->GetJavaVM(&android_keychain.vm);
    methods.loadKey = env->GetMethodID(clazz, "loadKey",
                                       "(Ljava/lang/String;)Ljava/security/KeyStore$PrivateKeyEntry;");
    methods.keyType = env->GetMethodID(clazz, "keyType",
                                       "(Ljava/security/KeyStore$PrivateKeyEntry;)I");
    methods.keyPub = env->GetMethodID(clazz, "pubKey",
                                      "(Ljava/security/KeyStore$PrivateKeyEntry;)[B");
    methods.sign = env->GetMethodID(clazz, "sign",
                                    "(Ljava/security/KeyStore$PrivateKeyEntry;Ljava/nio/ByteBuffer;)[B");
    methods.genKey = env->GetMethodID(
            clazz, "genKey", "(Ljava/lang/String;Ljava/lang/String;)Ljava/security/KeyStore$PrivateKeyEntry;");
    methods.delKey = env->GetMethodID(clazz, "deleteKey", "(Ljava/lang/String;)V");
}

int android_gen_key(keychain_key_t *pk, enum keychain_key_type type, const char *name) {
    if (android_keychain.store == nullptr) return -1;

    JNIEnv *env;
    android_keychain.vm->GetEnv((void **) &env, JNI_VERSION_1_6);

    jstring t;
    if (type == keychain_key_rsa) {
        t = env->NewStringUTF("RSA");
    } else if (type == keychain_key_ec) {
        t = env->NewStringUTF("EC");
    } else {
        return -1;
    }

    jstring n = env->NewStringUTF(name);
    jobject key = env->CallObjectMethod(android_keychain.store, methods.genKey, n, t);
    if (checkException(env)) {
        return -1;
    }
    env->DeleteLocalRef(n);
    env->DeleteLocalRef(t);

    if (key != nullptr) {
        *pk = env->NewGlobalRef(key);
        return 0;
    }

    return -1;
}

int android_load_key(keychain_key_t *k, const char *name) {
    if (android_keychain.store == nullptr) return -1;

    JNIEnv *env;
    android_keychain.vm->GetEnv((void **) &env, JNI_VERSION_1_6);

    jobject key = env->CallObjectMethod(android_keychain.store, methods.loadKey,
                                        env->NewStringUTF(name));

    if (checkException(env) || key == nullptr) {
        return -1;
    }

    *k = env->NewGlobalRef(key);
    return 0;
}

int android_rem_key(const char *name) {
    if (android_keychain.store == nullptr) {
        return -1;
    }

    JNIEnv *env;
    android_keychain.vm->GetEnv((void **) &env, JNI_VERSION_1_6);

    jstring n = env->NewStringUTF(name);

    env->CallObjectMethod(android_keychain.store, methods.delKey, n);
    if (checkException(env)) {
        return -1;
    }

    env->DeleteLocalRef(n);
    return 0;
}

enum keychain_key_type android_key_type(keychain_key_t k) {
    if (k == nullptr) {
        return keychain_key_invalid;
    }
    auto key = (jobject) k;

    JNIEnv *env;
    android_keychain.vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    auto type = (keychain_key_type) env->CallIntMethod(android_keychain.store, methods.keyType,
                                                       key);
    if (checkException(env)) {
        return keychain_key_invalid;
    }
    return type;
}

int android_key_public(keychain_key_t k, char *buf, size_t *len) {
    if (k == nullptr) {
        return -1;
    }
    auto key = (jobject) k;
    JNIEnv *env;
    android_keychain.vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    auto b = (jbyteArray) env->CallObjectMethod(android_keychain.store, methods.keyPub, key);

    if (checkException(env)) {
        return -1;
    }

    auto size = env->GetArrayLength(b);
    if (size > *len) {
        return -1;
    }
    auto bytes = env->GetByteArrayElements(b, nullptr);
    memcpy(buf, bytes, size);
    *len = size;
    env->ReleaseByteArrayElements(b, bytes, JNI_ABORT);

    return 0;
}

int android_key_sign(keychain_key_t k, const uint8_t *data, size_t datalen,
                     uint8_t *sig, size_t *siglen, int p) {
    if (k == nullptr) {
        return -1;
    }

    auto key = (jobject) k;
    JNIEnv *env;
    android_keychain.vm->GetEnv((void **) &env, JNI_VERSION_1_6);

    auto d = env->NewDirectByteBuffer((void *) data, datalen);
    auto s = (jbyteArray) env->CallObjectMethod(android_keychain.store, methods.sign, key, d);
    if (checkException(env) || s == nullptr) {
        env->DeleteLocalRef(d);
        return -1;
    }

    jsize size = env->GetArrayLength(s);
    *siglen = size;
    auto bytes = env->GetByteArrayElements(s, nullptr);
    memcpy(sig, bytes, size);
    env->ReleaseByteArrayElements(s, bytes, JNI_ABORT);
    return 0;
}

void android_free_key(keychain_key_t k) {
    if (k == nullptr) {
        return;
    }
    auto key = (jobject) k;
    JNIEnv *env;
    android_keychain.vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    env->DeleteGlobalRef(key);
    checkException(env);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_org_openziti_tunnel_Keychain_testNativeKey(JNIEnv *env, jclass, jstring name) {
    auto tls = default_tls_context(nullptr, 0);
    std::unique_ptr<tls_context, std::function<void(tls_context * )>> p(
            tls, [](tls_context *t) { t->free_ctx(t); }
    );

    tlsuv_private_key_t key;
    jboolean name_copy;
    int rc = tls->load_keychain_key(&key, env->GetStringUTFChars(name, &name_copy));
    if (rc == 0) {
        auto pub = key->pubkey(key);
    }

    const char *msg = "this is a message";
    char sig[512];
    size_t siglen = sizeof(sig);
    key->sign(key, hash_SHA256, msg, strlen(msg), sig, &siglen);

    return true;
}