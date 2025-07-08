#include <jni.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <android/bitmap.h>
#include <android/log.h>

using namespace cv;

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "NativeLib", __VA_ARGS__))

extern "C" JNIEXPORT jobject JNICALL
Java_ups_edu_aplicacionnativa_MainActivity_processAndReturnBitmap(JNIEnv* env, jobject /* this */, jobject bitmap) {
    AndroidBitmapInfo info;
    void* pixels = nullptr;

    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
        LOGI("Error al obtener info del bitmap");
        return nullptr;
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGI("Formato de bitmap no soportado");
        return nullptr;
    }

    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        LOGI("Error al bloquear pixeles");
        return nullptr;
    }

    Mat img(info.height, info.width, CV_8UC4, pixels);

    // 1. Convertir a gris
    Mat gray;
    cvtColor(img, gray, COLOR_RGBA2GRAY);

    // 2. Gaussian blur 5x5
    Mat blurred;
    GaussianBlur(gray, blurred, Size(5, 5), 0);

    // 3. Threshold inverso con Otsu
    Mat binary;
    threshold(blurred, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);

    // 4. Morph close elíptico 7x7, 2 iteraciones
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(7, 7));
    Mat closed;
    morphologyEx(binary, closed, MORPH_CLOSE, kernel, Point(-1, -1), 2);

    // 5. Encontrar contornos externos
    std::vector<std::vector<Point>> contours;
    std::vector<Vec4i> hierarchy;
    findContours(closed, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        LOGI("No se detectaron contornos");
        AndroidBitmap_unlockPixels(env, bitmap);
        return nullptr;
    }

    // 6. Seleccionar contorno más grande
    int largestContourIdx = 0;
    double maxArea = 0;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            largestContourIdx = static_cast<int>(i);
        }
    }

    // 7. Crear imagen rellena solo con contorno más grande
    Mat filledImage = Mat::zeros(closed.size(), CV_8UC1);
    drawContours(filledImage, contours, largestContourIdx, Scalar(255), FILLED);

    AndroidBitmap_unlockPixels(env, bitmap);

    // 8. Crear Bitmap Android ARGB_8888 para devolver la imagen procesada
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethod = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                          "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
    jmethodID valueOfMethod = env->GetStaticMethodID(bitmapConfigClass, "valueOf",
                                                     "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");
    jobject argb8888 = env->CallStaticObjectMethod(bitmapConfigClass, valueOfMethod,
                                                   env->NewStringUTF("ARGB_8888"));

    jobject resultBitmap = env->CallStaticObjectMethod(bitmapClass, createBitmapMethod,
                                                       filledImage.cols, filledImage.rows, argb8888);

    void* resultPixels = nullptr;
    if (AndroidBitmap_lockPixels(env, resultBitmap, &resultPixels) < 0) {
        LOGI("Error al bloquear pixeles del bitmap resultado");
        return nullptr;
    }

    int width = filledImage.cols;
    int height = filledImage.rows;
    uint8_t* srcData = filledImage.data;
    int srcStep = (int)filledImage.step;
    uint32_t* dstData = (uint32_t*)resultPixels;
    int dstStride = width;

    for (int y = 0; y < height; y++) {
        uint8_t* srcRow = srcData + y * srcStep;
        uint32_t* dstRow = dstData + y * dstStride;
        for (int x = 0; x < width; x++) {
            uint8_t val = srcRow[x];
            dstRow[x] = 0xFF000000 | (val << 16) | (val << 8) | val;  // ARGB
        }
    }

    AndroidBitmap_unlockPixels(env, resultBitmap);

    return resultBitmap;
}
