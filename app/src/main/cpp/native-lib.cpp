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
        d.label = std::string(labelC);

        for (int j = 0; j < 7; ++j) {
            d.huMoments[j] = huElements[j];
        }

        d.signature.assign(sigElements, sigElements + sigLen);

        g_descriptors.push_back(d);

        env->ReleaseStringUTFChars(labelJ, labelC);
        env->ReleaseDoubleArrayElements(huArray, huElements, JNI_ABORT);
        env->ReleaseDoubleArrayElements(sigArray, sigElements, JNI_ABORT);
    }

    LOGI("Se cargaron %d descriptores nativos con firmas", (int)g_descriptors.size());
}

static vector<double> calculateSignatureFromContour(const vector<Point>& contour) {
    Moments contourMoments = moments(contour);
    Point2f centroid(contourMoments.m10 / contourMoments.m00, contourMoments.m01 / contourMoments.m00);

    vector<double> signature;
    for (const Point& pt : contour) {
        double dist = norm(Point2f(pt) - centroid);
        signature.push_back(dist);
    }

    // Normalizar firma dividiendo por el valor máximo
    double maxDist = 0;
    for (double v : signature) {
        if (v > maxDist) maxDist = v;
    }
    if (maxDist > 0) {
        for (double& v : signature) {
            v /= maxDist;
        }
    }

    return signature;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_ups_edu_aplicacionnativa_MainActivity_classifyShapeNative(JNIEnv* env, jobject /* this */, jobject bitmap) {
    AndroidBitmapInfo info;
    void* pixels = nullptr;

    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
        LOGI("Error al obtener info del bitmap");
        return env->NewStringUTF("Error");
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGI("Formato de bitmap no soportado");
        return env->NewStringUTF("Formato no soportado");
    }

    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        LOGI("Error al bloquear pixeles");
        return env->NewStringUTF("Error");
    }

    Mat img(info.height, info.width, CV_8UC4, pixels);

    // Procesamiento imagen: gris, blur, threshold, morph close
    Mat gray, blurred, binary, closed;
    cvtColor(img, gray, COLOR_RGBA2GRAY);
    GaussianBlur(gray, blurred, Size(5,5), 0);
    threshold(blurred, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(7,7));
    morphologyEx(binary, closed, MORPH_CLOSE, kernel, Point(-1,-1), 2);

    AndroidBitmap_unlockPixels(env, bitmap);

    // Encontrar contornos
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;
    findContours(closed, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        LOGI("No se detectaron contornos");
        return env->NewStringUTF("No figura");
    }

    // Contorno más grande
    int largestContourIdx = 0;
    double maxArea = 0;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            largestContourIdx = static_cast<int>(i);
        }
    }

    // Imagen rellena con contorno más grande
    Mat filledImage = Mat::zeros(closed.size(), CV_8UC1);
    drawContours(filledImage, contours, largestContourIdx, Scalar(255), FILLED);

    // Validar relleno
    int whitePixels = countNonZero(filledImage);
    int totalPixels = filledImage.rows * filledImage.cols;
    double whiteRatio = (double)whitePixels / totalPixels;
    LOGI("White pixel ratio: %f", whiteRatio);
    isShapeFilled = (whiteRatio > 0.05);

    // Momentos Hu sobre imagen rellena
    Moments moms = moments(filledImage, true);
    double huMoments[7];
    HuMoments(moms, huMoments);

    // Normalizar momentos Hu
    for (int i = 0; i < 7; i++) {
        huMoments[i] = -1 * copysign(1.0, huMoments[i]) * log10(abs(huMoments[i]) + 1e-30);
    }

    // Calcular firma desde contorno
    vector<double> signatureCalc = calculateSignatureFromContour(contours[largestContourIdx]);

    // Comparar con descriptores
    string bestLabel = "Desconocido";
    double minDist = 1e10;

    for (const Descriptor& d : g_descriptors) {
        double distHu = 0;
        for (int i = 0; i < 7; ++i) {
            double diff = huMoments[i] - d.huMoments[i];
            distHu += diff * diff;
        }
        distHu = sqrt(distHu);

        double distSig = 0;
        size_t len = min(d.signature.size(), signatureCalc.size());
        for (size_t i = 0; i < len; ++i) {
            double diff = signatureCalc[i] - d.signature[i];
            distSig += diff * diff;
        }
        distSig = sqrt(distSig);

        // Distancia total ponderada
        double totalDist = distHu + distSig;

        if (totalDist < minDist) {
            minDist = totalDist;
            bestLabel = d.label;
        }
    }

    LOGI("Etiqueta detectada: %s", bestLabel.c_str());

    return env->NewStringUTF(bestLabel.c_str());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_ups_edu_aplicacionnativa_MainActivity_isShapeFilled(JNIEnv* env, jobject /* this */) {
    return isShapeFilled ? JNI_TRUE : JNI_FALSE;
}
