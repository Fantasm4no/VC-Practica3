#include <jni.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <android/bitmap.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <cmath>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "NativeLib", __VA_ARGS__))

using namespace cv;
using namespace std;

struct Descriptor {
    string label;
    double huMoments[7];
    vector<double> signature;
};

static vector<Descriptor> g_descriptors;
static bool isShapeFilled = true;

// Cargar descriptores desde Java
extern "C"
JNIEXPORT void JNICALL
Java_ups_edu_aplicacionnativa_MainActivity_setDescriptorsNative(JNIEnv* env, jobject /* this */,
                                                                jobjectArray labels,
                                                                jobjectArray huMomentsArrays,
                                                                jobjectArray signaturesArrays) {
    g_descriptors.clear();

    jsize n = env->GetArrayLength(labels);

    for (jsize i = 0; i < n; ++i) {
        jstring labelJ = (jstring) env->GetObjectArrayElement(labels, i);
        const char* labelC = env->GetStringUTFChars(labelJ, nullptr);

        jobject huArrayObj = env->GetObjectArrayElement(huMomentsArrays, i);
        jdoubleArray huArray = (jdoubleArray) huArrayObj;
        jdouble* huElements = env->GetDoubleArrayElements(huArray, nullptr);

        jobject sigArrayObj = env->GetObjectArrayElement(signaturesArrays, i);
        jdoubleArray sigArray = (jdoubleArray) sigArrayObj;
        jdouble* sigElements = env->GetDoubleArrayElements(sigArray, nullptr);
        jsize sigLen = env->GetArrayLength(sigArray);

        Descriptor d;
        d.label = string(labelC);

        for (int j = 0; j < 7; ++j) {
            d.huMoments[j] = huElements[j];
        }

        d.signature.assign(sigElements, sigElements + sigLen);

        g_descriptors.push_back(d);

        env->ReleaseStringUTFChars(labelJ, labelC);
        env->ReleaseDoubleArrayElements(huArray, huElements, JNI_ABORT);
        env->ReleaseDoubleArrayElements(sigArray, sigElements, JNI_ABORT);
    }

    LOGI("Loaded %d descriptors", (int)g_descriptors.size());
}

// ✅ Nueva firma basada en pares (x, y) como en consola
static vector<double> calculateSignatureFromContour(const vector<Point>& contour) {
    vector<double> signature;
    for (const Point& pt : contour) {
        signature.push_back(static_cast<double>(pt.x));
        signature.push_back(static_cast<double>(pt.y));
    }
    return signature;
}

// Clasificación de figura desde un Bitmap
extern "C"
JNIEXPORT jstring JNICALL
Java_ups_edu_aplicacionnativa_MainActivity_classifyShapeNative(JNIEnv* env, jobject /* this */, jobject bitmap) {
    AndroidBitmapInfo info;
    void* pixels = nullptr;

    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
        LOGI("Error getting bitmap info");
        return env->NewStringUTF("Error");
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGI("Unsupported bitmap format");
        return env->NewStringUTF("Unsupported format");
    }

    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        LOGI("Error locking pixels");
        return env->NewStringUTF("Error");
    }

    Mat img(info.height, info.width, CV_8UC4, pixels);

    // Procesamiento de imagen
    Mat gray, blurred, binary, closed;
    cvtColor(img, gray, COLOR_RGBA2GRAY);
    GaussianBlur(gray, blurred, Size(5,5), 0);
    threshold(blurred, binary, 0, 255, THRESH_BINARY_INV | THRESH_OTSU);
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(7,7));
    morphologyEx(binary, closed, MORPH_CLOSE, kernel, Point(-1,-1), 2);

    AndroidBitmap_unlockPixels(env, bitmap);

    // Detectar contornos
    vector<vector<Point>> contours;
    findContours(closed, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        LOGI("No contours detected");
        return env->NewStringUTF("No shape");
    }

    // Encontrar el contorno más grande
    int largestContourIdx = 0;
    double maxArea = 0;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            largestContourIdx = static_cast<int>(i);
        }
    }

    // Crear imagen rellena para momentos de Hu
    Mat filledImage = Mat::zeros(closed.size(), CV_8UC1);
    drawContours(filledImage, contours, largestContourIdx, Scalar(255), FILLED);

    // Validar si está cerrada
    int whitePixels = countNonZero(filledImage);
    int totalPixels = filledImage.rows * filledImage.cols;
    double whiteRatio = (double)whitePixels / totalPixels;
    LOGI("White pixel ratio: %f", whiteRatio);
    isShapeFilled = (whiteRatio > 0.05);

    // Calcular momentos Hu
    Moments moms = moments(filledImage, true);
    double huMoments[7];
    HuMoments(moms, huMoments);

    // Log transform para robustez (como en tu código original)
    for (int i = 0; i < 7; i++) {
        huMoments[i] = -1 * copysign(1.0, huMoments[i]) * log10(abs(huMoments[i]) + 1e-30);
    }

    // Firma tipo (x,y)
    vector<double> signature = calculateSignatureFromContour(contours[largestContourIdx]);

    // Comparar con base de datos
    string bestLabel = "Unknown";
    double minDist = 1e10;

    for (const Descriptor& ref : g_descriptors) {
        // Distancia de Hu Moments
        double distHu = 0;
        for (int i = 0; i < 7; ++i) {
            double diff = huMoments[i] - ref.huMoments[i];
            distHu += diff * diff;
        }
        distHu = sqrt(distHu);

        // Distancia de firma
        double distSig = 0;
        size_t len = min(signature.size(), ref.signature.size());
        for (size_t i = 0; i < len; ++i) {
            double diff = signature[i] - ref.signature[i];
            distSig += diff * diff;
        }
        distSig = sqrt(distSig);

        double totalDist = distHu + distSig;

        if (totalDist < minDist) {
            minDist = totalDist;
            bestLabel = ref.label;
        }
    }

    LOGI("Detected label: %s", bestLabel.c_str());
    return env->NewStringUTF(bestLabel.c_str());
}

// Devolver si está cerrada la figura
extern "C"
JNIEXPORT jboolean JNICALL
Java_ups_edu_aplicacionnativa_MainActivity_isShapeFilled(JNIEnv* env, jobject /* this */) {
    return isShapeFilled ? JNI_TRUE : JNI_FALSE;
}
