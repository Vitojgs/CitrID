#include <opencv4/opencv2/opencv.hpp>
#include <iostream>

extern "C" {
    #include "lib/vc.h"
}

int main() {
    // 1. Abrir o vídeo
    cv::VideoCapture cap("video.avi");
    if (!cap.isOpened()) {
        std::cerr << "Erro: Não foi possível abrir o vídeo!" << std::endl;
        return -1;
    }

    cv::Mat frame;
    while (cap.read(frame)) {
        // A tua lógica de Pessoa 1 (HSV, Segmentação) virá aqui
        
        cv::imshow("Janela Original (BGR)", frame);
        
        if (cv::waitKey(1) == 27) break; // Sai com a tecla ESC
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}