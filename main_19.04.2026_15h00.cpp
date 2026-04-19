#include <iostream>
#include <string>
#include <chrono>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include <opencv4/opencv2/videoio.hpp>

extern "C" {
    #include "lib/vc.h"
}

// ============================================================
// FUNCOES AUXILIARES
// ============================================================

int vc_bgr_to_rgb(IVC *image) {
    if (image == NULL || image->data == NULL || image->channels != 3) return 0;
    unsigned char *data = image->data;
    int npixels = image->width * image->height;
    unsigned char tmp;
    for (int i = 0; i < npixels; i++) {
        tmp             = data[i * 3];
        data[i * 3]     = data[i * 3 + 2];
        data[i * 3 + 2] = tmp;
    }
    return 1;
}

int vc_rgb_binary_to_gray_binary(IVC *src, IVC *dst) {
    if (src == NULL || dst == NULL || src->channels != 3 || dst->channels != 1) return 0;
    unsigned char *datasrc = src->data;
    unsigned char *datadst = dst->data;
    int npixels = src->width * src->height;

    for (int i = 0; i < npixels; i++) {
        datadst[i] = (datasrc[i * 3] == 255) ? 255 : 0;
    }
    return 1;
}

void vc_timer(void) {
    static bool running = false;
    static std::chrono::steady_clock::time_point previousTime = std::chrono::steady_clock::now();
    if (!running) { running = true; } 
    else {
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - previousTime);
        std::cout << "Tempo decorrido: " << time_span.count() << " segundos" << std::endl;
    }
}

// ============================================================
// MAIN
// ============================================================
int main(void) {
    char videofile[20] = "video.avi";
    cv::VideoCapture capture(videofile);
    if (!capture.isOpened()) return 1;

    int width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    int totalFrames = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);

    // Alocação
    IVC *imageRGB = vc_image_new(width, height, 3, 255);
    IVC *imageHSV = vc_image_new(width, height, 3, 255);
    IVC *imageMask3 = vc_image_new(width, height, 3, 255);
    IVC *imageMask1 = vc_image_new(width, height, 1, 255);
    IVC *imageTmp = vc_image_new(width, height, 1, 255);
    IVC *imageLabels = vc_image_new(width, height, 1, 255);

    cv::namedWindow("VC - VIDEO", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("DEBUG - Mascara", cv::WINDOW_AUTOSIZE);

    cv::Mat frame;
    while (capture.read(frame)) {
        memcpy(imageRGB->data, frame.data, width * height * 3);
        vc_bgr_to_rgb(imageRGB);
        vc_rgb_to_hsv_2(imageRGB, imageHSV);

        // CORREÇÃO: Passagem correta dos buffers
        vc_hsv_segmentation(imageHSV, imageMask3, 10, 44, 150, 255, 50, 255);
        vc_rgb_binary_to_gray_binary(imageMask3, imageMask1);

        vc_binary_open(imageMask1, imageTmp, 7);
        vc_binary_close(imageTmp, imageMask1, 15);

        int nlabels = 0;
        OVC *blobs = vc_binary_blob_labelling(imageMask1, imageLabels, &nlabels);
        if (blobs != NULL && nlabels > 0) {
            vc_binary_blob_info(imageLabels, blobs, nlabels);
            
            for (int i = 0; i < nlabels; i++) {
                if (blobs[i].area < 3000) continue;
                
                // Desenho Overlay
                cv::rectangle(frame, cv::Point(blobs[i].x, blobs[i].y), 
                              cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height), 
                              cv::Scalar(0, 165, 255), 2);
            }
            free(blobs);
        }

        cv::imshow("VC - VIDEO", frame);
        cv::imshow("DEBUG - Mascara", cv::Mat(height, width, CV_8UC1, imageMask1->data));
        if (cv::waitKey(1) == 'q') break;
    }

    // Limpeza
    vc_image_free(imageRGB); vc_image_free(imageHSV);
    vc_image_free(imageMask3); vc_image_free(imageMask1);
    vc_image_free(imageTmp); vc_image_free(imageLabels);
    capture.release();
    cv::destroyAllWindows();
    return 0;
}