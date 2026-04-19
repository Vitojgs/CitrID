#include <opencv4/opencv2/opencv.hpp>
#include <iostream>

extern "C" {
    #include "lib/vc.h" 
}

int main() {
    char videofile[] = "video.avi";
    cv::VideoCapture capture(videofile);

    if (!capture.isOpened()) {
        std::cerr << "Erro: Nao foi possivel abrir o video " << videofile << std::endl;
        return 1;
    }

    int width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    cv::Mat frame, frameRGB;
    
    cv::namedWindow("0. Video Original (Sem Filtros)", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("1. Frame em RGB (Para o IVC)", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("2. Mascara Binaria Final (Pessoa 1)", cv::WINDOW_AUTOSIZE);

    while (true) {
        capture.read(frame);
        if (frame.empty()) break;

        // Converter para RGB para a biblioteca IVC
        cv::cvtColor(frame, frameRGB, cv::COLOR_BGR2RGB);

        IVC *src = vc_image_new(width, height, 3, 255);
        IVC *hsv = vc_image_new(width, height, 3, 255);
        IVC *seg = vc_image_new(width, height, 3, 255);
        IVC *bin = vc_image_new(width, height, 1, 255);

        if (src && hsv && seg && bin) {
            memcpy(src->data, frameRGB.data, width * height * 3);

            // Processamento HSV e Segmentação
            vc_rgb_to_hsv_2(src, hsv);
            // H: 0-40, S: 120-255, V: 100-255
            vc_hsv_segmentation(hsv, seg, 0, 35, 180, 255, 60, 255);

            // Conversão para 1 canal e Limpeza Morfológica
            for (int i = 0; i < width * height; i++) {
                bin->data[i] = seg->data[i * 3]; 
            }
            vc_binary_open(bin, bin, 11);
            vc_binary_close(bin, bin, 15);

            // Visualização
            cv::imshow("0. Video Original (Sem Filtros)", frame);
            cv::imshow("1. Frame em RGB (Para o IVC)", frameRGB);
            
            cv::Mat resultado(height, width, CV_8UC1, bin->data);
            cv::imshow("2. Mascara Binaria Final (Pessoa 1)", resultado);
        }

        // Limpeza de memória
        vc_image_free(src);
        vc_image_free(hsv);
        vc_image_free(seg);
        vc_image_free(bin);

        if (cv::waitKey(1) == 'q') break;
    }

    capture.release();
    cv::destroyAllWindows();
    return 0;
}